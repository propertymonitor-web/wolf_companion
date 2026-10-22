#include "WolfScene.h"
#include "WolfArt.h"
#include "WolfProps.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_W   128
#define GROUND_Y   58      // row the paws stand on
#define MAX_CW     64
#define MAX_CH     56

// ------------------------------------------------------------ small bitmaps --
static const uint8_t bone_bits[]  = { 0xC1, 0x80, 0xFF, 0x80, 0xFF, 0x80, 0xC1, 0x80 };   // 9 x 4
static const uint8_t heart_bits[] = { 0x6C, 0xFE, 0xFE, 0x7C, 0x38, 0x10 };               // 7 x 6

// ------------------------------------------------------------- sprite maker --
enum Region : uint8_t { R_NONE = 0, R_BODY, R_LEG, R_HEAD, R_MUZZLE, R_TAILTIP, R_CHEST, R_SADDLE, R_EAR, R_TAIL };
enum Kind   : uint8_t { K_BODY, K_LEG, K_HEAD_SIDE, K_HEAD_FRONT, K_EAR, K_TAIL };

static uint8_t g_reg[MAX_CW * MAX_CH];
static uint8_t g_lit[MAX_CW * MAX_CH];
static uint8_t g_bits[((MAX_CW + 7) / 8) * MAX_CH];
static int g_ch;     // canvas height of the sprite being built

static void stamp(const WolfPart& p, int cw, bool mirror, Kind kind, int dy = 0) {
  int stride = (p.w + 7) / 8;
  for (int by = 0; by < p.h; by++) {
    for (int bx = 0; bx < p.w; bx++) {
      if (!(p.bits[by * stride + (bx >> 3)] & (0x80 >> (bx & 7)))) continue;
      int cx = p.x + bx, cy = p.y + by + dy;
      if (mirror) cx = cw - 1 - cx;
      if (cx < 0 || cx >= cw || cy < 0 || cy >= g_ch) continue;
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

  g_ch = ch;
  uint8_t hp = pose.head > 4 ? 0 : pose.head;
  bool front = hp == 2 || hp == 3;
  // lying down: everything above the legs drops until the belly rests on the ground
  int ldy = pose.lie ? (ch - 1) - (A.body.y + A.body.h - 1) : 0;
  stamp(A.tail[t.tail][pose.tail & 1], cw, pose.mirror, K_TAIL, ldy);
  if (!pose.lie) stamp(A.legs[pose.legs > 4 ? 0 : pose.legs], cw, pose.mirror, K_LEG);
  stamp(A.body, cw, pose.mirror, K_BODY, ldy);
  stamp(A.head[hp], cw, pose.mirror, front ? K_HEAD_FRONT : K_HEAD_SIDE, ldy);
  stamp(A.ear[t.ear][hp], cw, pose.mirror, K_EAR, ldy);
  if (pose.lie) {           // forepaws stretched out in front
    int fx = A.body.x + A.body.w - 2;
    for (int j = ch - 3; j < ch; j++) for (int i = 0; i < 7; i++) {
      int cx = pose.mirror ? cw - 1 - (fx + i) : fx + i;
      if (cx >= 0 && cx < cw) g_reg[j * cw + cx] = R_LEG;
    }
  }

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
      int sy = A.body.y + ldy + 2 + (int)((s >> 8) % (uint32_t)(A.body.h > 5 ? A.body.h - 4 : 1));
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
  const int ei = pose.blink ? 1 : t.eye;          // closed eye = a short line
  const int e1y = K.eye1y + ldy, e2y = K.eye2y + ldy, ny = K.nosey + ldy;
  xorRect(K.eye1x, e1y + (ei == 1 ? 1 : 0), ew[ei], eh[ei], cw, ch, pose.mirror);
  if (K.eye2x >= 0)
    xorRect(K.eye2x, e2y + (ei == 1 ? 1 : 0), ew[ei], eh[ei], cw, ch, pose.mirror);
  xorRect(K.nosex - 1, ny - 1, 2, 2, cw, ch, pose.mirror);
  if (!front) {
    // side view: slanted brow over the eye and a mouth line running back from the muzzle tip
    xorRect(K.eye1x - 1, e1y - 1, 1, 1, cw, ch, pose.mirror);
    xorRect(K.nosex - 7, ny + 1, 5, 1, cw, ch, pose.mirror);
  } else {
    // front view: brows slanting in towards the nose
    xorRect(K.eye1x - 1, e1y - 2, 1, 1, cw, ch, pose.mirror);
    if (K.eye2x >= 0) xorRect(K.eye2x + 2, e2y - 2, 1, 1, cw, ch, pose.mirror);
  }
  if (hp == 3 && K.mw > 0) xorRect(K.mx, K.my + ldy, K.mw, K.mh, cw, ch, pose.mirror);

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
  memset(&_ev, 0, sizeof(_ev));
  memset(&_hole, 0, sizeof(_hole));
  _last_ev = 255; _force = 0; _stage = STAGE_ADULT; _rw = 60;
  _cp0[0] = _cp0[1] = 0;
}

uint32_t WolfScene::rnd(uint32_t lo, uint32_t hi) {
  _rng ^= _rng << 13; _rng ^= _rng >> 17; _rng ^= _rng << 5;
  return hi <= lo ? lo : lo + _rng % (hi - lo + 1);
}

void WolfScene::startMode(uint8_t m, uint32_t now, uint32_t dur_ms) {
  _w.mode = m; _w.t0 = now; _w.until = now + dur_ms; _w.frame_ms = 0;
  _ev.show = false; _ev.phase = 0; _ev.pt0 = now;    // props belong to the activity that made them
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
  uint32_t wag = (a.mode == M_HAPPY || a.mode == M_GREET || a.mode == M_RETURN || a.mode == M_CHASE ||
                  a.mode == M_ZOOM || a.mode == M_DIG || a.mode == M_FLY || a.mode == M_STRETCH) ? 110 : 520;
  if (a.mode == M_SLEEP || a.mode == M_REST) wag = 1500;
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

  const bool in_event = a.mode >= M_BUNNY;
  if (in_event && (g->bark_pending || (_treat.active && _treat.landed))) startMode(M_IDLE, now, 0);   // a bark or a treat wins
  if (a.mode == M_IDLE || a.mode == M_WALK || in_event) {
    if (g->bark_pending && !in_event) {
      g->bark_pending = false;
      startMode(M_BARK, now, 2900);
    } else if (_treat.active && _treat.landed && !in_event) {
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
        if (rnd(0, 99) < 45 && !_treat.active && !g->treat_pending && _vphase == V_NONE) {
          beginEvent(pickEvent(stage), now, stage);
          break;
        }
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
      int wxx = a.x16 / 16;
      int tx = _treat.x16 / 16 + 4;
      if (tx > wxx && tx < wxx + rw) {      // the bone landed under the wolf: toss it just in front of the nose
        int m = a.dir > 0 ? wxx + rw - 3 : wxx + 2;
        tx = m + a.dir * 10;
        if (tx < 6) tx = 6;
        if (tx > SCREEN_W - 6) tx = SCREEN_W - 6;
        _treat.x16 = (int32_t)(tx - 4) * 16;
      }
      a.dir = tx > wxx + rw / 2 ? 1 : -1;   // decide by the wolf's middle, not its nose (the nose jumps sides when it turns)
      int mouth = a.dir > 0 ? wxx + rw - 3 : wxx + 2;
      if (abs(mouth - tx) <= 3) { _eat_start = now; startMode(M_EAT, now, 3200); break; }
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
    default:
      if (a.mode >= M_BUNNY) updateEvent(now, dt, stage, rw);
      break;
  }
}

// ------------------------------------------------------- random activities --
enum { EV_BUNNY, EV_BIRD, EV_FLY, EV_DIG, EV_SLEEP, EV_REST, EV_HOWL, EV_SHAKE, EV_YAWN, EV_CHASE,
       EV_SNIFF, EV_STRETCH, EV_ZOOM };

static inline int arc(uint32_t t, uint32_t period, int height) {     // 0 -> height -> 0 over `period` ms
  if (t >= period) return 0;
  int q = (int)(t * 1000 / period);
  return q * (1000 - q) * height / 250000;
}

uint8_t WolfScene::pickEvent(uint8_t stage) {
  static const uint8_t wt[3][EV_COUNT] = {
    // bunny bird fly dig sleep rest howl shake yawn chase sniff stretch zoom
    {   3,    2,   3,  2,  4,    1,   1,   2,    2,   4,    2,    2,      4 },   // pup
    {   3,    2,   3,  3,  3,    1,   2,   2,    2,   3,    2,    2,      3 },   // adolescent
    {   3,    2,   2,  3,  3,    2,   3,   2,    2,   1,    3,    2,      1 } }; // adult
  if (stage > 2) stage = 2;
  int total = 0;
  for (int i = 0; i < EV_COUNT; i++) if (i != _last_ev) total += wt[stage][i];
  int r = (int)rnd(0, (uint32_t)total - 1);
  for (int i = 0; i < EV_COUNT; i++) {
    if (i == _last_ev) continue;
    if (r < wt[stage][i]) return (uint8_t)i;
    r -= wt[stage][i];
  }
  return EV_SHAKE;
}

void WolfScene::beginEvent(uint8_t kind, uint32_t now, uint8_t stage) {
  (void)stage;
  static const uint16_t dur[EV_COUNT] = { 0, 0, 0, 4300, 0, 0, 3800, 1500, 2300, 3000, 4600, 2500, 3400 };
  _last_ev = kind;
  uint32_t d = dur[kind];
  if (kind == EV_SLEEP) d = rnd(9000, 15000);
  if (kind == EV_REST)  d = rnd(5000, 8000);
  if (!d) d = 60000;                       // phased activities end themselves
  startMode((uint8_t)(M_BUNNY + kind), now, d);
  memset(&_ev, 0, sizeof(_ev));
  _ev.pt0 = now;
  _w.legf = 0; _w.moving = false;
}

void WolfScene::updateEvent(uint32_t now, uint32_t dt, uint8_t stage, int rw) {
  Actor& a = _w;
  const uint32_t e = now - a.t0;
  const int wx = a.x16 / 16;
  const int speed = stage == STAGE_PUP ? 24 : (stage == STAGE_JUVENILE ? 21 : 18);
  bool fixed = true;                      // true: ends when `until` passes

  switch (a.mode) {
    case M_BUNNY: {                       // a rabbit hops in, the wolf creeps over and sniffs it, the rabbit hops away
      fixed = false;
      const int bw = BUNNY_SIT_R_W;
      int bx = _ev.px16 / 16;
      switch (_ev.phase) {
        case 0: {
          int goal = (wx + rw / 2 < SCREEN_W / 2) ? 4 : SCREEN_W - rw - 4;     // stroll to the nearer edge, leaving room
          if (abs(wx - goal) <= 1) {
            bool left = goal < SCREEN_W / 2;
            a.dir = left ? 1 : -1;
            _ev.pdir = left ? -1 : 1;
            _ev.px16 = (left ? SCREEN_W : -bw) * 16;
            _ev.aux = (uint32_t)(left ? goal + rw + 22 : goal - 22 - bw);       // where the rabbit stops
            _ev.phase = 1; _ev.pt0 = now; _ev.show = true;
          } else { a.dir = goal > wx ? 1 : -1; walkStep(a, dt, speed, rw); }
          break;
        }
        case 1: {                          // rabbit hops in
          uint32_t cyc = (now - _ev.pt0) % 700;
          if (cyc < 450) _ev.px16 += (int32_t)_ev.pdir * 26 * 16 * (int32_t)dt / 1000;
          bx = _ev.px16 / 16;
          if (_ev.pdir < 0 ? bx <= (int)_ev.aux : bx >= (int)_ev.aux) { _ev.phase = 2; _ev.pt0 = now; }
          break;
        }
        case 2:                            // the wolf notices
          if (now - _ev.pt0 > 800) { _ev.phase = 3; _ev.pt0 = now; }
          break;
        case 3: {                          // creeping up
          a.dir = bx > wx ? 1 : -1;
          int nose = a.dir > 0 ? bx : bx + bw;
          int mth = a.dir > 0 ? wx + rw - 3 : wx + 2;
          if (abs(mth - nose) <= 6 || now - _ev.pt0 > 9000) { _ev.phase = 4; _ev.pt0 = now; }
          else walkStep(a, dt, 15, rw);
          break;
        }
        case 4:                            // sniff sniff
          if (now - _ev.pt0 > 2300) { _ev.phase = 5; _ev.pt0 = now; _ev.pdir = bx > wx ? 1 : -1; }
          break;
        case 5: {                          // the rabbit hops off
          uint32_t cyc = (now - _ev.pt0) % 560;
          if (cyc < 380) _ev.px16 += (int32_t)_ev.pdir * 40 * 16 * (int32_t)dt / 1000;
          bx = _ev.px16 / 16;
          if (bx < -bw - 2 || bx > SCREEN_W + 2) startMode(M_HAPPY, now, 1200);
          break;
        }
      }
      break;
    }

    case M_BIRD: {                         // a bird lands, pecks at the ground, the wolf stalks it, it flies off
      fixed = false;
      const int bw = BIRD0_R_W, gy = GROUND_Y - (BIRD0_R_H - 1);
      const int mouth = a.dir > 0 ? wx + rw - 3 : wx + 2;
      switch (_ev.phase) {
        case 0: {
          int room_r = SCREEN_W - (wx + rw), room_l = wx;
          int d = room_r >= room_l ? 1 : -1;
          int tx = d > 0 ? mouth + 40 : mouth - 40 - bw;
          if (tx > SCREEN_W - bw - 6) tx = SCREEN_W - bw - 6;
          if (tx < 6) tx = 6;
          a.dir = d;
          int sx = tx + (rnd(0, 1) ? 30 : -30);
          _ev.aux = (uint32_t)tx; _ev.pdir = -d;                 // the bird faces the wolf
          _ev.px16 = sx * 16; _ev.py16 = -12 * 16;
          _ev.vx = (int16_t)((tx - sx) * 1000 / 1100); _ev.vy = (int16_t)((gy + 12) * 1000 / 1100);
          _ev.phase = 1; _ev.pt0 = now; _ev.show = true;
          break;
        }
        case 1:                            // flying in
          _ev.px16 += (int32_t)_ev.vx * 16 * (int32_t)dt / 1000;
          _ev.py16 += (int32_t)_ev.vy * 16 * (int32_t)dt / 1000;
          _ev.frame = (uint8_t)(2 + (((now - _ev.pt0) / 90) & 1));
          if (now - _ev.pt0 >= 1100) { _ev.px16 = (int32_t)_ev.aux * 16; _ev.py16 = gy * 16; _ev.phase = 2; _ev.pt0 = now; }
          break;
        case 2: case 3: {                  // pecking; after a moment the wolf starts to stalk it
          _ev.frame = (uint8_t)((((now - _ev.pt0) / 380) & 1) ? 1 : 0);
          if (_ev.phase == 2) { if (now - _ev.pt0 > 1300) { _ev.phase = 3; _ev.pt0 = now; } break; }
          int bx = _ev.px16 / 16;
          int gap = a.dir > 0 ? bx - mouth : mouth - (bx + bw);
          if (gap <= 12 || now - _ev.pt0 > 6500) {
            _ev.phase = 4; _ev.pt0 = now; _ev.aux2 = now;
            _ev.vx = (int16_t)(a.dir * 52); _ev.vy = -48; _ev.pdir = a.dir;
          } else walkStep(a, dt, 9, rw);
          break;
        }
        case 4:                            // startled
          _ev.px16 += (int32_t)_ev.vx * 16 * (int32_t)dt / 1000;
          _ev.py16 += (int32_t)_ev.vy * 16 * (int32_t)dt / 1000;
          _ev.frame = (uint8_t)(2 + (((now - _ev.pt0) / 90) & 1));
          if (_ev.py16 < -14 * 16 || _ev.px16 / 16 < -bw - 2 || _ev.px16 / 16 > SCREEN_W + 2) startMode(M_HAPPY, now, 1000);
          break;
      }
      break;
    }

    case M_FLY: {                          // a butterfly drifts across, the wolf follows it and leaps at it
      fixed = false;
      switch (_ev.phase) {
        case 0:
          _ev.pdir = rnd(0, 1) ? 1 : -1;
          _ev.px16 = (_ev.pdir > 0 ? -9 : SCREEN_W) * 16;
          _ev.aux = rnd(16, 30);                                  // flight height
          _ev.vy = (int16_t)rnd(1800, 4200);                      // when the wolf jumps
          _ev.vx = 0; _ev.phase = 1; _ev.pt0 = now; _ev.show = true;
          break;
        case 1: {
          uint32_t t = now - _ev.pt0;
          _ev.px16 += (int32_t)_ev.pdir * 20 * 16 * (int32_t)dt / 1000;
          _ev.py16 = (int32_t)(_ev.aux + 7.0f * sinf(t / 420.0f)) * 16;
          _ev.frame = (uint8_t)((t / 130) & 1);
          int bx = _ev.px16 / 16 + 4, wc = wx + rw / 2;
          a.dir = bx > wc ? 1 : -1;
          if (abs(bx - wc) > 16) walkStep(a, dt, 15, rw); else a.legf = 0;
          if (_ev.vx == 0 && t >= (uint32_t)_ev.vy && bx > 8 && bx < SCREEN_W - 8) { _ev.vx = 1; _ev.aux2 = now; }
          if (_ev.px16 / 16 < -12 || _ev.px16 / 16 > SCREEN_W + 4) startMode(M_IDLE, now, 900);
          break;
        }
      }
      break;
    }

    case M_DIG: {                          // dig a hole; sometimes there is a bone down there
      int mouth = a.dir > 0 ? wx + rw - 3 : wx + 2;
      _hole.active = true; _hole.x = mouth - a.dir * 6; _hole.until = now + 30000;
      _hole.depth = (uint8_t)(e / 800 > 4 ? 4 : e / 800);
      if (e >= 4300) {
        if (rnd(0, 99) < 45) {              // dug up a bone: chew it right there
          _treat.active = true; _treat.landed = true;
          _treat.x16 = (int32_t)(_hole.x - 4) * 16; _treat.y16 = (GROUND_Y - 3) * 16;
          _eat_start = now;
          startMode(M_EAT, now, 3200);
        } else startMode(M_HAPPY, now, 1200);
      }
      fixed = false;
      break;
    }

    case M_SLEEP: case M_REST:
      if (now >= a.until) { startMode(a.mode == M_SLEEP ? (uint8_t)M_YAWN : (uint8_t)M_IDLE, now, a.mode == M_SLEEP ? 2300 : 700); }
      fixed = false;
      break;

    case M_CHASE:                          // chasing its own tail
      a.dir = ((e / 230) & 1) ? 1 : -1;
      a.legf = (uint8_t)((e / 90) & 3);
      a.moving = true;
      break;

    case M_SNIFF: {                        // nose down, following a scent
      bool go = ((e / 900) % 3) != 2;
      if (go) walkStep(a, dt, 9, rw); else a.legf = 0;
      break;
    }

    case M_ZOOM:                           // zoomies
      walkStep(a, dt, speed * 2 + 8, rw);
      a.frame_ms += dt;
      break;

    case M_STRETCH:
      if (now >= a.until) { startMode(rnd(0, 1) ? (uint8_t)M_YAWN : (uint8_t)M_IDLE, now, 2300); fixed = false; }
      break;

    default: break;                        // howl, shake, yawn: just play out
  }

  if (fixed && now >= a.until && a.mode >= M_BUNNY) startMode(M_IDLE, now, rnd(900, 2400));
}

void WolfScene::drawEventProps(DisplayDriver& d, uint32_t now, int rw, int ch) {
  // the hole stays after the digging is done
  if (_hole.active) {
    if ((int32_t)(now - _hole.until) >= 0) _hole.active = false;
    else {
      int hw = 7, top = GROUND_Y + 1, dep = _hole.depth < 1 ? 1 : _hole.depth;
      for (int k = 0; k <= dep; k++) {
        int w2 = hw - (k * k * hw) / (dep * dep + 3);
        int y = top + k;
        if (y > 63) break;
        d.fillRect(_hole.x - w2, y, 1, 1);
        d.fillRect(_hole.x + w2, y, 1, 1);
        if (k == dep) d.fillRect(_hole.x - w2, y, 2 * w2 + 1, 1);
      }
      d.fillRect(_hole.x - hw - 5, GROUND_Y, 5, 1);                 // little pile of dirt beside it
      d.fillRect(_hole.x - hw - 4, GROUND_Y - 1, 3, 1);
      d.fillRect(_hole.x + hw + 1, GROUND_Y, 5, 1);
      d.fillRect(_hole.x + hw + 2, GROUND_Y - 1, 3, 1);
    }
  }

  const Actor& a = _w;
  const uint32_t e = now - a.t0;
  const int wx = a.x16 / 16;
  const int hx = wx + (a.dir > 0 ? (rw * 3) / 4 : rw / 4);

  if (_ev.show) {
    int px = _ev.px16 / 16;
    switch (a.mode) {
      case M_BUNNY: {
        bool moving = (_ev.phase == 1 && ((now - _ev.pt0) % 700) < 450) || (_ev.phase == 5 && ((now - _ev.pt0) % 560) < 380);
        uint32_t cyc = _ev.phase == 1 ? (now - _ev.pt0) % 700 : (now - _ev.pt0) % 560;
        bool right = _ev.pdir > 0;
        if (moving) {
          int lift = arc(cyc, _ev.phase == 1 ? 450 : 380, 6);
          d.drawXbm(px, GROUND_Y - (BUNNY_HOP_R_H - 1) - lift, right ? bunny_hop_r : bunny_hop_l, BUNNY_HOP_R_W, BUNNY_HOP_R_H);
        } else {
          d.drawXbm(px, GROUND_Y - (BUNNY_SIT_R_H - 1), right ? bunny_sit_r : bunny_sit_l, BUNNY_SIT_R_W, BUNNY_SIT_R_H);
        }
        break;
      }
      case M_BIRD: {
        bool right = _ev.pdir > 0;
        const uint8_t* bm = bird0_r; const uint8_t* bl = bird0_l;
        switch (_ev.frame) { case 1: bm = bird1_r; bl = bird1_l; break; case 2: bm = bird2_r; bl = bird2_l; break; case 3: bm = bird3_r; bl = bird3_l; break; }
        d.drawXbm(px, _ev.py16 / 16, right ? bm : bl, BIRD0_R_W, BIRD0_R_H);
        break;
      }
      case M_FLY:
        d.drawXbm(px, _ev.py16 / 16, _ev.frame ? fly1 : fly0, FLY0_W, FLY0_H);
        break;
      default: break;
    }
  }

  switch (a.mode) {
    case M_HOWL: {
      int mx = hx < SCREEN_W / 2 ? 104 : 11;
      d.drawXbm(mx, 12, moon, MOON_W, MOON_H);
      if (e > 700) {
        int tx = hx + (a.dir > 0 ? 20 : -20);
        if (tx < 22) tx = 22;
        if (tx > SCREEN_W - 22) tx = SCREEN_W - 22;
        d.drawTextCentered(tx, 11, _stage == STAGE_PUP ? "aroo~" : "AWOOOO~");
      }
      break;
    }
    case M_SLEEP: {
      int base = GROUND_Y - ch / 2 - 6;
      int t = (int)((now / 140) % 24);
      for (int i = 0; i < 3; i++) {
        int off = (t + i * 8) % 24;
        int y = base - off / 2;
        if (y < 10) continue;
        int x = hx + (a.dir > 0 ? 3 : -8) + i * 4 * (a.dir > 0 ? 1 : -1) + (off / 6);
        d.setTextSize(1);
        d.setCursor(x, y);
        d.print(i == 0 ? "z" : "Z");
      }
      break;
    }
    case M_SHAKE: {                        // droplets flying off
      for (int i = 0; i < 8; i++) {
        uint32_t ph = (e / 110 + i * 5) * 2654435761u;
        int dx = (int)((ph >> 8) % (uint32_t)(rw + 16)) - 8;
        int dy = (int)((ph >> 20) % 12);
        d.fillRect(wx + dx, GROUND_Y - ch + 6 + dy, 1, 1);
      }
      break;
    }
    case M_DIG: {                          // dirt flying out behind the front paws
      for (int i = 0; i < 6; i++) {
        uint32_t t = (e + i * 97) % 520;
        int dx = -a.dir * (int)(t * 18 / 520);
        int dy = -arc(t, 520, 9);
        d.fillRect(_hole.x + dx - a.dir * 2, GROUND_Y - 1 + dy, 1, 1);
      }
      break;
    }
    case M_SNIFF: {
      if (((e / 900) % 3) == 2) {          // a questioning moment
        int qx = hx + (a.dir > 0 ? 6 : -10);
        d.setTextSize(1); d.setCursor(qx < 2 ? 2 : qx, GROUND_Y - ch + 4);
        d.print("?");
      }
      break;
    }
    default: break;
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

  _stage = stage; _rw = rw;
  if (_force) {                                   // test hook: start an activity right now
    uint8_t k = _force - 1; _force = 0;
    _treat.active = false;
    beginEvent(k, now, stage);
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
    p.lie = 0;
    p.blink = (((now / 100) % 37) == 0);      // an occasional blink
    int lift = 0, shake = 0;
    switch (_w.mode) {
      case M_WALK: case M_SEEK: case M_LEAVE: case M_RETURN: p.legs = 1 + _w.legf; break;
      case M_GREET: p.legs = (_w.moving && !bow) ? 1 + _w.legf : 0; p.head = bow ? 1 : 0; break;
      case M_EAT:  p.head = (((now - _w.t0) / 230) & 1) ? 0 : 1; break;
      case M_BUNNY:
        p.legs = (_ev.phase == 0 || _ev.phase == 3) && _w.moving ? 1 + _w.legf : 0;
        p.head = _ev.phase == 4 ? 1 : 0;
        if (_ev.phase == 2) p.tail = ((now / 110) & 1);
        break;
      case M_BIRD:
        p.legs = (_ev.phase == 3 && _w.moving) ? 1 + _w.legf : 0;
        lift = _ev.phase == 4 ? arc(now - _ev.aux2, 520, 6) : 0;
        break;
      case M_FLY:
        p.legs = _w.moving ? 1 + _w.legf : 0;
        if (_ev.vx == 1) lift = arc(now - _ev.aux2, 620, 8);
        break;
      case M_DIG:
        p.head = 1; p.legs = 2 + (uint8_t)(((now - _w.t0) / 120) & 1);
        break;
      case M_SLEEP: p.lie = 1; p.blink = true; break;
      case M_REST:  p.lie = 1; p.blink = (((now - _w.t0) / 1700) & 3) == 3 && (((now - _w.t0) / 110) & 1); break;
      case M_HOWL:  p.head = 4; break;
      case M_SHAKE: shake = (((now - _w.t0) / 60) & 1) ? 1 : -1; p.blink = true; break;
      case M_YAWN: {
        uint32_t e = now - _w.t0;
        p.head = (e >= 500 && e < 1700) ? 3 : 2;
        p.blink = p.head == 3;
        break;
      }
      case M_CHASE: p.legs = 1 + _w.legf; lift = arc((now - _w.t0) % 460, 460, 1); break;
      case M_SNIFF: p.head = 1; p.legs = _w.moving ? 1 + _w.legf : 0; break;
      case M_STRETCH: p.head = 1; break;
      case M_ZOOM: p.legs = 1 + _w.legf; break;
      case M_BARK: {
        uint32_t e = now - _w.t0;
        if (e < 500) p.head = 2;
        else if (e < 2100) { bool open = (((e - 500) / 260) & 1) == 0; p.head = open ? 3 : 2; woof = open; }
        else p.head = 2;
        break;
      }
      default: break;
    }
    drawEventProps(d, now, rw, A.ch);
    wolfDrawSprite(d, w.genome, stage, _w.x16 / 16 + shake, GROUND_Y - ((_w.mode == M_GREET && !bow) ? hop_r : 0) - lift, p);
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
