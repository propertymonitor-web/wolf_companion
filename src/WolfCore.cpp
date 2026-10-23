#include "WolfCore.h"
#include <stdio.h>

#define WOLF_FILE_VER 2
#define WOLF_STATE_MAX 640

static inline uint32_t nib(uint32_t g, int i) { return (g >> (4 * i)) & 0xF; }
static inline bool due(uint32_t now, uint32_t t) { return (int32_t)(now - t) >= 0; }

WolfGame::WolfGame(WolfHost* host) : count(0), current(0), treat_pending(0), bark_pending(false),
    hearts_until_ms(0), _h(host), _last_ms(0), _ms_acc(0), _secs_since_save(0),
    _treat_cnt(0), _treat_target(10), _bark_cnt(0), _bark_target(10), _next_trip_ms(0), _visit_seq(0),
    _trip_idx(0), _dirty(false) {
  memset(pack, 0, sizeof(pack));
  memset(away, 0, sizeof(away));
  memset(&visitor, 0, sizeof(visitor));
  memset(mem, 0, sizeof(mem));
  memset(_seen, 0, sizeof(_seen));
  memset(&_trip, 0, sizeof(_trip));
  memset(_out, 0, sizeof(_out));
}

uint32_t WolfGame::rnd(uint32_t lo, uint32_t hi) {
  if (hi <= lo) return lo;
  return lo + (_h->random32() % (hi - lo + 1));
}

void WolfGame::rollTreat() { _treat_target = rnd(WOLF_TREAT_MIN_MSGS, WOLF_TREAT_MAX_MSGS); }
void WolfGame::rollBark()  { _bark_target  = rnd(WOLF_BARK_MIN_MSGS,  WOLF_BARK_MAX_MSGS); }

// ---------------------------------------------------------------- genetics --
void WolfGame::traitsOf(uint32_t g, WolfTraits& t) {
  uint32_t v;
  v = nib(g, 0); t.shade   = v <= 3 ? 0 : v <= 8 ? 1 : v <= 12 ? 2 : 3;
  v = nib(g, 1); t.ear     = v <= 5 ? 0 : v <= 10 ? 1 : 2;
  v = nib(g, 2); t.tail    = v <= 6 ? 0 : v <= 11 ? 1 : 2;
  v = nib(g, 3); t.marking = v <= 5 ? 0 : v <= 7 ? 1 : v <= 9 ? 2 : v <= 11 ? 3 : v <= 13 ? 4 : 5;
  v = nib(g, 4); t.tailtip = v <= 8 ? 0 : v <= 12 ? 1 : 2;
  v = nib(g, 5); t.fluff   = v >= 9 ? 1 : 0;
  v = nib(g, 6); t.eye     = v <= 7 ? 0 : v <= 11 ? 1 : 2;
  v = nib(g, 7); t.spotseed = (uint8_t)v;
}

uint8_t WolfGame::stageFromMonths(uint32_t m) {
  return m < WOLF_PUP_MONTHS ? STAGE_PUP : (m < WOLF_ADULT_MONTHS ? STAGE_JUVENILE : STAGE_ADULT);
}
uint8_t WolfGame::stageOf(const Wolf& w) { return stageFromMonths(ageMonths(w)); }

// Each of the 8 genes is inherited from the mother or the father (coin flip);
// roughly 1 gene in 12 mutates to a brand new value.
uint32_t WolfGame::mixGenome(uint32_t mother, uint32_t father) {
  uint32_t child = 0;
  for (int i = 0; i < 8; i++) {
    uint32_t n = (rnd(0, 1) ? nib(mother, i) : nib(father, i));
    if (rnd(0, 11) == 0) n = rnd(0, 15);
    child |= n << (4 * i);
  }
  return child;
}

// parents/children and siblings never mate
bool WolfGame::related(uint32_t g1, uint32_t s1, uint32_t d1, uint32_t g2, uint32_t s2, uint32_t d2) {
  if (s1 == g2 || d1 == g2 || s2 == g1 || d2 == g1) return true;
  if (s1 && s1 == s2) return true;
  if (d1 && d1 == d2) return true;
  return false;
}

// ------------------------------------------------------------ visit memory ---
uint8_t WolfGame::pairVisits(uint32_t mine, uint32_t other) const {
  for (int i = 0; i < WOLF_MEM_SIZE; i++)
    if (mem[i].visits && mem[i].mine == mine && mem[i].other == other) return mem[i].visits;
  return 0;
}

uint8_t WolfGame::bumpVisits(uint32_t mine, uint32_t other) {
  int free_slot = -1, weakest = 0;
  for (int i = 0; i < WOLF_MEM_SIZE; i++) {
    if (mem[i].visits && mem[i].mine == mine && mem[i].other == other) {
      if (mem[i].visits < 250) mem[i].visits++;
      _dirty = true;
      return mem[i].visits;
    }
    if (!mem[i].visits) { if (free_slot < 0) free_slot = i; }
    else if (mem[i].visits < mem[weakest].visits || !mem[weakest].visits) weakest = i;
  }
  int s = free_slot >= 0 ? free_slot : weakest;
  mem[s].mine = mine; mem[s].other = other; mem[s].visits = 1;
  _dirty = true;
  return 1;
}

bool WolfGame::consents(const Wolf& w, uint32_t other) const {
  return pairVisits(w.genome, other) >= w.visits_needed;
}

// ---------------------------------------------------------------- lifecycle --
void WolfGame::createFounder() {
  memset(pack, 0, sizeof(pack));
  Wolf& w = pack[0];
  w.genome = _h->identityTail();
  w.sex = (uint8_t)rnd(0, 1);
  w.founder = 1;
  w.visits_needed = rollNeed();
  count = 1;
  current = 0;
}

void WolfGame::begin(uint32_t now_ms) {
  _last_ms = now_ms;
  _ms_acc = 0;
  uint8_t buf[WOLF_STATE_MAX];
  int len = 0;
  bool ok = _h->loadState(buf, sizeof(buf), len) && deserialize(buf, len);
  if (!ok) {
    createFounder();
    save();
  }
  rollTreat();
  rollBark();
  _trip_idx = (uint8_t)rnd(0, 250);
  _next_trip_ms = now_ms + rnd(WOLF_HELLO_FIRST_MIN_S, WOLF_HELLO_FIRST_MAX_S) * 1000UL;
}

void WolfGame::save() {
  uint8_t buf[WOLF_STATE_MAX];
  int n = serialize(buf, sizeof(buf));
  if (n > 0) _h->saveState(buf, n);
  _secs_since_save = 0;
  _dirty = false;
}

void WolfGame::ageOneSecond() {
  uint8_t n = count;
  for (uint8_t i = 0; i < n; i++) {
    Wolf& w = pack[i];
    w.age_secs++;
    if (w.cooldown_secs) w.cooldown_secs--;
    if (w.preg_secs_left) {
      if (--w.preg_secs_left == 0) giveBirth(w);
    }
  }
  if (++_secs_since_save >= WOLF_SAVE_EVERY_S) save();
}

void WolfGame::queue(const uint8_t* pkt, int len, uint32_t due_ms) {
  if (len > (int)sizeof(_out[0].pkt)) return;
  for (int i = 0; i < 4; i++) {
    if (!_out[i].used) {
      memcpy(_out[i].pkt, pkt, len);
      _out[i].len = len; _out[i].due_ms = due_ms; _out[i].used = true;
      return;
    }
  }
}

void WolfGame::tick(uint32_t now) {
  uint32_t dt = now - _last_ms;
  _last_ms = now;
  if (dt > 60000) dt = 60000;  // guard against odd jumps
  _ms_acc += dt;
  while (_ms_acc >= 1000) {
    _ms_acc -= 1000;
    ageOneSecond();
  }

  // time for one of our wolves to go and visit somebody?
  if (due(now, _next_trip_ms)) {
    bool sent = (!_trip.active && !visitor.active) && sendRequest(now);
    _next_trip_ms = now + (sent ? rnd(WOLF_HELLO_MIN_S, WOLF_HELLO_MAX_S) * 1000UL : 60000UL);
  }

  // our wolf is visiting another radio
  if (_trip.active) {
    if (!_trip.confirmed) {
      if ((uint32_t)(now - _trip.request_ms) > WOLF_REQUEST_WAIT_MS) _trip.active = false;  // nobody home
    } else {
      if (due(now, _trip.next_ping_ms)) { sendPing(_trip.host, now); _trip.next_ping_ms = now + WOLF_PING_MS; }
      if (due(now, _trip.until_ms) || (uint32_t)(now - _trip.last_heard_ms) > WOLF_PEER_LOST_MS) endTrip(now);
    }
  }

  // another radio's wolf is visiting us
  if (visitor.active) {
    if (due(now, visitor.next_ping_ms)) { sendPing(visitor.owner, now); visitor.next_ping_ms = now + WOLF_PING_MS; }
    if ((uint32_t)(now - visitor.last_heard_ms) > WOLF_PEER_LOST_MS && !due(now, visitor.until_ms))
      visitor.until_ms = now;                      // lost contact: the guest heads home
    if (due(now, visitor.until_ms)) { visitor.active = false; visitor.family = false; }
  }

  // outgoing packets (small random delays keep radios from talking over each other)
  for (int i = 0; i < 4; i++) {
    if (_out[i].used && due(now, _out[i].due_ms)) {
      _out[i].used = false;
      _h->broadcast(_out[i].pkt, _out[i].len);
    }
  }
  if (_dirty) save();
}

// ------------------------------------------------------------------- pack ---
bool WolfGame::addWolf(const Wolf& w) {
  if (count < WOLF_MAX_PACK) {
    pack[count++] = w;
    return true;
  }
  // full: an older, non-founder adult goes to a new home to make room
  int victim = -1;
  for (int i = 0; i < count; i++) {
    if (pack[i].founder || !isAdult(pack[i]) || pack[i].preg_secs_left || away[i].away) continue;
    if (victim < 0 || pack[i].age_secs > pack[victim].age_secs) victim = i;
  }
  if (victim < 0) return false;
  pack[victim] = w;
  return true;
}

void WolfGame::makePregnant(Wolf& m, uint32_t father) {
  m.preg_secs_left = WOLF_GESTATION_MONTHS * WOLF_SECS_PER_MONTH;
  m.father_genome = father;
  m.litter = (uint8_t)rnd(1, 3);
  hearts_until_ms = _last_ms + 8000;
  _h->alert("Wolves mated!");
  _dirty = true;
}

void WolfGame::giveBirth(Wolf& mother) {
  int first_new = -1;
  uint8_t n = mother.litter ? mother.litter : 1;
  uint32_t father = mother.father_genome, mg = mother.genome;
  mother.preg_secs_left = 0;
  mother.litter = 0;
  mother.cooldown_secs = WOLF_COOLDOWN_MONTHS * WOLF_SECS_PER_MONTH;
  for (uint8_t i = 0; i < n; i++) {
    Wolf c;
    memset(&c, 0, sizeof(c));
    c.genome = mixGenome(mg, father);
    c.sex = (uint8_t)rnd(0, 1);
    c.sire = father;
    c.dam = mg;
    c.visits_needed = rollNeed();
    int before = count;
    if (addWolf(c)) {
      mother.pups_born++;
      if (first_new < 0) first_new = (count > before) ? before : -2;
    }
  }
  if (first_new >= 0 && !_trip.active) current = (uint8_t)first_new;
  if (first_new != -1) {
    char msg[24];
    snprintf(msg, sizeof(msg), "Pups born! x%u", (unsigned)n);
    _h->alert(msg);
  }
  _dirty = true;
}

void WolfGame::cycleWolf() {
  if (count) current = (uint8_t)((current + 1) % count);
}

void WolfGame::describe(const Wolf& w, char* out, int outlen) const {
  snprintf(out, outlen, "%s %umo #%08lX", w.sex ? "F" : "M", (unsigned)ageMonths(w),
           (unsigned long)w.genome);
}

// ------------------------------------------------------------------ events --
void WolfGame::onMessage() {
  if (++_treat_cnt >= _treat_target) {
    _treat_cnt = 0;
    rollTreat();
    if (treat_pending < 3) treat_pending++;
  }
  if (++_bark_cnt >= _bark_target) {
    _bark_cnt = 0;
    rollBark();
    bark_pending = true;
  }
}

// ---------------------------------------------------------------- networking --
void WolfGame::sendPing(const uint8_t* target, uint32_t now) {
  uint8_t p[10];
  p[0] = WOLF_PKT_TYPE | WOLF_PKT_PING;
  p[1] = WOLF_PKT_VERSION;
  memcpy(&p[2], target, 4);
  _h->ownerPrefix(&p[6]);
  queue(p, sizeof(p), now + rnd(50, 400));
}

void WolfGame::sendBred(const uint8_t* target, uint32_t father, uint32_t mother) {
  uint8_t b[19];
  b[0] = WOLF_PKT_TYPE | WOLF_PKT_BRED;
  b[1] = WOLF_PKT_VERSION;
  memcpy(&b[2], target, 4);
  _h->ownerPrefix(&b[6]);
  memcpy(&b[10], &father, 4);
  memcpy(&b[14], &mother, 4);
  b[18] = WOLF_MATE_EXTEND_SECS;
  queue(b, sizeof(b), _last_ms + rnd(1400, 2200));   // after the ACK has gone out
}

// One of our wolves asks to visit whichever radio can hear it.
bool WolfGame::sendRequest(uint32_t now) {
  int idx = -1;
  for (int k = 0; k < count; k++) {
    int i = (_trip_idx + k) % count;
    if (away[i].away || pack[i].preg_secs_left || hidden(pack[i])) continue;   // pregnant mothers (and chrysalises) stay home
    idx = i;
    break;
  }
  if (idx < 0) return false;
  _trip_idx = (uint8_t)(idx + 1);
  const Wolf& w = pack[idx];

  uint8_t p[40];
  p[0] = WOLF_PKT_TYPE | WOLF_PKT_VISIT;
  p[1] = WOLF_PKT_VERSION;
  _h->ownerPrefix(&p[2]);
  memcpy(&p[6], &w.genome, 4);
  p[10] = (uint8_t)((w.sex ? 1 : 0) | (isAdult(w) ? 2 : 0) | (w.preg_secs_left ? 4 : 0));
  uint32_t m = ageMonths(w);
  if (m > 0xFFFF) m = 0xFFFF;
  p[11] = (uint8_t)(m & 0xFF);
  p[12] = (uint8_t)(m >> 8);
  memcpy(&p[13], &w.sire, 4);
  memcpy(&p[17], &w.dam, 4);
  // the wolves this one has met often enough to consent to (strongest first, at most 3)
  uint8_t n = 0;
  bool taken[WOLF_MEM_SIZE] = {false};
  while (n < 3) {
    int best = -1;
    for (int i = 0; i < WOLF_MEM_SIZE; i++) {
      if (taken[i] || !mem[i].visits || mem[i].mine != w.genome || mem[i].visits < w.visits_needed) continue;
      if (best < 0 || mem[i].visits > mem[best].visits) best = i;
    }
    if (best < 0) break;
    taken[best] = true;
    memcpy(&p[22 + 4 * n], &mem[best].other, 4);
    n++;
  }
  p[21] = n;

  memset(&_trip, 0, sizeof(_trip));
  _trip.active = true;
  _trip.wolf = (uint8_t)idx;
  _trip.request_ms = now;
  queue(p, 22 + 4 * n, now);
  return true;
}

void WolfGame::endTrip(uint32_t now) {
  if (!_trip.active) return;
  if (_trip.wolf < count) {
    away[_trip.wolf].away = false;
    away[_trip.wolf].return_ms = now;
    away[_trip.wolf].return_dir = rnd(0, 1) ? 1 : -1;   // comes home through either edge
  }
  _trip.active = false;
  _dirty = true;
}

void WolfGame::onPacket(const uint8_t* p, int len, uint32_t now) {
  if (!p || len < 2 || (p[0] & 0xF0) != WOLF_PKT_TYPE || p[1] != WOLF_PKT_VERSION) return;
  switch (p[0] & 0x0F) {
    case WOLF_PKT_VISIT: handleRequest(p, len, now); break;
    case WOLF_PKT_ACK:   handleAck(p, len, now);     break;
    case WOLF_PKT_BRED:  handleBred(p, len, now);    break;
    case WOLF_PKT_PING:  handlePing(p, len, now);    break;
    default: break;
  }
}

// ----- we are the host: somebody's wolf wants to visit us -----
void WolfGame::handleRequest(const uint8_t* p, int len, uint32_t now) {
  if (len < 22) return;
  uint8_t me[4];
  _h->ownerPrefix(me);
  if (memcmp(me, &p[2], 4) == 0) return;      // our own echo
  if (_trip.active || visitor.active || !count) return;   // we're out, or already have a guest

  // a radio we met less than 9 minutes ago doesn't count as a new visit
  int slot = -1, oldest = 0;
  for (int i = 0; i < 8; i++) {
    if (_seen[i].used && memcmp(_seen[i].owner, &p[2], 4) == 0) {
      if ((uint32_t)(now - _seen[i].at_ms) < 540000UL) return;
      slot = i;
      break;
    }
    if (!_seen[i].used) { if (slot < 0) slot = i; }
    else if ((uint32_t)(now - _seen[i].at_ms) > (uint32_t)(now - _seen[oldest].at_ms)) oldest = i;
  }
  if (slot < 0) slot = oldest;
  memcpy(_seen[slot].owner, &p[2], 4);
  _seen[slot].at_ms = now;
  _seen[slot].used = true;

  uint32_t genome, vsire, vdam;
  memcpy(&genome, &p[6], 4);
  memcpy(&vsire, &p[13], 4);
  memcpy(&vdam, &p[17], 4);
  uint8_t flags = p[10];
  bool v_male = !(flags & 1), v_adult = (flags & 2) != 0;
  uint16_t months = (uint16_t)(p[11] | (p[12] << 8));
  uint8_t n = p[21];
  if (len < 22 + 4 * n) n = 0;

  // which of our wolves comes out to meet the guest?
  int gi = -1;
  bool family = false;
  for (int i = 0; i < count; i++) if (pack[i].sire == genome) { gi = i; family = true; break; }   // his pups
  for (int i = 0; i < count && gi < 0; i++) {
    const Wolf& w = pack[i];
    if (v_adult && isAdult(w) && w.sex != (v_male ? 0 : 1) && !w.preg_secs_left &&
        !related(w.genome, w.sire, w.dam, genome, vsire, vdam)) gi = i;      // a possible mate
  }
  if (gi < 0) gi = current < count ? current : 0;
  Wolf& G = pack[gi];
  current = (uint8_t)gi;

  uint8_t visits = bumpVisits(G.genome, genome);
  uint32_t secs = family ? WOLF_FAMILY_VISIT_SECS : WOLF_VISIT_SECS;

  visitor.active = true;
  visitor.genome = genome;
  visitor.sex = (flags & 1) ? 1 : 0;
  visitor.months = months;
  memcpy(visitor.owner, &p[2], 4);
  visitor.until_ms = now + secs * 1000UL;
  visitor.last_heard_ms = now;
  visitor.next_ping_ms = now + WOLF_PING_MS;
  visitor.enter_dir = rnd(0, 1) ? 1 : -1;
  visitor.exit_dir = rnd(0, 1) ? 1 : -1;
  visitor.visits = visits;
  visitor.family = family;
  visitor.greeter = (uint8_t)gi;
  visitor.id = ++_visit_seq;

  // answer: "come in, this wolf will greet you"
  uint8_t a[30];
  a[0] = WOLF_PKT_TYPE | WOLF_PKT_ACK;
  a[1] = WOLF_PKT_VERSION;
  memcpy(&a[2], &p[2], 4);
  memcpy(&a[6], me, 4);
  memcpy(&a[10], &genome, 4);
  memcpy(&a[14], &G.genome, 4);
  a[18] = (uint8_t)((G.sex ? 1 : 0) | (isAdult(G) ? 2 : 0) | (G.preg_secs_left ? 4 : 0) |
                    (consents(G, genome) ? 8 : 0));
  uint32_t gm = ageMonths(G);
  if (gm > 0xFFFF) gm = 0xFFFF;
  a[19] = (uint8_t)(gm & 0xFF);
  a[20] = (uint8_t)(gm >> 8);
  a[21] = (uint8_t)secs;
  memcpy(&a[22], &G.sire, 4);
  memcpy(&a[26], &G.dam, 4);
  queue(a, sizeof(a), now + rnd(200, 1200));

  // courtship: we hold the female.  Needs adult + opposite sex + not related, and BOTH wolves
  // must have reached their own private visit threshold with each other.
  if (G.sex == 1 && v_male && v_adult && isAdult(G) && !G.preg_secs_left && !G.cooldown_secs &&
      !related(G.genome, G.sire, G.dam, genome, vsire, vdam)) {
    bool mine_ok = visits >= G.visits_needed;
    bool his_ok = false;
    for (int i = 0; i < n; i++) {
      uint32_t c;
      memcpy(&c, &p[22 + 4 * i], 4);
      if (c == G.genome) his_ok = true;
    }
    if (mine_ok && his_ok && rnd(1, 100) <= WOLF_MATE_CHANCE_PCT) {
      makePregnant(G, genome);
      visitor.until_ms += WOLF_MATE_EXTEND_SECS * 1000UL;
      sendBred(visitor.owner, genome, G.genome);
    }
  }
  _dirty = true;
}

// ----- we are the visitor: the host accepted our wolf -----
void WolfGame::handleAck(const uint8_t* p, int len, uint32_t now) {
  if (len < 30 || !_trip.active || _trip.confirmed) return;
  uint8_t me[4];
  _h->ownerPrefix(me);
  if (memcmp(me, &p[2], 4) != 0) return;
  Wolf& V = pack[_trip.wolf];
  uint32_t echo, greeter, gsire, gdam;
  memcpy(&echo, &p[10], 4);
  if (echo != V.genome) return;
  memcpy(&greeter, &p[14], 4);
  memcpy(&gsire, &p[22], 4);
  memcpy(&gdam, &p[26], 4);
  uint8_t flags = p[18];
  uint32_t secs = p[21];

  _trip.confirmed = true;
  memcpy(_trip.host, &p[6], 4);
  _trip.greeter = greeter;
  _trip.last_heard_ms = now;
  _trip.next_ping_ms = now + WOLF_PING_MS;
  _trip.until_ms = now + secs * 1000UL + _trip.extend_ms;

  uint8_t visits = bumpVisits(V.genome, greeter);
  away[_trip.wolf].away = true;
  away[_trip.wolf].leave_ms = now;
  away[_trip.wolf].leave_dir = rnd(0, 1) ? 1 : -1;       // walks off through either edge
  current = _trip.wolf;                                   // so you can watch it go
  sendPing(_trip.host, now);                              // "I'm coming" -- other hosts just time out

  bool g_male = !(flags & 1), g_adult = (flags & 2) != 0;
  if (V.sex == 1 && g_male && g_adult && isAdult(V) && !V.preg_secs_left && !V.cooldown_secs &&
      !related(V.genome, V.sire, V.dam, greeter, gsire, gdam)) {
    bool mine_ok = visits >= V.visits_needed;
    bool his_ok = (flags & 8) != 0;
    if (mine_ok && his_ok && rnd(1, 100) <= WOLF_MATE_CHANCE_PCT) {
      makePregnant(V, greeter);
      _trip.until_ms += WOLF_MATE_EXTEND_SECS * 1000UL;
      sendBred(_trip.host, greeter, V.genome);
    }
  }
  _dirty = true;
}

void WolfGame::handlePing(const uint8_t* p, int len, uint32_t now) {
  if (len < 10) return;
  uint8_t me[4];
  _h->ownerPrefix(me);
  if (memcmp(me, &p[2], 4) != 0) return;
  if (_trip.active && _trip.confirmed && memcmp(_trip.host, &p[6], 4) == 0) _trip.last_heard_ms = now;
  if (visitor.active && memcmp(visitor.owner, &p[6], 4) == 0) visitor.last_heard_ms = now;
}

void WolfGame::handleBred(const uint8_t* p, int len, uint32_t now) {
  if (len < 19) return;
  uint8_t me[4];
  _h->ownerPrefix(me);
  if (memcmp(me, &p[2], 4) != 0) return;   // meant for someone else
  uint32_t father, mother;
  memcpy(&father, &p[10], 4);
  memcpy(&mother, &p[14], 4);
  (void)mother;
  uint32_t ext = (uint32_t)p[18] * 1000UL;

  if (_trip.active && _trip.wolf < count && pack[_trip.wolf].genome == father) {
    // our wolf is the visiting father
    pack[_trip.wolf].pups_sired++;
    _trip.extend_ms += ext;
    if (_trip.confirmed) {
      _trip.until_ms += ext;
      _trip.last_heard_ms = now;
    }
    _h->alert("Your wolf sired a litter!");
    _dirty = true;
  } else if (visitor.active && memcmp(visitor.owner, &p[6], 4) == 0 && visitor.greeter < count &&
             pack[visitor.greeter].genome == father) {
    // we host the father and the visiting female chose him
    pack[visitor.greeter].pups_sired++;
    visitor.until_ms += ext;
    visitor.last_heard_ms = now;
    hearts_until_ms = now + 8000;
    _h->alert("Your wolf sired a litter!");
    _dirty = true;
  }
}

// ------------------------------------------------------------ persistence ----
static void put32(uint8_t*& p, uint32_t v) { memcpy(p, &v, 4); p += 4; }
static void put16(uint8_t*& p, uint16_t v) { memcpy(p, &v, 2); p += 2; }
static uint32_t get32(const uint8_t*& p) { uint32_t v; memcpy(&v, p, 4); p += 4; return v; }
static uint16_t get16(const uint8_t*& p) { uint16_t v; memcpy(&v, p, 2); p += 2; return v; }

int WolfGame::serialize(uint8_t* buf, int maxlen) const {
  uint8_t nmem = 0;
  for (int i = 0; i < WOLF_MEM_SIZE; i++) if (mem[i].visits) nmem++;
  int need = 6 + count * 36 + 1 + nmem * 9;
  if (maxlen < need) return -1;
  uint8_t* p = buf;
  *p++ = 'W'; *p++ = 'L'; *p++ = 'F'; *p++ = WOLF_FILE_VER;
  *p++ = count; *p++ = current;
  for (int i = 0; i < count; i++) {
    const Wolf& w = pack[i];
    put32(p, w.genome); put32(p, w.age_secs);
    *p++ = w.sex; *p++ = w.founder; *p++ = w.litter; *p++ = w.visits_needed;
    put32(p, w.preg_secs_left); put32(p, w.father_genome); put32(p, w.cooldown_secs);
    put32(p, w.sire); put32(p, w.dam);
    put16(p, w.pups_born); put16(p, w.pups_sired);
  }
  *p++ = nmem;
  for (int i = 0; i < WOLF_MEM_SIZE; i++) {
    if (!mem[i].visits) continue;
    put32(p, mem[i].mine); put32(p, mem[i].other); *p++ = mem[i].visits;
  }
  return (int)(p - buf);
}

bool WolfGame::deserialize(const uint8_t* buf, int len) {
  if (len < 7 || buf[0] != 'W' || buf[1] != 'L' || buf[2] != 'F' || buf[3] != WOLF_FILE_VER) return false;
  uint8_t n = buf[4], cur = buf[5];
  if (n == 0 || n > WOLF_MAX_PACK || len < 6 + n * 36 + 1) return false;
  const uint8_t* p = buf + 6;
  Wolf tmp[WOLF_MAX_PACK];
  memset(tmp, 0, sizeof(tmp));
  for (int i = 0; i < n; i++) {
    Wolf& w = tmp[i];
    w.genome = get32(p); w.age_secs = get32(p);
    w.sex = *p++ & 1; w.founder = *p++; w.litter = *p++; w.visits_needed = *p++;
    if (w.visits_needed < WOLF_CONSENT_MIN || w.visits_needed > WOLF_CONSENT_MAX) w.visits_needed = WOLF_CONSENT_MAX;
    w.preg_secs_left = get32(p); w.father_genome = get32(p); w.cooldown_secs = get32(p);
    w.sire = get32(p); w.dam = get32(p);
    w.pups_born = get16(p); w.pups_sired = get16(p);
  }
  uint8_t nmem = *p++;
  if (nmem > WOLF_MEM_SIZE || len < (int)(p - buf) + nmem * 9) return false;
  WolfMemory tm[WOLF_MEM_SIZE];
  memset(tm, 0, sizeof(tm));
  for (int i = 0; i < nmem; i++) {
    tm[i].mine = get32(p); tm[i].other = get32(p); tm[i].visits = *p++;
  }
  memcpy(pack, tmp, sizeof(pack));
  memcpy(mem, tm, sizeof(mem));
  memset(away, 0, sizeof(away));
  count = n;
  current = cur < n ? cur : 0;
  return true;
}
