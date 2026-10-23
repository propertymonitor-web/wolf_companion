#pragma once
// PetEgg: the egg a pet starts in, and the shell left behind when it hatches.  Shared by the quadruped and the
// insect scene.  Drawn from scratch: an egg-shaped outline, a pattern for the species, cracks that grow as hatching
// time gets near, and a wobble that gets stronger.

#include <helpers/ui/DisplayDriver.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "WolfCore.h"

#define EGG_W   26
#define EGG_H   32
#define EGG_CW  (EGG_W + 12)      // room for the sideways lean while it rocks

// look of the shell: 0 speckled, 1 scaly (dragon), 2 flames (phoenix), 3 mystery ("?")
static inline bool eggInside(int x, int y) {
  if (y < 0 || y >= EGG_H) return false;
  float t = (y + 0.5f) / EGG_H, u = 2.0f * t - 1.0f;
  float hw = (EGG_W / 2.0f) * sqrtf(1.0f - u * u) * (0.80f + 0.26f * t);
  if (hw > EGG_W / 2.0f) hw = EGG_W / 2.0f;
  float dx = (x + 0.5f) - EGG_W / 2.0f;
  return dx >= -hw && dx <= hw;
}

static const uint8_t egg_q_bits[7] = { 0x70, 0xD8, 0x18, 0x30, 0x20, 0x00, 0x20 };      // a 5 x 7 question mark (top 5 bits)

// build the egg into `bits` (EGG_CW x EGG_H, MSB first) and return nothing; p = hatch progress 0..1000
static inline void eggBuild(uint8_t* bits, uint32_t genome, uint8_t style, int progress, uint32_t now, bool lowerOnly) {
  const int stride = (EGG_CW + 7) / 8;
  memset(bits, 0, stride * EGG_H);
  // rocking: nothing at first, a gentle tilt later, a frantic shake at the end
  float tilt = 0;
  if (progress > 250) {
    float amp = progress < 700 ? 1.2f : (progress < 940 ? 2.4f : 4.0f);
    float per = progress < 940 ? 700.0f : 180.0f;
    uint32_t cyc = now % (uint32_t)(per * (progress < 700 ? 6 : 3));
    tilt = cyc < per * 2.0f ? amp * sinf(cyc * 6.2831853f / per) : 0.0f;      // a rock, then a pause
  }
  for (int y = 0; y < EGG_H; y++) {
    if (lowerOnly && y < EGG_H * 55 / 100) continue;
    int shift = (int)lroundf(tilt * (EGG_H - 1 - y) / EGG_H) + 6;
    for (int x = 0; x < EGG_W; x++) {
      if (!eggInside(x, y)) continue;
      bool edge = !eggInside(x - 1, y) || !eggInside(x + 1, y) || !eggInside(x, y - 1) || !eggInside(x, y + 1);
      bool on = edge;
      if (lowerOnly && y < EGG_H * 55 / 100 + 3 && ((x / 2 + y) & 1) == 0) continue;     // ragged broken top
      if (!on) {
        uint32_t h = (uint32_t)(x / 2 * 73856093u) ^ (uint32_t)(y / 2 * 19349663u) ^ (genome * 2654435761u);
        switch (style) {
          case 1:  on = (((x + y) & 3) == 0) || (((x - y + 64) & 3) == 0); break;                       // scales
          case 2: { int wave = (int)(2.0f * sinf(y * 0.55f)); on = (((x + wave + 40) % 5) == 0) && y > EGG_H / 4; break; }   // flames
          case 3:  on = ((h >> 9) % 7) == 0; break;
          default: on = ((h >> 9) % 5) == 0; break;
        }
      }
      int px = x + shift;
      if (on && px >= 0 && px < EGG_CW) bits[y * stride + (px >> 3)] |= 0x80 >> (px & 7);
    }
  }
  if (style == 3 && !lowerOnly) {                      // the question mark, over a cleared patch
    int ox = EGG_W / 2 - 2 + 6 + (int)lroundf(tilt * 0.5f), oy = EGG_H / 2 - 4;
    for (int j = -1; j < 8; j++) for (int i = -1; i < 6; i++) {
      int px = ox + i, py = oy + j;
      if (px >= 0 && px < EGG_CW && py >= 0 && py < EGG_H) bits[py * stride + (px >> 3)] &= ~(0x80 >> (px & 7));
    }
    for (int j = 0; j < 7; j++) for (int i = 0; i < 5; i++)
      if (egg_q_bits[j] & (0x80 >> i)) { int px = ox + i, py = oy + j; bits[py * stride + (px >> 3)] |= 0x80 >> (px & 7); }
  }
  if (!lowerOnly) {                                    // cracks (gaps in the shell)
    struct Crack { int8_t x, y; } c1[] = { {0,5},{1,7},{-1,9},{1,11},{-1,13},{1,15},{0,17} };
    struct Crack2 { int8_t x, y; } c2[] = { {5,11},{3,13},{4,15},{2,17},{3,19},{1,21} };
    struct Crack3 { int8_t x, y; } c3[] = { {-6,13},{-4,15},{-5,17},{-3,19},{-4,22},{-2,24} };
    int sh0 = (int)lroundf(tilt * 0.5f) + 6 + EGG_W / 2;
    auto punch = [&](int x, int y) {
      if (x >= 0 && x < EGG_CW && y >= 0 && y < EGG_H) bits[y * stride + (x >> 3)] &= ~(0x80 >> (x & 7));
    };
    if (progress > 350) for (auto& c : c1) punch(sh0 + c.x, c.y);
    if (progress > 650) for (auto& c : c2) punch(sh0 + c.x, c.y);
    if (progress > 880) for (auto& c : c3) punch(sh0 + c.x, c.y);
    if (progress > 960) for (int y = 3; y < 9; y++) { punch(sh0 + (y & 1) - 3, y); punch(sh0 + 3 - (y & 1), y); }
  }
}

static inline int eggProgress(const Wolf& w) {
  uint32_t e = WolfGame::eggSecs(w.genome);
  if (!e) return 1000;
  uint64_t p = (uint64_t)w.age_secs * 1000u / e;
  return p > 1000 ? 1000 : (int)p;
}

// the whole egg scene (hud is drawn by the caller)
static inline void eggDraw(DisplayDriver& d, const Wolf& w, uint8_t style, uint32_t now, int ground_y, int screen_w) {
  uint8_t bits[((EGG_CW + 7) / 8) * EGG_H];
  int p = eggProgress(w);
  eggBuild(bits, w.genome, style, p, now, false);
  int x = screen_w / 2 - EGG_CW / 2;
  int hop = (p > 960 && ((now / 120) % 9) == 0) ? 2 : 0;
  // a little nest of straw
  for (int i = 0; i < 9; i++) {
    uint32_t h = (uint32_t)((i + 1) * 2654435761u);
    int sx = screen_w / 2 - 20 + (int)((h >> 5) % 40);
    int len = 3 + (int)((h >> 13) % 5);
    d.fillRect(sx, ground_y - 1 - (int)((h >> 20) % 2), len, 1);
  }
  d.drawXbm(x, ground_y - EGG_H - hop + 1, bits, EGG_CW, EGG_H);
}

// the empty shell, left on the ground for a while after hatching
static inline void eggShell(DisplayDriver& d, const Wolf& w, uint8_t style, int ground_y, int screen_w, uint32_t since_ms) {
  uint8_t bits[((EGG_CW + 7) / 8) * EGG_H];
  eggBuild(bits, w.genome, style, 1000, 0, true);
  int x = screen_w / 2 - EGG_CW / 2;
  d.drawXbm(x, ground_y - EGG_H + 1, bits, EGG_CW, EGG_H);
  // two shards that flew off
  if (since_ms < 1400) {
    int h = (int)(since_ms * 12 / 1400);
    d.fillRect(x - 3 - (int)(since_ms / 150), ground_y - 14 - h + (int)(since_ms / 90), 2, 2);
    d.fillRect(x + EGG_CW + 2 + (int)(since_ms / 150), ground_y - 12 - h + (int)(since_ms / 100), 2, 2);
  }
}
