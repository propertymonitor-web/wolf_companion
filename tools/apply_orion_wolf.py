#!/usr/bin/env python3
"""Add the wolf pet to a MeshCore tree that LODESTAR NOVA/ORION has already been applied to.

usage: apply_orion_wolf.py --meshcore /path/to/MeshCore

Run tools/apply_nova.py (from the ORION repo) first.  This script copies src/Wolf*.{h,cpp} into
examples/companion_radio/ui-new/ and edits UITask.cpp / UITask.h with anchored substitutions.
Every anchor must match exactly once, so a MeshCore change that moves things fails loudly
instead of producing a quietly wrong build.
"""
import argparse, glob, os, shutil, sys

ap = argparse.ArgumentParser()
ap.add_argument("--meshcore", required=True)
a = ap.parse_args()
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ui = os.path.join(a.meshcore, "examples", "companion_radio", "ui-new")
if not os.path.isdir(ui):
    sys.exit("not a MeshCore checkout (missing %s)" % ui)
if not os.path.exists(os.path.join(ui, "NovaPages.h")):
    sys.exit("apply NOVA/ORION first (NovaPages.h is missing)")

for f in sorted(glob.glob(os.path.join(root, "src", "Wolf*")) + [os.path.join(root, "src", "PetSpecies.h"), os.path.join(root, "src", "PetMirror.h")]):
    shutil.copy(f, ui)
    print("copied", os.path.basename(f))


def patch(path, edits):
    s = open(path).read()
    for name, old, new in edits:
        n = s.count(old)
        if n != 1:
            sys.exit("anchor '%s' matched %d times in %s (need exactly 1)" % (name, n, os.path.basename(path)))
        s = s.replace(old, new)
        print("  +", name)
    open(path, "w").write(s)


print("UITask.cpp")
patch(os.path.join(ui, "UITask.cpp"), [
    ("include", '#include "icons.h"\n', '#include "icons.h"\n#include "WolfModule.h"\n'),
    ("enum WOLF", "  enum HomePage {\n    FIRST,",
     "  enum HomePage {\n    WOLF,     // the wolf pet: first screen after the splash, never times out\n    FIRST,"),
    ("onWolfPage", "  HomeScreen(UITask* task, mesh::RTCClock* rtc,",
     "  bool onWolfPage() const { return _page == HomePage::WOLF; }\n\n  HomeScreen(UITask* task, mesh::RTCClock* rtc,"),
    ("render", "  int render(DisplayDriver& display) override {\n    display.setColor(UIColor::title_bkg);\n    display.fillRect(0, 0, display.width(), 12);",
     "  int render(DisplayDriver& display) override {\n    if (_page == HomePage::WOLF) return _task->renderWolf(display);   // full-screen wolf, no title bar\n    display.setColor(UIColor::title_bkg);\n    display.fillRect(0, 0, display.width(), 12);"),
    ("long press", "    if (c == KEY_ENTER && _page == HomePage::BLUETOOTH) {",
     "    if (c == KEY_ENTER && _page == HomePage::WOLF) {   // long press: look at the next wolf\n      _task->wolfNextWolf();\n      return true;\n    }\n    if (c == KEY_ENTER && _page == HomePage::BLUETOOTH) {"),
    ("begin", "  msg_preview = new MsgPreviewScreen(this, &rtc_clock);\n  setCurrScreen(splash);",
     "  msg_preview = new MsgPreviewScreen(this, &rtc_clock);\n  wolf = new WolfModule();\n  wolf->begin(this);\n  setCurrScreen(splash);"),
    ("onMessage", "  if (!(txt_type == TXT_TYPE_PLAIN || txt_type == TXT_TYPE_SIGNED_PLAIN)) return;\n",
     "  if (!(txt_type == TXT_TYPE_PLAIN || txt_type == TXT_TYPE_SIGNED_PLAIN)) return;\n  if (wolf) wolf->onMessage();\n"),
    ("onChannelMessage", "void UITask::onChannelMessageRecv(mesh::Packet *pkt, ChannelDetails& channel_details, const char* text) {\n",
     "void UITask::onChannelMessageRecv(mesh::Packet *pkt, ChannelDetails& channel_details, const char* text) {\n  if (wolf) wolf->onMessage();\n"),
    ("onControl", "void UITask::onControlDataRecv(const mesh::Packet* packet) {\n",
     "void UITask::onControlDataRecv(const mesh::Packet* packet) {\n  if (wolf) wolf->onControl(packet->payload, packet->payload_len);\n"),
    ("shutdown", "void UITask::shutdown(bool restart){\n", "void UITask::shutdown(bool restart){\n  if (wolf) wolf->save();\n"),
    ("loop", "void UITask::loop() {\n  char c = 0;\n", "void UITask::loop() {\n  char c = 0;\n  if (wolf) wolf->loop();\n"),
    ("splash next", "    if (millis() >= dismiss_after) {\n      _task->gotoHomeScreen();",
     "    if (millis() >= dismiss_after) {\n      _task->gotoAfterSplash();"),
    ("inspired screen", "class HomeScreen : public UIScreen {",
     '''#if PET_SPLASH
// "INSPIRED BY" card, shown for 5 seconds after the ORION splash (alligator and crocodile only)
class InspiredScreen : public UIScreen {
  UITask* _task;
  unsigned long _until;
public:
  InspiredScreen(UITask* task) : _task(task) { _until = millis() + 5000; }
  int render(DisplayDriver& d) override {
    d.setColor(UIColor::primary_txt);
    d.setTextSize(1);
    int w = d.width();
    d.fillRect(16, 14, w - 32, 1);
    d.drawTextCentered(w / 2, 22, "INSPIRED BY:");
    d.drawTextCentered(w / 2, 36, "PAT STEINHAUER-SCATES");
    d.fillRect(16, 50, w - 32, 1);
    return 1000;
  }
  void poll() override {
    if ((long)(millis() - _until) >= 0) _task->gotoHomeScreen();
  }
};
#endif

class HomeScreen : public UIScreen {'''),
    ("showWolf", "  bool onWolfPage() const { return _page == HomePage::WOLF; }\n",
     "  bool onWolfPage() const { return _page == HomePage::WOLF; }\n  void showWolf() { _page = HomePage::WOLF; }\n"),
    ("triple click", '  MESH_DEBUG_PRINTLN("UITask: triple click triggered");\n  checkDisplayOn(c);\n  toggleBuzzer();\n',
     '''  MESH_DEBUG_PRINTLN("UITask: triple click triggered");
  checkDisplayOn(c);
  if (!wolfPageActive()) {      // a quick triple click from any other screen goes straight back to the pet
    gotoWolfScreen();
    return 0;
  }
  toggleBuzzer();
'''),
    ("auto off", "    if (millis() > _auto_off) {\n      _display->turnOff();",
     "    if (millis() > _auto_off && !wolfPageActive()) {   // the wolf screen never times out\n      _display->turnOff();"),
    ("double click", '  MESH_DEBUG_PRINTLN("UITask: double-click triggered");\n  checkDisplayOn(c);\n  return c;\n}\n',
     '''  MESH_DEBUG_PRINTLN("UITask: double-click triggered");
  if (_display != NULL && _display->isOn() && wolfPageActive()) {
    _display->turnOff();     // on the wolf screen, a double click turns the screen off
    return 0;
  }
  checkDisplayOn(c);
  return c;
}

bool UITask::wolfPageActive() const {
  return curr != NULL && curr == home && ((HomeScreen *) home)->onWolfPage();
}

int UITask::renderWolf(DisplayDriver& display) {
  return wolf ? wolf->render(display, getBattMilliVolts()) : 1000;
}

void UITask::gotoAfterSplash() {
#if PET_SPLASH
  setCurrScreen(new InspiredScreen(this));
#else
  gotoHomeScreen();
#endif
}

void UITask::gotoWolfScreen() {
  if (home == NULL) return;
  ((HomeScreen *) home)->showWolf();
  setCurrScreen(home);
  _next_refresh = 0;
}

void UITask::wolfNextWolf() {
  if (!wolf) return;
  char desc[32];
  wolf->nextWolf(desc, sizeof(desc));
  showAlert(desc, 1800);
  _next_refresh = 0;
}
'''),
])

print("UITask.h")
patch(os.path.join(ui, "UITask.h"), [
    ("include", '#include "../NodePrefs.h"\n', '#include "../NodePrefs.h"\n#include "WolfModule.h"\n'),
    ("member", "  UIScreen* curr;\n", "  UIScreen* curr;\n  WolfModule* wolf;\n"),
    ("init", "    curr = NULL;\n", "    curr = NULL;\n    wolf = NULL;\n"),
    ("methods", "  bool isButtonPressed() const;\n",
     "  bool isButtonPressed() const;\n\n  // wolf pet\n  bool wolfPageActive() const;           // true while the wolf screen is showing\n  int  renderWolf(DisplayDriver& display);\n  void wolfNextWolf();\n  void gotoAfterSplash();\n  void gotoWolfScreen();\n"),
])
print("MyMesh.cpp (dashboard: watch the pet on a big screen)")
patch(os.path.join(a.meshcore, "examples", "companion_radio", "MyMesh.cpp"), [
    ("pet decl", "void MyMesh::novaTick(uint32_t nv_now) {\n",
     "size_t wolfMirrorLine(uint32_t now, char* out, size_t cap, uint16_t batt_mv);   // WolfModule.cpp\n\nvoid MyMesh::novaTick(uint32_t nv_now) {\n"),
    ("pet state", "  static uint32_t nv_next_json = 0;\n",
     "  static uint32_t nv_next_json = 0;\n  static uint32_t nv_pet_until = 0, nv_pet_next = 0;   // the pet picture stream, while a page asks for it\n"),
    ("pet command", "    if (strncmp(nv_line + 4, \" json\", 5) == 0)",
     "    if (strncmp(nv_line + 4, \" pet\", 4) == 0 && (nv_line[8] == 0 || nv_line[8] == ' ')) { nv_pet_until = nv_now + 4000u; continue; }\n    if (strncmp(nv_line + 4, \" json\", 5) == 0)"),
    ("pet stream", "  static bool nv_was_on = false;\n",
     """  if ((int32_t)(nv_pet_until - nv_now) > 0 && (int32_t)(nv_now - nv_pet_next) >= 0) {   // eight pictures a second
    nv_pet_next = nv_now + 125u;
    static char nv_pet[2200];
    size_t nv_pn = wolfMirrorLine(nv_now, nv_pet, sizeof(nv_pet), nv_mv);
    if (nv_pn > 0) {
      Serial.write((const uint8_t*)nv_pet, nv_pn);
      Serial.write('\\n');
    }
  }
  static bool nv_was_on = false;
"""),
])

print("wolf added")
