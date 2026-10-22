#include "WolfScene.h"
#include "WolfArt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_W   128
#define GROUND_Y   58      // row the paws stand on
#define MAX_CW     64
#define MAX_CH     48

// ------------------------------------------------------------ small bitmaps --
static const uint8_t bone_bits[]  = { 0xC1, 0x80, 0xFF, 0x80, 0xFF, 0x80, 0xC1, 0x80 };   // 9 x 4
static const uint8_t heart_bits[] = { 0x6C, 0xFE, 0xFE, 0x7C, 0x38, 0x10 };               // 7 x 6

// ------------------------------------------------------------- sprite maker --
enum Region : uint8_t { R_NONE = 0, R_BODY, R_LEG, R_HEAD, R_MUZZLE, R_TAILTIP, R_CHEST, R_SADDLE, R_EAR, R_TAIL };
enum Kind   : uint8_t { K_BODY, K_LEG, K_HEAD_SIDE, K_HEAD_FRONT, K_EAR, K_TAIL };

static uint8_t g_reg[MAX_CW * MAX_CH];
static uint8_t g_lit[MAX_CW * MAX_CH];
static uint8_t g_bits[((MAX_CW + 7) / 8) * MAX_CH];

static void stamp(const WolfPart& p, int cw, bool mirror, Kind kind) {
  int stride = (p.w + 7) / 8;
  for (int by = 0; by < p.h; by++) {
    for (int bx = 0; bx < p.w; bx++) {
      if (!(p.bits[by * stride + (bx >> 3)] & (0x80 >> (bx & 7)))) continue;
      int cx = p.x + bx, cy = p.y + by;
      if (mirror) cx = cw - 1 - cx;
      if (cx < 0 || cx >= cw || cy < 0 || cy >= MAX_CH) continue;
      uint8_t r = R_BODY;
      switch (kind) {
        case K_BODY:
          r = (bx * 100 >= p.w * 68) ? R_CHEST : ((by * 100 <= p.h * 35) ? R_SADDLE : R_BODY);
          break;
        case K_LEG:  r = R_LEG; break;
        case K_HEAD_SIDE:
          r = (bx * 100 >= p.w * 60 && by * 100 >= p.h * 35) ? R_MUZZLE : R_HEAD;
          break;
        case K_HEAD_FRONT:
          r = (by * 100 >= p.h * 55 && bx * 100 >= p.w * 25 && bx * 100 <= p.w * 75) ? R_MUZZLE : R_HEAD;
          break;
        case K_EAR:  r = R_EAR; break;
        case K_TAIL: r = (bx * 100 <= p.w * 30) ? R_TAILTIP : R_TAIL; break;
      }
      g_reg[cy * cw + cx] = r;
    }
  }
}

static inline bool inside(int x, int y, int cw, int ch) {
  return x >= 0 && x < cw && y >= 0 && y < ch && g_reg[y * cw + x] != R_NONE;
}

static inline bool pattern(uint8_t level, int x, int y) {
  switch (level) {
    case 0: return false;
    case 1: return ((x & 1) == 0) && ((y & 1) == 0);
    case 2: return ((x + y) & 1) == 0;
    default: return !((x & 1) && (y & 1));
  }
}

static uint8_t levelFor(uint8_t region, const WolfTraits& t) {
  uint8_t base = t.shade;
  switch (region) {
    case R_LEG:     return t.marking == 1 ? 3 : base;
    case R_MUZZLE:  return t.marking == 2 ? 0 : base;
    case R_CHEST:   return t.marking == 3 ? 3 : base;
    case R_SADDLE:  return t.marking == 4 ? (base >= 2 ? 0 : 3) : base;
    case R_TAILTIP: return t.tailtip == 1 ? 3 : (t.tailtip == 2 ? 0 : base);
    case R_EAR:     return base >= 2 ? base - 1 : base;
    default:        return base;
  }
}

static void xorRect(int x, int y, int w, int h, int cw, int ch, bool mirror) {
  for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) {
    int px = mirror ? cw - 1 - (x + i) : x + i;
    int py = y + j;
    if (px >= 0 && px < cw && py >= 0 && py < ch) g_lit[py * cw + px] ^= 1;
  }
}

int wolfDrawSprite(DisplayDriver& d, uint32_t genome, uint8_t stage, int x, int ground_y, const WolfPose& pose) {
  if (stage > 2) stage = 2;
  const WolfStageArt& A = *wolf_art_stages[stage];
  const int cw = A.cw, ch = A.ch;
  WolfTraits t;
  WolfGame::traitsOf(genome, t);

  memset(g_reg, 0, cw * ch);
  memset(g_lit, 0, cw * ch);

  uint8_t hp = pose.head > 3 ? 0 : pose.head;
  bool front = hp >= 2;
  stamp(A.tail[t.tail][pose.tail & 1], cw, pose.mirror, K_TAIL);
  stamp(A.legs[pose.legs > 4 ? 0 : pose.legs], cw, pose.mirror, K_LEG);
  stamp(A.body, cw, pose.mirror, K_BODY);
  stamp(A.head[hp], cw, pose.mirror, front ? K_HEAD_FRONT : K_HEAD_SIDE);
  stamp(A.ear[t.ear][hp], cw, pose.mirror, K_EAR);

  // coat fill + outline
  for (int y = 0; y < ch; y++) {
    for (int px = 0; px < cw; px++) {
      uint8_t r = g_reg[y * cw + px];
      if (r == R_NONE) continue;
      bool edge = !inside(px - 1, y, cw, ch) || !inside(px + 1, y, cw, ch) ||
                  !inside(px, y - 1, cw, ch) || !inside(px, y + 1, cw, ch);
      g_lit[y * cw + px] = edge || pattern(levelFor(r, t), px, y);
    }
  }

  // fluffy ruff: little tufts just outside the silhouette
  if (t.fluff) {
    for (int y = 0; y < ch; y++) for (int px = 0; px < cw; px++) {
      if (g_reg[y * cw + px] != R_NONE || g_lit[y * cw + px]) continue;
      static const int8_t nb[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};
      for (int k = 0; k < 4; k++) {
        int nx = px + nb[k][0], ny = y + nb[k][1];
        if (!inside(nx, ny, cw, ch)) continue;
        uint8_t r = g_reg[ny * cw + nx];
        if (r == R_BODY || r == R_CHEST || r == R_TAIL || r == R_SADDLE) {
          uint32_t h = (uint32_t)(px * 73856093u) ^ (uint32_t)(y * 19349663u) ^ (genome * 2654435761u);
          if (((h >> 9) % 4) == 0) g_lit[y * cw + px] = 1;
        }
        break;
      }
    }
  }

  // spots
  if (t.marking == 5) {
    int bx0 = pose.mirror ? cw - (A.body.x + A.body.w) : A.body.x;
    uint32_t s = genome * 1664525u + 1013904223u + t.spotseed;
    for (int n = 0; n < 6; n++) {
      s = s * 1664525u + 1013904223u;
      int sx = bx0 + 2 + (int)((s >> 8) % (uint32_t)(A.body.w > 5 ? A.body.w - 4 : 1));
      s = s * 1664525u + 1013904223u;
      int sy = A.body.y + 2 + (int)((s >> 8) % (uint32_t)(A.body.h > 5 ? A.body.h - 4 : 1));
      for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) {
        int px = sx + i, py = sy + j;
        if (!inside(px, py, cw, ch)) continue;
        uint8_t r = g_reg[py * cw + px];
        if (r == R_BODY || r == R_SADDLE || r == R_CHEST) g_lit[py * cw + px] ^= 1;
      }
    }
  }

  // face details (XOR so they show on any coat)
  const WolfKey& K = A.key[hp];
  static const uint8_t ew[3] = {2, 2, 1}, eh[3] = {2, 1, 2};
  xorRect(K.eye1x, K.eye1y + (t.eye == 1 ? 1 : 0), ew[t.eye], eh[t.eye], cw, ch, pose.mirror);
  if (K.eye2x >= 0)
    xorRect(K.eye2x, K.eye2y + (t.eye == 1 ? 1 : 0), ew[t.eye], eh[t.eye], cw, ch, pose.mirror);
  xorRect(K.nosex - 1, K.nosey - 1, 2, 2, cw, ch, pose.mirror);
  if (!front) {
    // side view: slanted brow over the eye and a mouth line running back from the muzzle tip
    xorRect(K.eye1x - 1, K.eye1y - 1, 1, 1, cw, ch, pose.mirror);
    xorRect(K.nosex - 7, K.nosey + 1, 5, 1, cw, ch, pose.mirror);
  } else {
    // front view: brows slanting in towards the nose
    xorRect(K.eye1x - 1, K.eye1y - 2, 1, 1, cw, ch, pose.mirror);
    if (K.eye2x >= 0) xorRect(K.eye2x + 2, K.eye2y - 2, 1, 1, cw, ch, pose.mirror);
  }
  if (hp == 3 && K.mw > 0) xorRect(K.mx, K.my, K.mw, K.mh, cw, ch, pose.mirror);

  // pack and draw
  int stride = (cw + 7) / 8;
  memset(g_bits, 0, stride * ch);
  for (int y = 0; y < ch; y++) for (int px = 0; px < cw; px++)
    if (g_lit[y * cw + px]) g_bits[y * stride + (px >> 3)] |= 0x80 >> (px & 7);
  d.drawXbm(x, ground_y - (ch - 1), g_bits, cw, ch);
  return cw;
}

// -------------------------------------------------------------------- scene --
// "trade places" game for familiar friends: hold, run across to each other's side, hold, run back
static int swapX(uint32_t t, int home, int away, int home_dir, int8_t& dir, bool& moving) {
  uint32_t c = t % 6400;
  bool going = c >= 1600 && c < 3000, coming = c >= 4600 && c < 6000, swapped = c >= 3000 && c < 4600;
  float s = going ? (c - 1600) / 1400.0f : (coming ? 1.0f - (c - 4600) / 1400.0f : (swapped ? 1.0f : 0.0f));
  moving = going || coming;
  int d = away > home ? 1 : -1;
  dir = moving ? (going ? d : -d) : (int8_t)(swapped ? -home_dir : home_dir);
  float e = s * s * (3.0f - 2.0f * s);
  return home + (int)((away - home) * e + (away > home ? 0.5f : -0.5f));
}

WolfScene::WolfScene(WolfGame* game) : g(game), _init(false), _last_ms(0), _rng(0x9E3779B9u),
    _shown_idx(0), _shown_genome(0), _vphase(V_NONE), _vseen_id(0), _play_t0(0), _play_level(0), _eat_start(0) {
  memset(&_w, 0, sizeof(_w));
  memset(&_v, 0, sizeof(_v));
  memset(&_treat, 0, sizeof(_treat));
  _cp0[0] = _cp0[1] = 0;
}

uint32_t WolfScene::rnd(uint32_t lo, uint32_t hi) {
  _rng ^= _rng << 13; _rng ^= _rng >> 17; _rng ^= _rng << 5;
  return hi <= lo ? lo : lo + _rng % (hi - lo + 1);
}

void WolfScene::startMode(uint8_t m, uint32_t now, uint32_t dur_ms) {
  _w.mode = m; _w.t0 = now; _w.until = now + dur_ms; _w.frame_ms = 0;
}

void WolfScene::walkStep(Actor& a, uint32_t dt, int speed, int cw) {
  a.x16 += (int32_t)a.dir * speed * 16 * (int32_t)dt / 1000;
  int32_t maxx = (SCREEN_W - cw) * 16;
  if (a.x16 < 0)    { a.x16 = 0;    a.dir = 1; }
  if (a.x16 > maxx) { a.x16 = maxx; a.dir = -1; }
  a.frame_ms += dt;
  if (a.frame_ms >= 125) { a.frame_ms = 0; a.legf = (a.legf + 1) & 3; }
  a.moving = true;
}

// where a wolf stands while the two of them are together; the visitor chooses its side
int WolfScene::restX(bool resident, int rw, int vw) const {
  int ed = g->visitor.enter_dir;      // +1: visitor came in from the left edge
  if (resident) return ed > 0 ? SCREEN_W - rw - 2 : 2;
  return ed > 0 ? 2 : SCREEN_W - vw - 2;
}

void WolfScene::update(uint32_t now, uint32_t dt, uint8_t stage, int rw, int vw) {
  Actor& a = _w;
  a.moving = false;
  const int speed = stage == STAGE_PUP ? 24 : (stage == STAGE_JUVENILE ? 21 : 18);

  // falling treat
  if (_treat.active && !_treat.landed) {
    _treat.y16 += 75 * 16 * (int32_t)dt / 1000;
    if (_treat.y16 >= (GROUND_Y - 3) * 16) { _treat.y16 = (GROUND_Y - 3) * 16; _treat.landed = true; }
  }

  // tail wag
  uint32_t wag = (a.mode == M_HAPPY || a.mode == M_GREET || a.mode == M_RETURN) ? 110 : 520;
  a.tail_ms += dt;
  if (a.tail_ms >= wag) { a.tail_ms = 0; a.tailf ^= 1; }

  // our wolf is going out visiting / coming home: always through a screen edge
  const WolfAway& aw = g->away[g->current];
  if (aw.away && a.mode != M_LEAVE && a.mode != M_AWAY) {
    a.dir = aw.leave_dir ? aw.leave_dir : 1;
    startMode(M_LEAVE, now, 0);
  } else if (!aw.away && (a.mode == M_LEAVE || a.mode == M_AWAY)) {
    a.dir = aw.return_dir ? aw.return_dir : 1;      // +1: walks in from the left edge
    a.x16 = (a.dir > 0 ? -rw : SCREEN_W) * 16;
    startMode(M_RETURN, now, 0);
  }

  // a guest takes priority over free roaming
  if (a.mode != M_LEAVE && a.mode != M_AWAY && a.mode != M_RETURN) {
    if (_vphase != V_NONE && a.mode != M_GREET) startMode(M_GREET, now, 0);
    if (_vphase == V_NONE && a.mode == M_GREET) { a.dir = 1; startMode(M_IDLE, now, 600); }
  }

  if (a.mode == M_IDLE || a.mode == M_WALK) {
    if (g->bark_pending) {
      g->bark_pending = false;
      startMode(M_BARK, now, 2900);
    } else if (_treat.active && _treat.landed) {
      startMode(M_SEEK, now, 0);
    } else if (!_treat.active && g->treat_pending) {
      g->treat_pending--;
      _treat.active = true; _treat.landed = false;
      _treat.x16 = (int32_t)rnd(12, 116) * 16;
      _treat.y16 = 10 * 16;
    }
  }

  switch (a.mode) {
    case M_IDLE:
      a.legf = 0;
      if (now >= a.until) {
        int x = a.x16 / 16;
        a.dir = rnd(0, 1) ? 1 : -1;
        if (x < 10) a.dir = 1;
        if (x > SCREEN_W - rw - 10) a.dir = -1;
        startMode(M_WALK, now, rnd(1800, 4500));
      }
      break;
    case M_WALK:
      walkStep(a, dt, speed, rw);
      if (now >= a.until) startMode(M_IDLE, now, rnd(900, 3000));
      break;
    case M_SEEK: {
      if (!_treat.active) { startMode(M_IDLE, now, 500); break; }
      int tx = _treat.x16 / 16 + 4;
      int mouth = a.dir > 0 ? a.x16 / 16 + rw - 3 : a.x16 / 16 + 2;
      if (abs(mouth - tx) <= 3) { _eat_start = now; startMode(M_EAT, now, 3200); break; }
      a.dir = tx > mouth ? 1 : -1;
      walkStep(a, dt, speed + 4, rw);
      break;
    }
    case M_EAT:
      a.legf = 0;
      if (_treat.active && now - a.t0 > 1900) _treat.active = false;
      if (now >= a.until) { _treat.active = false; startMode(M_HAPPY, now, 1500); }
      break;
    case M_HAPPY:
      a.legf = 0;
      if (now >= a.until) startMode(M_IDLE, now, rnd(800, 2000));
      break;
    case M_BARK:
      a.legf = 0;
      if (now >= a.until) startMode(M_IDLE, now, 800);
      break;
    case M_GREET: {
      int target = restX(true, rw, vw);
      bool playing = _play_t0 != 0;
      if (playing && _play_level >= 2) {            // familiar friend: the two trade places
        uint32_t t = now - _play_t0;
        int other_side = g->visitor.enter_dir > 0 ? 2 : SCREEN_W - rw - 2;
        bool mv; int8_t dr;
        a.x16 = swapX(t, target, other_side, -g->visitor.enter_dir, dr, mv) * 16;
        a.dir = dr;
        a.moving = mv;
        a.legf = (uint8_t)((t / 110) & 3);
      } else {
        int x = a.x16 / 16;
        if (abs(x - target) > 1) {
          a.dir = target > x ? 1 : -1;
          walkStep(a, dt, speed + 4, rw);
        } else {
          a.x16 = target * 16;
          if (_vphase != V_NONE) a.dir = -g->visitor.enter_dir;   // look at the guest
          a.legf = 0;
        }
      }
      break;
    }
    case M_LEAVE:
      a.x16 += (int32_t)a.dir * (speed + 8) * 16 * (int32_t)dt / 1000;
      a.frame_ms += dt;
      if (a.frame_ms >= 110) { a.frame_ms = 0; a.legf = (a.legf + 1) & 3; }
      a.moving = true;
      if (a.dir > 0 ? a.x16 / 16 >= SCREEN_W : a.x16 / 16 <= -rw) startMode(M_AWAY, now, 0);
      break;
    case M_AWAY:
      break;
    case M_RETURN:
      a.x16 += (int32_t)a.dir * (speed + 8) * 16 * (int32_t)dt / 1000;
      a.frame_ms += dt;
      if (a.frame_ms >= 110) { a.frame_ms = 0; a.legf = (a.legf + 1) & 3; }
      a.moving = true;
      if (a.dir > 0 ? a.x16 / 16 >= 6 : a.x16 / 16 <= SCREEN_W - rw - 6) startMode(M_HAPPY, now, 1800);
      break;
  }
}

void WolfScene::updateVisitor(uint32_t now, uint32_t dt, int rw, int vw) {
  const WolfVisitor& v = g->visitor;
  const int speed = 20;
  if (v.active && _vphase == V_NONE && v.id != _vseen_id) {       // a new guest arrives
    _vseen_id = v.id;
    _vphase = V_ENTER;
    _v.dir = v.enter_dir ? v.enter_dir : -1;
    _v.x16 = (_v.dir > 0 ? -vw : SCREEN_W) * 16;
    _v.legf = 0; _v.frame_ms = 0; _v.tailf = 0; _v.tail_ms = 0;
    _play_level = (v.family || v.visits >= 3) ? 2 : (v.visits == 2 ? 1 : 0);
    _play_t0 = 0;
  }
  if (!v.active && (_vphase == V_ENTER || _vphase == V_STAY)) { _vphase = V_LEAVE; _v.dir = v.exit_dir ? v.exit_dir : 1; }

  _v.moving = false;
  _v.tail_ms += dt;
  if (_v.tail_ms >= 130) { _v.tail_ms = 0; _v.tailf ^= 1; }

  int vspot = restX(false, rw, vw);
  bool resident_ready = _w.mode == M_GREET && abs(_w.x16 / 16 - restX(true, rw, vw)) <= 1;

  switch (_vphase) {
    case V_ENTER:
      _v.x16 += (int32_t)_v.dir * speed * 16 * (int32_t)dt / 1000;
      _v.frame_ms += dt;
      if (_v.frame_ms >= 125) { _v.frame_ms = 0; _v.legf = (_v.legf + 1) & 3; }
      _v.moving = true;
      if (_v.dir > 0 ? _v.x16 / 16 >= vspot : _v.x16 / 16 <= vspot) { _v.x16 = vspot * 16; _vphase = V_STAY; }
      break;
    case V_STAY: {
      if ((int32_t)(now + 4000 - v.until_ms) >= 0) { _vphase = V_LEAVE; _v.dir = v.exit_dir ? v.exit_dir : 1; _play_t0 = 0; break; }
      if (_play_t0 == 0 && resident_ready && _v.x16 / 16 == vspot) {      // both in place: start playing
        _play_t0 = now ? now : 1;
      }
      if (_play_t0 && _play_level >= 2) {
        uint32_t t = now - _play_t0;
        int other_side = v.enter_dir > 0 ? SCREEN_W - vw - 2 : 2;
        bool mv; int8_t dr;
        _v.x16 = swapX(t, vspot, other_side, v.enter_dir ? v.enter_dir : -1, dr, mv) * 16;
        _v.dir = dr;
        _v.moving = mv;
        _v.legf = (uint8_t)((t / 110) & 3);
      } else {
        _v.dir = v.enter_dir ? v.enter_dir : -1;
        _v.legf = 0;
      }
      break;
    }
    case V_LEAVE:
      _play_t0 = 0;
      _v.x16 += (int32_t)_v.dir * speed * 16 * (int32_t)dt / 1000;
      _v.frame_ms += dt;
      if (_v.frame_ms >= 125) { _v.frame_ms = 0; _v.legf = (_v.legf + 1) & 3; }
      _v.moving = true;
      if (_v.dir > 0 ? _v.x16 / 16 >= SCREEN_W : _v.x16 / 16 <= -vw) _vphase = V_NONE;
      break;
    default: break;
  }
}

void WolfScene::drawHud(DisplayDriver& d, const Wolf& w, uint16_t batt_mv) {
  d.setTextSize(1);
  d.setColor(UIColor::primary_txt);
  char tmp[24];
  if (g->count > 1)
    snprintf(tmp, sizeof(tmp), "%s %umo %u/%u", w.sex ? "F" : "M", (unsigned)WolfGame::ageMonths(w),
             (unsigned)(g->current + 1), (unsigned)g->count);
  else
    snprintf(tmp, sizeof(tmp), "%s %umo", w.sex ? "F" : "M", (unsigned)WolfGame::ageMonths(w));
  d.setCursor(0, 0);
  d.print(tmp);
  int tw = d.getTextWidth(tmp);
  if (w.preg_secs_left) d.drawXbm(tw + 5, 1, heart_bits, 7, 6);

  // battery
#ifndef BATT_MIN_MILLIVOLTS
  #define BATT_MIN_MILLIVOLTS 3000
#endif
#ifndef BATT_MAX_MILLIVOLTS
  #define BATT_MAX_MILLIVOLTS 4200
#endif
  int pct = ((int)batt_mv - BATT_MIN_MILLIVOLTS) * 100 / (BATT_MAX_MILLIVOLTS - BATT_MIN_MILLIVOLTS);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  int bx = SCREEN_W - 16, by = 1;
  d.drawRect(bx, by, 12, 7);
  d.fillRect(bx + 12, by + 2, 2, 3);
  d.fillRect(bx + 2, by + 2, (pct * 8) / 100, 3);
}

int WolfScene::render(DisplayDriver& d, uint32_t now, uint16_t batt_mv) {
  if (!g->count) return 500;
  const Wolf& w = g->pack[g->current];
  uint8_t stage = WolfGame::stageOf(w);
  const WolfStageArt& A = *wolf_art_stages[stage];
  int rw = A.cw;

  if (!_init || g->current != _shown_idx || w.genome != _shown_genome) {
    _rng ^= w.genome + now;
    if (!_rng) _rng = 1;
    _shown_idx = g->current;
    _shown_genome = w.genome;
    memset(&_w, 0, sizeof(_w));
    _w.x16 = (int32_t)rnd(10, SCREEN_W - rw - 10) * 16;
    _w.dir = rnd(0, 1) ? 1 : -1;
    if (g->away[g->current].away) startMode(M_AWAY, now, 0);
    else startMode(M_IDLE, now, 700);
    _init = true;
    _last_ms = now;
  }

  uint32_t dt = now - _last_ms;
  if (dt > 250) dt = 250;
  _last_ms = now;

  int vw = 0;
  uint8_t vstage = STAGE_ADULT;
  if (g->visitor.active || _vphase != V_NONE) {
    vstage = WolfGame::stageFromMonths(g->visitor.months);
    vw = wolf_art_stages[vstage]->cw;
  }

  if (vw) updateVisitor(now, dt, rw, vw);
  update(now, dt, stage, rw, vw);

  d.setColor(UIColor::primary_txt);

  // ground: a dotted line
  for (int x = 0; x < SCREEN_W; x += 2) d.fillRect(x, GROUND_Y + 1, 1, 1);

  // treat
  if (_treat.active) d.drawXbm(_treat.x16 / 16, _treat.y16 / 16, bone_bits, 9, 4);

  // play: hops and play-bows once two wolves know each other
  int hop_r = 0, hop_v = 0;
  bool bow = false;
  bool playing = _play_t0 != 0 && _vphase == V_STAY;
  if (playing && _play_level >= 1) {
    uint32_t t = now - _play_t0;
    int q = (int)((t % 600) * 1024 / 600);
    hop_r = (q < 512 ? q : 1023 - q) * 4 / 512;
    int q2 = (int)(((t + 300) % 600) * 1024 / 600);
    hop_v = (q2 < 512 ? q2 : 1023 - q2) * 4 / 512;
    if (_play_level == 1) bow = (t % 3400) < 900;
    if (_play_level >= 2) { hop_r /= 2; hop_v /= 2; }
  }

  // visitor (drawn first so the resident is on top)
  if (_vphase != V_NONE && vw) {
    WolfPose vp;
    vp.head = bow ? 1 : 0;
    vp.legs = (_v.moving && !bow) ? (uint8_t)(1 + _v.legf) : 0;
    vp.tail = (playing || _vphase != V_STAY) ? _v.tailf : _v.tailf;
    vp.mirror = _v.dir < 0;
    wolfDrawSprite(d, g->visitor.genome, vstage, _v.x16 / 16, GROUND_Y - (bow ? 0 : hop_v), vp);
  }

  // resident
  bool woof = false;
  if (_w.mode != M_AWAY) {
    WolfPose p;
    p.mirror = _w.dir < 0;
    p.tail = _w.tailf;
    p.legs = 0;
    p.head = 0;
    switch (_w.mode) {
      case M_WALK: case M_SEEK: case M_LEAVE: case M_RETURN: p.legs = 1 + _w.legf; break;
      case M_GREET: p.legs = (_w.moving && !bow) ? 1 + _w.legf : 0; p.head = bow ? 1 : 0; break;
      case M_EAT:  p.head = (((now - _w.t0) / 230) & 1) ? 0 : 1; break;
      case M_BARK: {
        uint32_t e = now - _w.t0;
        if (e < 500) p.head = 2;
        else if (e < 2100) { bool open = (((e - 500) / 260) & 1) == 0; p.head = open ? 3 : 2; woof = open; }
        else p.head = 2;
        break;
      }
      default: break;
    }
    wolfDrawSprite(d, w.genome, stage, _w.x16 / 16, GROUND_Y - ((_w.mode == M_GREET && !bow) ? hop_r : 0), p);
  } else {
    d.drawTextCentered(SCREEN_W / 2, 30, "off visiting...");
  }

  if (woof) {
    int hx = _w.x16 / 16 + (_w.dir > 0 ? (rw * 3) / 4 : rw / 4);
    if (hx < 20) hx = 20;
    if (hx > SCREEN_W - 20) hx = SCREEN_W - 20;
    int ty = GROUND_Y - A.ch - 9;
    if (ty < 10) ty = 10;
    d.drawTextCentered(hx, ty, "WOOF!");
  }

  // hearts after a mating
  if ((int32_t)(g->hearts_until_ms - now) > 0) {
    for (int i = 0; i < 3; i++) {
      int tri = (int)(((now / 110) + i * 3) % 12);
      if (tri > 6) tri = 12 - tri;
      d.drawXbm(52 + i * 12, 14 + tri, heart_bits, 7, 6);
    }
  }

  drawHud(d, w, batt_mv);
  return 110;   // ~9 frames per second
}
