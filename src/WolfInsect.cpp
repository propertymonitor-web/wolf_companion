// WolfInsect: the butterfly's scene -- caterpillar, chrysalis, young and grown butterfly.
#include "WolfScene.h"
#if PET_INSECT && !(PET_MYST && !defined(PET_NS))
#include "PetSpecies.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "PetEgg.h"

PET_NS_OPEN
#include PET_ART

#if PET_MYST
  #define EGG_LOOK 3
#else
  #define EGG_LOOK PET_EGG_STYLE
#endif

#define SCREEN_W 128
#define GROUND_Y 58
#define MAX_W    48
#define MAX_H    42

static const uint8_t heart_bits[] = { 0x6C, 0xFE, 0xFE, 0x7C, 0x38, 0x10 };               // 7 x 6

// ------------------------------------------------------------ sprite maker --
enum Reg : uint8_t { R_NONE = 0, R_FORE, R_HIND, R_BODY, R_ANT, R_CAT, R_HEAD, R_CHRY };

static uint8_t g_reg[MAX_W * MAX_H];
static uint8_t g_lit[MAX_W * MAX_H];
static uint8_t g_bits[((MAX_W + 7) / 8) * MAX_H];
static int g_w, g_h;

static void clearBuf(int w, int h) { g_w = w; g_h = h; memset(g_reg, 0, w * h); memset(g_lit, 0, w * h); }

static void stamp(const InsPart& p, uint8_t reg, bool mirror = false) {
  int stride = (p.w + 7) / 8;
  for (int by = 0; by < p.h; by++) for (int bx = 0; bx < p.w; bx++) {
    if (!(p.bits[by * stride + (bx >> 3)] & (0x80 >> (bx & 7)))) continue;
    int cx = p.x + bx, cy = p.y + by;
    if (mirror) cx = g_w - 1 - cx;
    if (cx < 0 || cx >= g_w || cy < 0 || cy >= g_h) continue;
    if (g_reg[cy * g_w + cx] == R_NONE || reg == R_FORE || reg == R_BODY || reg == R_ANT) g_reg[cy * g_w + cx] = reg;
  }
}

static inline bool inside(int x, int y) { return x >= 0 && x < g_w && y >= 0 && y < g_h && g_reg[y * g_w + x] != R_NONE; }

static inline bool dither(uint8_t level, int x, int y) {
  switch (level) {
    case 0: return false;
    case 1: return ((x & 1) == 0) && ((y & 1) == 0);
    case 2: return ((x + y) & 1) == 0;
    default: return !((x & 1) && (y & 1));
  }
}

// outline every region and fill it; `level(reg, x, y)` picks the density
template <typename F>
static void finish(F level) {
  for (int y = 0; y < g_h; y++) for (int x = 0; x < g_w; x++) {
    uint8_t r = g_reg[y * g_w + x];
    if (r == R_NONE) continue;
    bool edge = !inside(x - 1, y) || !inside(x + 1, y) || !inside(x, y - 1) || !inside(x, y + 1);
    g_lit[y * g_w + x] = edge || dither(level(r, x, y), x, y);
  }
}

static void ring(int cx, int cy, int r0, int r1, bool mirror_x = false) {     // an eyespot
  (void)mirror_x;
  for (int y = cy - r1; y <= cy + r1; y++) for (int x = cx - r1; x <= cx + r1; x++) {
    if (x < 0 || x >= g_w || y < 0 || y >= g_h || !inside(x, y)) continue;
    int d2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
    if (d2 <= r1 * r1 && d2 >= r0 * r0) g_lit[y * g_w + x] = 1;
    else if (d2 < r0 * r0) g_lit[y * g_w + x] = (d2 <= 1);
  }
}

static void flush(DisplayDriver& d, int x, int y) {
  int stride = (g_w + 7) / 8;
  memset(g_bits, 0, stride * g_h);
  for (int j = 0; j < g_h; j++) for (int i = 0; i < g_w; i++)
    if (g_lit[j * g_w + i]) g_bits[j * stride + (i >> 3)] |= 0x80 >> (i & 7);
  d.drawXbm(x, y, g_bits, g_w, g_h);
}

// butterfly seen from the front; (x, y) = the thorax
static void drawButterflySprite(DisplayDriver& d, uint32_t genome, bool adult, int x, int y, int frame) {
  WolfTraits t; WolfGame::traitsOf(genome, t);
  clearBuf(INS_BW, INS_BH);
  const int v = t.tail > 2 ? 2 : t.tail;            // wing shape
  const int f = frame < 0 ? 0 : (frame > 3 ? 3 : frame);
  const InsPart& body = adult ? bf_adult.body : bf_young.body;
  const InsPart& ant  = adult ? bf_adult.ant[t.ear > 2 ? 2 : t.ear] : bf_young.ant[t.ear > 2 ? 2 : t.ear];
  stamp(adult ? bf_adult.hind[v][f] : bf_young.hind[v][f], R_HIND);
  stamp(adult ? bf_adult.fore[v][f] : bf_young.fore[v][f], R_FORE);
  stamp(body, R_BODY);
  stamp(ant, R_ANT);
  const float k = adult ? 1.0f : 0.72f;
  const int span = (int)(22 * k * (f == 0 ? 1.0f : (f == 1 ? 0.7f : (f == 2 ? 0.4f : 0.15f))));
  const uint8_t base = t.shade < 1 ? 1 : t.shade;
  finish([&](uint8_t r, int px, int py) -> uint8_t {
    if (r == R_BODY || r == R_ANT) return t.fluff ? 3 : 2;
    int dx = abs(px - INS_AX), dy = py - INS_TY;
    uint8_t lv = base;
    switch (t.marking) {
      case 1: if (r == R_HIND) lv = 3; break;                                       // dark hind wings
      case 2: if (span > 2 && dx * 100 > span * 66) lv = 3; break;                  // dark wing tips
      case 3: if (span > 2 && dx * 100 > span * 30 && dx * 100 < span * 62) lv = 3; break;   // a bold band
      case 4: lv = ((py >> 1) & 1) ? 3 : base; break;                               // stripes
      default: break;
    }
    if (t.tailtip == 1 && span > 2 && dx * 100 > span * 82) lv = 0;                  // pale edges
    if (t.tailtip == 2 && span > 2 && dx * 100 > span * 82) lv = 3;                  // dark edges
    (void)dy;
    return lv;
  });
  if (t.marking == 5 && f <= 1) {                                                   // eyespots
    int ox = (int)((f == 0 ? 9.0f : 6.0f) * k), oy = (int)(9.0f * k);
    ring(INS_AX - ox, INS_TY + oy, 1, 3); ring(INS_AX + ox, INS_TY + oy, 1, 3);
  }
  flush(d, x - INS_AX, y - INS_TY);
}

static void drawCaterpillarSprite(DisplayDriver& d, uint32_t genome, int x, int ground_y, int frame, bool mirror) {
  WolfTraits t; WolfGame::traitsOf(genome, t);
  clearBuf(INS_CW, INS_CH);
  const InsPart& p = cat_frames[frame < 0 ? 0 : (frame > 5 ? 5 : frame)];
  stamp(p, R_CAT, mirror);
  // the head is the front 5 columns of the sprite
  const int hx0 = p.x + p.w - 6;
  for (int yy = 0; yy < g_h; yy++) for (int xx = 0; xx < g_w; xx++) {
    int sx = mirror ? g_w - 1 - xx : xx;
    if (g_reg[yy * g_w + xx] != R_NONE && sx >= hx0 && frame != 5) g_reg[yy * g_w + xx] = R_HEAD;
  }
  const uint8_t base = t.shade;
  finish([&](uint8_t r, int px, int py) -> uint8_t {
    int sx = mirror ? g_w - 1 - px : px;
    if (r == R_HEAD) return 3;
    uint8_t lv = base;
    switch (t.marking) {
      case 1: case 2: lv = (((sx - p.x) / 3) & 1) ? 3 : base; break;                // banded
      case 3: if (py >= g_h - 7) lv = 3; break;                                      // dark belly
      case 4: lv = (((sx - p.x) / 5) & 1) ? 2 : 3; break;
      case 5: lv = ((sx * 7 + py * 3 + t.spotseed) % 9 == 0) ? 0 : base; break;       // spotted
      default: break;
    }
    return lv;
  });
  if (t.fluff) {                                                                     // a furry fringe
    for (int yy = 0; yy < g_h; yy++) for (int xx = 0; xx < g_w; xx++) {
      if (g_reg[yy * g_w + xx] != R_NONE || g_lit[yy * g_w + xx]) continue;
      if (inside(xx, yy + 1) && (((xx * 73856093u) ^ (yy * 19349663u) ^ genome) >> 9) % 3 == 0) g_lit[yy * g_w + xx] = 1;
    }
  }
  flush(d, x, ground_y - (g_h - 1));
}

static void drawChrysalisSprite(DisplayDriver& d, uint32_t genome, int x, int y, int frame) {
  WolfTraits t; WolfGame::traitsOf(genome, t);
  clearBuf(INS_YW, INS_YH);
  stamp(chry_frames[frame < 0 ? 0 : (frame > 2 ? 2 : frame)], R_CHRY);
  const uint8_t base = t.shade;
  finish([&](uint8_t, int px, int py) -> uint8_t {
    if (t.marking >= 3 && ((py / 3) & 1)) return 3;                                   // ridged
    (void)px;
    return base >= 2 ? 2 : base;
  });
  if (t.marking == 5 || t.marking == 2) {                                           // a few golden studs
    for (int i = 0; i < 3; i++) { int yy = 8 + i * 3; if (inside(7, yy)) g_lit[yy * g_w + 7] ^= 1; }
  }
  flush(d, x, y);
}

// ------------------------------------------------------------------- scene --
WolfScene::WolfScene(WolfGame* game) : g(game), _init(false), _last_ms(0), _rng(0x9E3779B9u), _shown_idx(0), _vphase(V_NONE),
    _form(F_ADULT), _last_ev(255), _force(0), _was_egg(false), _hatch_t0(0), _shown_genome(0), _vseen_id(0), _trail_n(0) {
  memset(&_w, 0, sizeof(_w));
  memset(&_v, 0, sizeof(_v));
  memset(&_food, 0, sizeof(_food));
  memset(&_lady, 0, sizeof(_lady));
  memset(_trail, 0, sizeof(_trail));
}

uint32_t WolfScene::rnd(uint32_t lo, uint32_t hi) {
  _rng ^= _rng << 13; _rng ^= _rng >> 17; _rng ^= _rng << 5;
  return hi <= lo ? lo : lo + _rng % (hi - lo + 1);
}

uint8_t WolfScene::formOf(uint32_t months) const {
  if (months < WOLF_PUP_MONTHS) return F_CATERPILLAR;
  if (months < PET_HIDDEN_TO) return F_CHRYSALIS;
  return months < WOLF_ADULT_MONTHS ? F_YOUNG : F_ADULT;
}

void WolfScene::startMode(uint8_t m, uint32_t now, uint32_t dur_ms) {
  _w.mode = m; _w.t0 = now; _w.until = now + dur_ms; _w.phase = 0; _w.pt0 = now; _w.frame_ms = 0;
  if (m != I_LADY) _lady.show = false;
  _trail_n = 0;
}

bool WolfScene::flyTo(Actor& a, float tx, float ty, float speed, uint32_t dt) {
  float dx = tx - a.x, dy = ty - a.y;
  float dist = sqrtf(dx * dx + dy * dy);
  float step = speed * dt / 1000.0f;
  a.moving = true;
  if (dx > 0.4f) a.dir = 1; else if (dx < -0.4f) a.dir = -1;
  if (dist <= step + 0.3f) { a.x = tx; a.y = ty; return true; }
  a.x += dx / dist * step; a.y += dy / dist * step;
  return false;
}

void WolfScene::crawl(Actor& a, uint32_t dt, float speed, int w) {
  a.x += a.dir * speed * dt / 1000.0f;
  if (a.x < 0) { a.x = 0; a.dir = 1; }
  if (a.x > SCREEN_W - w) { a.x = (float)(SCREEN_W - w); a.dir = -1; }
  a.frame_ms += dt;
  if (a.frame_ms >= 150) { a.frame_ms = 0; a.frame = (uint8_t)((a.frame + 1) & 3); }
  a.moving = true;
}

void WolfScene::beginEvent(uint8_t kind, uint32_t now) {
  _last_ev = kind;
  bool bf = _form == F_YOUNG || _form == F_ADULT;
  if (kind == 0) {                                   // food: a flower (or a leaf) sprouts
    if (!_food.active) { _food.active = true; _food.grown = false; _food.t0 = now; _food.x = (float)rnd(20, 100); }
    startMode(I_IDLE, now, 300);
    return;
  }
  if (kind == 1) kind = bf ? 1 : 5;                  // bask   (caterpillars look around instead)
  if (kind == 2 && !bf) kind = 5;                    // dance
  if (kind == 4 && bf) kind = 3;                     // crawl  (butterflies meet a ladybug)
  if (kind == 5 && bf) kind = 1;
  if (kind == 6 && bf) kind = 2;
  static const uint8_t md[7] = { I_IDLE, I_BASK, I_DANCE, I_LADY, I_CRAWL, I_REAR, I_CURL };
  static const uint16_t dur[7] = { 0, 0, 3400, 0, 4200, 3200, 0 };
  uint32_t d = dur[kind];
  if (kind == 6) d = rnd(7000, 11000);
  if (!d) d = 60000;
  startMode(md[kind], now, d);
  _w.ax = (float)rnd(20, 108);
}

void WolfScene::update(uint32_t now, uint32_t dt) {
  Actor& a = _w;
  a.moving = false;
  const bool bf = _form == F_YOUNG || _form == F_ADULT;
  const float k = _form == F_ADULT ? 1.0f : 0.72f;
  const int cw = INS_CW;

  // flower / leaf grows
  if (_food.active && !_food.grown && now - _food.t0 > 1400) _food.grown = true;

  // trips: out through an edge, home again through an edge
  const WolfAway& aw = g->away[g->current];
  if (_form != F_CHRYSALIS) {
    if (aw.away && a.mode != I_LEAVE && a.mode != I_AWAY) {
      a.dir = aw.leave_dir ? aw.leave_dir : 1;
      startMode(I_LEAVE, now, 0);
    } else if (!aw.away && (a.mode == I_LEAVE || a.mode == I_AWAY)) {
      a.dir = aw.return_dir ? aw.return_dir : 1;
      a.x = a.dir > 0 ? -30.0f : (float)SCREEN_W + 30.0f;
      if (!bf) a.x = a.dir > 0 ? -(float)cw : (float)SCREEN_W;
      a.y = bf ? 30.0f : (float)GROUND_Y;
      startMode(I_RETURN, now, 0);
    }
  }
  // a guest takes priority
  if (a.mode != I_LEAVE && a.mode != I_AWAY && a.mode != I_RETURN && _form != F_CHRYSALIS) {
    if (_vphase != V_NONE && a.mode != I_GREET) startMode(I_GREET, now, 0);
    if (_vphase == V_NONE && a.mode == I_GREET) startMode(I_IDLE, now, 600);
  }
  if (_form == F_CHRYSALIS) {                         // just hangs there (and twitches now and then)
    if (a.mode != I_IDLE) startMode(I_IDLE, now, 1000);
    return;
  }

  const bool in_event = a.mode >= I_BASK;
  if (in_event && (g->bark_pending || (_food.active && _food.grown && a.mode != I_LADY))) startMode(I_IDLE, now, 0);
  if (a.mode == I_IDLE || a.mode == I_WANDER || in_event) {
    if (g->bark_pending && !in_event) { g->bark_pending = false; startMode(I_FLOURISH, now, 2600); }
    else if (!_food.active && g->treat_pending) {
      g->treat_pending--;
      _food.active = true; _food.grown = false; _food.t0 = now; _food.x = (float)rnd(20, 100);
    }
  }
  if ((a.mode == I_IDLE || a.mode == I_WANDER) && _food.active && _food.grown) startMode(I_SEEK, now, 0);

  const float foodTopY = GROUND_Y - 20 - 4;                      // above the open flower
  switch (a.mode) {
    case I_IDLE:
      if (!bf) a.frame = 0;
      if (now >= a.until) {
        if (rnd(0, 99) < 45 && !_food.active && !g->treat_pending && _vphase == V_NONE) {
          static const uint8_t wb[7] = { 3, 3, 2, 2, 0, 0, 0 }, wc[7] = { 3, 0, 0, 2, 4, 2, 3 };
          const uint8_t* w = bf ? wb : wc;
          int total = 0; for (int i = 0; i < 7; i++) if (i != _last_ev) total += w[i];
          int r = (int)rnd(0, (uint32_t)total - 1), pick = 0;
          for (int i = 0; i < 7; i++) { if (i == _last_ev) continue; if (r < w[i]) { pick = i; break; } r -= w[i]; }
          beginEvent((uint8_t)pick, now);
          break;
        }
        if (bf) {
          a.ax = (float)rnd(20, 108); a.ay = (float)rnd(20, 44);
          startMode(I_WANDER, now, 12000);
        } else {
          a.dir = rnd(0, 1) ? 1 : -1;
          startMode(I_WANDER, now, rnd(2500, 5000));
        }
      }
      break;
    case I_WANDER:
      if (bf) {
        if (flyTo(a, a.ax, a.ay, 20.0f, dt) || now >= a.until) startMode(I_IDLE, now, rnd(500, 1500));
      } else {
        crawl(a, dt, 8.0f, cw);
        if (now >= a.until) startMode(I_IDLE, now, rnd(1200, 3000));
      }
      break;
    case I_SEEK: {
      if (!_food.active) { startMode(I_IDLE, now, 400); break; }
      if (bf) {
        if (flyTo(a, _food.x + 6, foodTopY - 2 - 8 * k, 26.0f, dt)) { _food.eat0 = now; startMode(I_FEED, now, 3800); }
      } else {
        const float fc = _food.x + 6;
        if (a.phase == 0) { a.dir = fc > a.x + cw / 2 ? 1 : -1; a.phase = 1; }
        float tx = a.dir > 0 ? fc - (cw - 4) : fc - 4;
        if (tx < 0) tx = 0;
        if (tx > SCREEN_W - cw) tx = (float)(SCREEN_W - cw);
        if (fabsf(a.x - tx) <= 1.5f) { a.x = tx; _food.eat0 = now; startMode(I_FEED, now, 3800); }
        else {
          a.x += (tx > a.x ? 1 : -1) * 11.0f * dt / 1000.0f; a.moving = true;
          a.frame_ms += dt; if (a.frame_ms >= 150) { a.frame_ms = 0; a.frame = (uint8_t)((a.frame + 1) & 3); }
        }
      }
      break;
    }
    case I_FEED:
      if (now >= a.until) { _food.active = false; startMode(I_HAPPY, now, 1400); }
      break;
    case I_HAPPY: case I_FLOURISH:
      if (now >= a.until) startMode(I_IDLE, now, rnd(600, 1600));
      break;
    case I_GREET: break;                              // handled in updateVisitor's partner logic below
    case I_LEAVE:
      if (bf) { a.x += a.dir * 34.0f * dt / 1000.0f; a.y -= 3.0f * dt / 1000.0f; if (a.dir > 0 ? a.x > SCREEN_W + 30 : a.x < -30) startMode(I_AWAY, now, 0); }
      else    { a.x += a.dir * 16.0f * dt / 1000.0f; if (a.dir > 0 ? a.x > SCREEN_W : a.x < -cw) startMode(I_AWAY, now, 0); }
      a.moving = true;
      a.frame_ms += dt; if (a.frame_ms >= 120) { a.frame_ms = 0; a.frame = (uint8_t)((a.frame + 1) & 3); }
      break;
    case I_AWAY: break;
    case I_RETURN: {
      float tx = bf ? 50.0f + (a.dir > 0 ? 0 : 28) : (a.dir > 0 ? 14.0f : SCREEN_W - cw - 14.0f);
      if (bf) { if (flyTo(a, a.dir > 0 ? 30.0f : 98.0f, 30.0f, 34.0f, dt)) startMode(I_HAPPY, now, 1500); }
      else { a.x += a.dir * 16.0f * dt / 1000.0f; a.moving = true;
             a.frame_ms += dt; if (a.frame_ms >= 120) { a.frame_ms = 0; a.frame = (uint8_t)((a.frame + 1) & 3); }
             if (a.dir > 0 ? a.x >= tx : a.x <= tx) startMode(I_HAPPY, now, 1500); }
      break;
    }

    // ---- activities ----
    case I_BASK:                                    // land on the ground, wings open to the sun
      switch (a.phase) {
        case 0:
          if (flyTo(a, a.ax, GROUND_Y - 17.0f * k, 22.0f, dt)) { a.phase = 1; a.pt0 = now; }
          break;
        case 1:
          if (now - a.pt0 > 4400) { a.phase = 2; a.pt0 = now; a.ax = a.x + (a.dir > 0 ? 8.0f : -8.0f); a.ay = 30.0f; }
          break;
        case 2:
          if (flyTo(a, a.ax, a.ay, 26.0f, dt) || now - a.pt0 > 6000) startMode(I_IDLE, now, 700);
          break;
      }
      break;
    case I_DANCE: {                                 // a loop-de-loop, leaving sparkles
      uint32_t e = now - a.t0;
      if (a.phase == 0) {
        a.ax = a.x < 36 ? 36.0f : (a.x > 92 ? 92.0f : a.x);
        a.ay = a.y < 30 ? 30.0f : (a.y > 38 ? 38.0f : a.y);
        a.phase = 1;
      }
      float ang = e * 2.0f * 3.14159265f / 1600.0f;
      a.x = a.ax + 12.0f * sinf(ang); a.y = a.ay - 8.0f * cosf(ang) + 8.0f;
      a.dir = cosf(ang) >= 0 ? 1 : -1;
      a.moving = true;
      if (_trail_n < 6) _trail_n++;
      for (int i = 5; i > 0; i--) { _trail[i][0] = _trail[i - 1][0]; _trail[i][1] = _trail[i - 1][1]; }
      _trail[0][0] = a.x; _trail[0][1] = a.y;
      if (now >= a.until) startMode(I_IDLE, now, 600);
      break;
    }
    case I_LADY: {                                  // a ladybug crosses the ground, then takes off
      switch (a.phase) {
        case 0: {
          int d = rnd(0, 1) ? 1 : -1;
          _lady.show = true; _lady.dir = (int8_t)d; _lady.frame = 0;
          _lady.x = d > 0 ? -12.0f : (float)SCREEN_W; _lady.y = (float)GROUND_Y;
          a.ax = (float)rnd(40, 80);                                            // where it stops
          a.phase = 1; a.pt0 = now;
          break;
        }
        case 1:
          _lady.x += _lady.dir * 11.0f * dt / 1000.0f;
          _lady.frame = (uint8_t)(((now - a.pt0) / 140) & 1);
          if (!bf) a.dir = _lady.x + 4 > a.x + cw / 2 ? 1 : -1;
          if (bf) flyTo(a, _lady.x + 5, GROUND_Y - 24.0f, 22.0f, dt);
          if (_lady.dir > 0 ? _lady.x >= a.ax : _lady.x <= a.ax) { a.phase = 2; a.pt0 = now; }
          break;
        case 2:
          _lady.frame = 0;
          if (bf) flyTo(a, _lady.x + 5, GROUND_Y - 24.0f, 22.0f, dt);
          if (now - a.pt0 > 1700) { a.phase = 3; a.pt0 = now; }
          break;
        case 3:
          _lady.x += _lady.dir * 40.0f * dt / 1000.0f; _lady.y -= 30.0f * dt / 1000.0f;
          _lady.frame = (uint8_t)(2 + (((now - a.pt0) / 80) & 1));
          if (bf) flyTo(a, _lady.x + 5, GROUND_Y - 24.0f, 14.0f, dt);
          if (_lady.y < -12 || _lady.x < -14 || _lady.x > SCREEN_W + 4) startMode(I_IDLE, now, 800);
          break;
      }
      break;
    }
    case I_CRAWL:
      crawl(a, dt, 9.0f, cw);
      if (now >= a.until) startMode(I_IDLE, now, 900);
      break;
    case I_REAR:                                    // rears up and looks about
      a.frame = 4;
      if (((now - a.t0) / 1100) % 2 == 1) a.dir = a.dir;                      // (looking is done in the draw)
      if (now >= a.until) startMode(I_IDLE, now, 900);
      break;
    case I_CURL:
      a.frame = 5;
      if (now >= a.until) startMode(I_IDLE, now, 900);
      break;
  }
}

// ----------------------------------------------------------------- guests --
void WolfScene::updateVisitor(uint32_t now, uint32_t dt) {
  const WolfVisitor& v = g->visitor;
  const uint8_t vform = formOf(v.months);
  const bool vbf = vform >= F_YOUNG;
  if (v.active && _vphase == V_NONE && v.id != _vseen_id) {
    _vseen_id = v.id; _vphase = V_ENTER;
    _v.dir = v.enter_dir ? v.enter_dir : -1;
    _v.x = vbf ? (_v.dir > 0 ? -26.0f : SCREEN_W + 26.0f) : (_v.dir > 0 ? -(float)INS_CW : (float)SCREEN_W);
    _v.y = vbf ? 30.0f : (float)GROUND_Y;
    _v.frame = 0; _v.frame_ms = 0; _v.pt0 = 0;
  }
  if (!v.active && (_vphase == V_ENTER || _vphase == V_STAY)) { _vphase = V_LEAVE; _v.dir = v.exit_dir ? v.exit_dir : 1; }
  _v.moving = false;
  const bool rbf = _form >= F_YOUNG;
  // meeting places
  const float vspot = v.enter_dir > 0 ? 34.0f : 94.0f, rspot = v.enter_dir > 0 ? 94.0f : 34.0f;
  switch (_vphase) {
    case V_ENTER:
      _v.moving = true;
      if (vbf) { if (flyTo(_v, vspot, 32.0f, 30.0f, dt)) _vphase = V_STAY; }
      else {
        _v.x += _v.dir * 16.0f * dt / 1000.0f;
        _v.frame_ms += dt; if (_v.frame_ms >= 130) { _v.frame_ms = 0; _v.frame = (uint8_t)((_v.frame + 1) & 3); }
        float tx = v.enter_dir > 0 ? 8.0f : SCREEN_W - INS_CW - 8.0f;
        if (_v.dir > 0 ? _v.x >= tx : _v.x <= tx) _vphase = V_STAY;
      }
      break;
    case V_STAY:
      if ((int32_t)(now + 4000 - v.until_ms) >= 0) { _vphase = V_LEAVE; _v.dir = v.exit_dir ? v.exit_dir : 1; break; }
      if (vbf && rbf && _w.mode == I_GREET) {         // two butterflies dance round each other
        if (_v.pt0 == 0) _v.pt0 = now ? now : 1;
        float ang = (now - _v.pt0) * 2.0f * 3.14159265f / 2400.0f;
        float cx = 64.0f, cy = 32.0f, rx = 24.0f - (g->visitor.visits >= 3 ? 8.0f : 0.0f);
        _v.x = cx + rx * sinf(ang);  _v.y = cy + 8.0f * cosf(ang) * 1.0f;
        _w.x = cx - rx * sinf(ang);  _w.y = cy - 8.0f * cosf(ang);
        _v.dir = cosf(ang) >= 0 ? 1 : -1; _w.dir = -_v.dir;
        _v.moving = _w.moving = true;
      } else if (vbf) {                                // a butterfly hovering over somebody on the ground
        float tx = (_form == F_CATERPILLAR || !rbf) ? _w.x + INS_CW / 2 : vspot;
        float ty = (_form == F_CATERPILLAR || !rbf) ? GROUND_Y - 26.0f : 32.0f + 3.0f * sinf(now / 400.0f);
        flyTo(_v, tx, ty, 18.0f, dt);
      }
      break;
    case V_LEAVE:
      _v.pt0 = 0; _v.moving = true;
      if (vbf) { _v.x += _v.dir * 34.0f * dt / 1000.0f; if (_v.dir > 0 ? _v.x > SCREEN_W + 26 : _v.x < -26) _vphase = V_NONE; }
      else {
        _v.x += _v.dir * 16.0f * dt / 1000.0f;
        _v.frame_ms += dt; if (_v.frame_ms >= 130) { _v.frame_ms = 0; _v.frame = (uint8_t)((_v.frame + 1) & 3); }
        if (_v.dir > 0 ? _v.x > SCREEN_W : _v.x < -INS_CW) _vphase = V_NONE;
      }
      break;
    default: break;
  }
  // the resident's side of it
  if (_w.mode == I_GREET) {
    if (rbf && vbf && _vphase == V_STAY) { /* orbiting: positions already set above */ }
    else if (rbf) { flyTo(_w, rspot, 32.0f + 3.0f * sinf(now / 450.0f), 24.0f, dt); }
    else {                                           // caterpillar: crawl to its place, then rear up to say hello
      float tx = v.enter_dir > 0 ? SCREEN_W - INS_CW - 8.0f : 8.0f;
      if (fabsf(_w.x - tx) > 1.5f) { _w.dir = tx > _w.x ? 1 : -1; crawl(_w, dt, 13.0f, INS_CW); }
      else { _w.x = tx; _w.dir = -(v.enter_dir ? v.enter_dir : -1); _w.frame = (_vphase == V_STAY && ((now / 1500) & 1)) ? 4 : 0; }
    }
  }
}

// ------------------------------------------------------------------ drawing --
void WolfScene::drawCreature(DisplayDriver& d, uint32_t genome, uint8_t form, const Actor& a, uint32_t now, bool resident) {
  switch (form) {
    case F_CATERPILLAR: {
      int f = a.frame;
      if (a.mode == I_GREET && resident) f = a.frame;               // set by the greeting logic
      if (resident && a.mode == I_REAR) {                           // look left, look right
        f = 4;
      }
      if (f <= 3 && !a.moving) f = 0;
      bool mirror = a.dir < 0;
      if (resident && a.mode == I_REAR && ((now - a.t0) / 1100) % 2 == 1) mirror = !mirror;
      int lift = 0;
      if (resident && (a.mode == I_HAPPY || a.mode == I_FLOURISH)) { f = (((now - a.t0) / 220) & 1) ? 4 : 0; }
      if (resident && a.mode == I_FEED) f = (((now - a.t0) / 260) & 1) ? 0 : 1;
      drawCaterpillarSprite(d, genome, (int)a.x, GROUND_Y - lift, f, mirror);
      break;
    }
    case F_CHRYSALIS: {
      int f = 0;
      uint32_t c = (now / 100) % 160;                               // an occasional wriggle
      if (c < 10) f = 1; else if (c < 20) f = 2; else if (c >= 24 && c < 30) f = 1;
      drawChrysalisSprite(d, genome, (int)a.x, 10, f);
      break;
    }
    default: {
      bool adult = form == F_ADULT;
      int frame;
      static const uint8_t seq[6] = {0, 1, 2, 3, 2, 1};
      if (resident && a.mode == I_BASK && a.phase == 1) frame = (((now - a.pt0) % 1300) > 1100) ? 2 : 0;
      else if (resident && a.mode == I_FEED) frame = ((now / 650) & 1) ? 0 : 2;
      else if (a.moving || (resident && (a.mode == I_DANCE || a.mode == I_HAPPY || a.mode == I_FLOURISH))) frame = seq[(now / 75) % 6];
      else frame = seq[(now / 140) % 6];
      float y = a.y;
      if (!(resident && ((a.mode == I_BASK && a.phase == 1) || a.mode == I_FEED))) y += 1.5f * sinf(now / 260.0f);   // gentle bob
      int ix = (int)(a.x + (a.x < 0 ? -0.5f : 0.5f)), iy = (int)(y + 0.5f);
      if (resident && (a.mode == I_HAPPY || a.mode == I_FLOURISH)) {  // a little loop
        float ang = (now - a.t0) * 2.0f * 3.14159265f / 700.0f;
        ix += (int)(8.0f * sinf(ang)); iy += (int)(5.0f * (1.0f - cosf(ang)) - 5.0f);
      }
      drawButterflySprite(d, genome, adult, ix, iy, frame);
    }
  }
}

void WolfScene::drawProps(DisplayDriver& d, uint32_t now) {
  const bool bf = _form >= F_YOUNG;
  if (_food.active) {
    int fx = (int)_food.x;
    if (bf) {
      int fr = (!_food.grown) ? ((now - _food.t0) < 600 ? 0 : 1) : 2;
      const uint8_t* bm = fr == 0 ? flower0 : (fr == 1 ? flower1 : flower2);
      d.drawXbm(fx, GROUND_Y - 19, bm, FLOWER0_W, FLOWER0_H);
    } else {
      int e = (_w.mode == I_FEED) ? (int)((now - _food.eat0) / 1300) : 0;
      if (e > 2) e = 2;
      const uint8_t* bm = e == 0 ? leaf0 : (e == 1 ? leaf1 : leaf2);
      d.drawXbm(fx, GROUND_Y - 5, bm, LEAF0_W, LEAF0_H);
    }
  }
  if (_lady.show) {
    int f = _lady.frame > 3 ? 3 : _lady.frame;
    const uint8_t* bm = _lady.dir > 0 ? (f == 0 ? lady0_r : f == 1 ? lady1_r : f == 2 ? lady2_r : lady3_r)
                                      : (f == 0 ? lady0_l : f == 1 ? lady1_l : f == 2 ? lady2_l : lady3_l);
    d.drawXbm((int)_lady.x, (int)_lady.y - (LADY0_R_H - 1), bm, LADY0_R_W, LADY0_R_H);
  }
  if (_w.mode == I_DANCE) {                                        // sparkles along the path
    for (int i = 1; i < _trail_n; i++) {
      int sx = (int)_trail[i][0] + (i & 1 ? 9 : -9), sy = (int)_trail[i][1] + 2 - (i * 3) / 2;
      d.fillRect(sx, sy, 1, 1);
      if (i % 2 == 0) { d.fillRect(sx - 1, sy, 3, 1); d.fillRect(sx, sy - 1, 1, 3); }
    }
  }
  if (_w.mode == I_FLOURISH) {
    for (int i = 0; i < 6; i++) {
      uint32_t ph = ((now - _w.t0) / 180 + i * 7) * 2654435761u;
      int sx = (int)_w.x + (int)((ph >> 8) % 41) - 20, sy = (int)_w.y + (int)((ph >> 18) % 29) - 18;
      if (sy < 11) sy = 11;
      d.fillRect(sx, sy, 1, 1); d.fillRect(sx - 1, sy, 3, 1); d.fillRect(sx, sy - 1, 1, 3);
    }
  }
  if (_w.mode == I_CURL) {                                          // z z Z
    int base = GROUND_Y - 24, t = (int)((now / 140) % 24);
    for (int i = 0; i < 3; i++) {
      int off = (t + i * 8) % 24, y = base - off / 2;
      if (y < 10) continue;
      d.setTextSize(1); d.setCursor((int)_w.x + 22 + i * 4 + off / 6, y); d.print(i == 0 ? "z" : "Z");
    }
  }
}

void WolfScene::drawHud(DisplayDriver& d, const Wolf& w, uint16_t batt_mv) {
  d.setTextSize(1);
  d.setColor(UIColor::primary_txt);
  char tmp[24];
  if (WolfGame::isEgg(w))
    snprintf(tmp, sizeof(tmp), "EGG %uh", (unsigned)((WolfGame::eggLeftSecs(w) + 3599) / 3600));
  else if (g->count > 1)
    snprintf(tmp, sizeof(tmp), "%s %umo %u/%u", w.sex ? "F" : "M", (unsigned)WolfGame::ageMonths(w),
             (unsigned)(g->current + 1), (unsigned)g->count);
  else
    snprintf(tmp, sizeof(tmp), "%s %umo", w.sex ? "F" : "M", (unsigned)WolfGame::ageMonths(w));
  d.setCursor(0, 0);
  d.print(tmp);
  int tw = d.getTextWidth(tmp);
  if (w.preg_secs_left) d.drawXbm(tw + 5, 1, heart_bits, 7, 6);
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
  uint8_t form = formOf(WolfGame::ageMonths(w));

  if (WolfGame::isEgg(w)) {                       // not hatched yet
    _was_egg = true;
    d.setColor(UIColor::primary_txt);
    for (int x = 0; x < SCREEN_W; x += 2) d.fillRect(x, GROUND_Y + 1, 1, 1);
    eggDraw(d, w, EGG_LOOK, now, GROUND_Y, SCREEN_W);
    drawHud(d, w, batt_mv);
    return 110;
  }
  if (_was_egg) { _was_egg = false; _hatch_t0 = now; _init = false; }

  if (!_init || g->current != _shown_idx || w.genome != _shown_genome || form != _form) {
    _rng ^= w.genome + now;
    if (!_rng) _rng = 1;
    _shown_idx = g->current; _shown_genome = w.genome; _form = form;
    memset(&_w, 0, sizeof(_w));
    _food.active = false; _lady.show = false;
    _w.dir = rnd(0, 1) ? 1 : -1;
    if (form == F_CATERPILLAR) { _w.x = (float)rnd(10, SCREEN_W - INS_CW - 10); _w.y = GROUND_Y; }
    else if (form == F_CHRYSALIS) { _w.x = (float)rnd(30, 90); _w.y = 10; }
    else { _w.x = (float)rnd(30, 98); _w.y = (float)rnd(24, 40); }
    if (g->away[g->current].away && form != F_CHRYSALIS) startMode(I_AWAY, now, 0);
    else startMode(I_IDLE, now, 700);
    _init = true; _last_ms = now;
  }
  uint32_t dt = now - _last_ms;
  if (dt > 250) dt = 250;
  _last_ms = now;

  if (_force) {
    uint8_t k = _force - 1; _force = 0;
    _food.active = false;
    beginEvent(k, now);
  }
  if (g->visitor.active || _vphase != V_NONE) updateVisitor(now, dt);
  update(now, dt);

  d.setColor(UIColor::primary_txt);
  for (int x = 0; x < SCREEN_W; x += 2) d.fillRect(x, GROUND_Y + 1, 1, 1);     // the ground
  if (_hatch_t0) {
    uint32_t since = now - _hatch_t0;
    if (since > 120000) _hatch_t0 = 0;
    else {
      eggShell(d, w, PET_EGG_STYLE, GROUND_Y, SCREEN_W, since);
      if (since < 2000 && ((since / 250) & 1) == 0) d.drawTextCentered(SCREEN_W / 2, 12, "CRACK!");
    }
  }
  if (form == F_CHRYSALIS) d.fillRect(0, 9, SCREEN_W, 1);                      // the twig it hangs from

  drawProps(d, now);
  if (_vphase != V_NONE) {
    uint8_t vf = formOf(g->visitor.months);
    drawCreature(d, g->visitor.genome, vf, _v, now, false);
  }
  if (_w.mode != I_AWAY) drawCreature(d, w.genome, form, _w, now, true);
  else d.drawTextCentered(SCREEN_W / 2, 30, "off visiting...");

  if ((int32_t)(g->hearts_until_ms - now) > 0) {
    for (int i = 0; i < 3; i++) {
      int tri = (int)(((now / 110) + i * 3) % 12);
      if (tri > 6) tri = 12 - tri;
      d.drawXbm(52 + i * 12, 14 + tri, heart_bits, 7, 6);
    }
  }
  drawHud(d, w, batt_mv);
  return 110;
}

#include "PetFactory.inc"
#endif   // PET_INSECT
