#include "WolfModule.h"
#include "PetMirror.h"
#include "UITask.h"
#include "../MyMesh.h"
#include "../DataStore.h"
#include "target.h"

extern DataStore store;   // defined in main.cpp

#define WOLF_STATE_FILE PET_STATE_FILE
#define WOLF_BACKUP_FILE PET_STATE_FILE ".bak"

// ---- platform glue ---------------------------------------------------------------------------
static File wolfOpenWrite(FILESYSTEM* fs, const char* filename) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  fs->remove(filename);
  return fs->open(filename, FILE_O_WRITE);
#elif defined(RP2040_PLATFORM)
  return fs->open(filename, "w");
#else
  return fs->open(filename, "w", true);
#endif
}

struct WolfModule::Host : public WolfHost {
  UITask* ui;
  explicit Host(UITask* u) : ui(u) {}

  uint32_t random32() override {
    uint32_t v = 0;
    the_mesh.getRNG()->random((uint8_t*)&v, 4);
    return v;
  }

  // The last 8 hex characters of the radio's public key = its last 4 bytes.
  uint32_t identityTail() override {
    const uint8_t* k = the_mesh.self_id.pub_key;
    return ((uint32_t)k[PUB_KEY_SIZE - 4] << 24) | ((uint32_t)k[PUB_KEY_SIZE - 3] << 16) |
           ((uint32_t)k[PUB_KEY_SIZE - 2] << 8) | (uint32_t)k[PUB_KEY_SIZE - 1];
  }

  void ownerPrefix(uint8_t out[4]) override { memcpy(out, the_mesh.self_id.pub_key, 4); }

  bool loadState(uint8_t* buf, int maxlen, int& len) override {
    FILESYSTEM* fs = store.getPrimaryFS();
    if (!fs || !fs->exists(WOLF_STATE_FILE)) return false;
    File f = store.openRead(WOLF_STATE_FILE);
    if (!f) return false;
    int n = (int)f.read(buf, maxlen);
    f.close();
    if (n <= 0) return false;
    len = n;
    return true;
  }

  // Two copies, written one after the other: if the power dies in the middle of a write, the other copy is intact.
  void saveState(const uint8_t* buf, int len) override {
    FILESYSTEM* fs = store.getPrimaryFS();
    if (!fs) return;
    const char* names[2] = { WOLF_STATE_FILE, WOLF_BACKUP_FILE };
    for (int i = 0; i < 2; i++) {
      File f = wolfOpenWrite(fs, names[i]);
      if (!f) continue;
      f.write(buf, len);
      f.close();
    }
  }

  bool loadBackup(uint8_t* buf, int maxlen, int& len) override {
    FILESYSTEM* fs = store.getPrimaryFS();
    if (!fs || !fs->exists(WOLF_BACKUP_FILE)) return false;
    File f = store.openRead(WOLF_BACKUP_FILE);
    if (!f) return false;
    int n = (int)f.read(buf, maxlen);
    f.close();
    if (n <= 0) return false;
    len = n;
    return true;
  }

  void broadcast(const uint8_t* buf, int len) override {
    mesh::Packet* pkt = the_mesh.createControlData(buf, len);
    if (pkt) the_mesh.sendZeroHop(pkt);
  }

  void alert(const char* text) override { ui->showAlert(text, 2500); }
};

// ---- module ------------------------------------------------------------------------------------
static WolfModule* g_wolf_instance = NULL;
struct MirrorState { PetMirror disp; PetMirrorEncoder enc; };

// called from the dashboard link in MyMesh.cpp (see apply_orion_wolf.py)
bool wolfSetPetName(const char* text, char* reply, int reply_len) {
  if (!g_wolf_instance) { snprintf(reply, reply_len, "Err - no pet on this radio"); return false; }
  return g_wolf_instance->setPetName(text, reply, reply_len);
}
size_t wolfMirrorLine(uint32_t now, char* out, size_t cap, uint16_t batt_mv) {
  return g_wolf_instance ? g_wolf_instance->mirrorLine(now, out, cap, batt_mv) : 0;
}

WolfModule::WolfModule() : _host(NULL), _game(NULL), _scene(NULL), _mirror(NULL) {
#if PET_MYST
  _scene_sp = 0; _scene_head = 0xFF;
#endif
}

WolfModule::~WolfModule() {
  delete _scene;
  delete (MirrorState*)_mirror;
  delete _game;
  delete _host;
}

void WolfModule::begin(UITask* ui) {
  _host = new Host(ui);
  _game = new WolfGame(_host);
  _game->begin(millis());
#if PET_MYST
  _scene_sp = WolfGame::speciesOf(_game->pack[_game->current < _game->count ? _game->current : 0]);
  _scene = makePetScene(_scene_sp, _game);
  _scene_head = WolfGame::headOf(_game->pack[_game->current < _game->count ? _game->current : 0]);
  if (_scene) _scene->setHead(_scene_head);
#else
  _scene = new WolfScene(_game);
#endif
  g_wolf_instance = this;
}

void WolfModule::loop() {
  if (_game) _game->tick(millis());
}

void WolfModule::onMessage() {
  if (_game) _game->onMessage();
}

void WolfModule::onControl(const uint8_t* payload, int len) {
  if (_game) _game->onPacket(payload, len, millis());
}

void WolfModule::syncScene() {
#if PET_MYST
  if (!_game || !_game->count) return;
  uint8_t sp = WolfGame::speciesOf(_game->pack[_game->current < _game->count ? _game->current : 0]);
  if (_scene && sp != _scene_sp) {
    delete _scene;
    _scene = makePetScene(sp, _game);
    _scene_sp = sp;
    _scene_head = 0xFE;   // force head refresh
  }
  uint8_t hd = WolfGame::headOf(_game->pack[_game->current < _game->count ? _game->current : 0]);
  if (_scene && hd != _scene_head) { _scene->setHead(hd); _scene_head = hd; }
#endif
}

int WolfModule::render(DisplayDriver& d, uint16_t batt_mv) {
  syncScene();
  return _scene ? _scene->render(d, millis(), batt_mv) : 1000;
}

void WolfModule::nextWolf(char* desc, int desc_len) {
  if (!_game) return;
  _game->cycleWolf();
  _game->describe(_game->pack[_game->current], desc, desc_len);
}

bool WolfModule::setPetName(const char* text, char* reply, int reply_len) {
  if (!_game || !_game->count) { snprintf(reply, reply_len, "Err - no pet yet"); return false; }
  while (text && *text == ' ') text++;
  if (text && text[0] == '-' && text[1] == 0) text = "";        // "-" clears the name
  _game->setName(_game->current, text);
  const Wolf& w = _game->pack[_game->current];
  if (w.name[0]) snprintf(reply, reply_len, "OK - your pet is called %s", w.name);
  else snprintf(reply, reply_len, "OK - the name is cleared");
  _game->save();
  return true;
}

void WolfModule::save() {
  if (_game) _game->save();
}

size_t WolfModule::mirrorLine(uint32_t now, char* out, size_t cap, uint16_t batt_mv) {
  if (!_scene || !_game || !_game->count) return 0;
  syncScene();
  if (!_mirror) _mirror = new MirrorState();
  MirrorState* m = (MirrorState*)_mirror;
  m->disp.startFrame();
  _scene->render(m->disp, now, batt_mv);
  const Wolf& w = _game->pack[_game->current];
#if PET_MYST
  char nmbuf[16];
  const char* nm = "egg";
  if (!WolfGame::isEgg(w)) {
    WolfGame::titleOf(w, nmbuf, sizeof(nmbuf));
    for (char* c = nmbuf; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32;      // the dashboard writes it with a capital itself
    nm = nmbuf;
  }
#else
  const char* nm = PET_NAME;
#endif
  return m->enc.line(m->disp.fb, now, out, cap, nm, w.sex, (int)WolfGame::ageMonths(w), _game->count, _game->current + 1, w.name);
}
