// TACTICAL RADAR for the 128x64 OLED: a sweeping radar of the repeaters, companions and room servers this radio hears.
// Header only, no hardware calls (tested on the PC).  Shown instead of the pet while TACTICAL mode is on; the pet keeps
// ageing in the background (the game clock runs from WolfModule::loop(), not from the screen).
#pragma once
#include <helpers/ui/DisplayDriver.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace tac {

static const uint32_t PERIOD_MS = 6500;        // one turn of the sweep
static const uint32_t WINDOW_MS = 900000;      // a contact not heard for 15 minutes leaves the radar
static const int TAC_MAXC = 48;

struct Contact {
  uint8_t id;          // stable slot number (so the radar can remember when the sweep last lit it)
  uint8_t type;        // 1 companion, 2 repeater, 3 room server, 4 sensor
  uint8_t k0;          // first byte of the public key (the Mesh Analyzer sees this byte as the sender)
  int8_t  snr;         // dB
  bool    has_pos;     // the node announced a position
  int32_t lat_e6, lon_e6;
  uint32_t age_ms;     // how long ago it was heard
};
struct Self { bool fix; int32_t lat_e6, lon_e6; };

struct Radar {
  bool     on = false;                 // TACTICAL mode (persisted by the module)
  uint32_t lit[TAC_MAXC];          // when the sweep last passed each contact (0 = not yet)
  uint8_t  litk[TAC_MAXC];
  float    prev = 0;
  float    range_m = 1000;
  uint8_t  ping_src[16]; uint32_t ping_ms[16]; uint8_t ping_n = 0;   // recent Mesh Analyzer packets (sender byte, time)
  int      shown_rep = 0, shown_cmp = 0, shown_room = 0;

  Radar() { memset(lit, 0, sizeof(lit)); memset(litk, 0, sizeof(litk)); memset(ping_src, 0, sizeof(ping_src)); memset(ping_ms, 0, sizeof(ping_ms)); }

  // the Mesh Analyzer heard a packet whose sender byte is `src`: that contact counts as heard right now
  void onPacket(uint8_t type, const uint8_t* payload, int len, uint32_t now) {
    if (!payload || len < 2) return;
    uint8_t src;
    if (type == 4) src = payload[0];
    else if (type == 0 || type == 1 || type == 2 || type == 8) src = payload[1];
    else return;
    ping_src[ping_n & 15] = src; ping_ms[ping_n & 15] = now | 1u; ping_n++;
  }

  static void px(DisplayDriver& d, int x, int y) { if (x >= 0 && x < 128 && y >= 0 && y < 64) d.fillRect(x, y, 1, 1); }
  static void line(DisplayDriver& d, int x0, int y0, int x1, int y1, int every = 1) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1, sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx - dy, k = 0;
    for (;;) { if (k++ % every == 0) px(d, x0, y0); if (x0 == x1 && y0 == y1) break; int e2 = 2 * e; if (e2 > -dy) { e -= dy; x0 += sx; } if (e2 < dx) { e += dx; y0 += sy; } }
  }
  static void circle(DisplayDriver& d, int cx, int cy, int r, int every) {
    int n = r * 8; for (int i = 0; i < n; i += every) { float a = (float)i / n * 6.2831853f; px(d, cx + (int)lroundf(sinf(a) * r), cy - (int)lroundf(cosf(a) * r)); }
  }
  static void shape(DisplayDriver& d, uint8_t type, int x, int y, int stage) {
    // stage 0 just lit (big, ringed), 1 solid, 2 hollow, 3 a single dot
    if (stage == 3) { px(d, x, y); return; }
    int s = stage == 0 ? 2 : 1;
    if (type == 2) {                                        // repeater: round
      if (stage == 2) { px(d, x - 1, y); px(d, x + 1, y); px(d, x, y - 1); px(d, x, y + 1); }
      else if (stage == 1) { d.fillRect(x - 1, y - 1, 3, 3); }
      else { d.fillRect(x - 2, y - 1, 5, 3); d.fillRect(x - 1, y - 2, 3, 5); }
    } else if (type == 1) {                                 // companion: triangle
      if (stage == 2) { px(d, x, y - 1); px(d, x - 1, y + 1); px(d, x + 1, y + 1); px(d, x, y + 1); }
      else for (int i = -s; i <= s; i++) { int h = s + 1 - (i < 0 ? -i : i); d.fillRect(x + i, y + s - h + 1, 1, h); }
    } else {                                                // room server / sensor: diamond
      if (stage == 2) { px(d, x, y - 1); px(d, x - 1, y); px(d, x + 1, y); px(d, x, y + 1); }
      else for (int j = -s; j <= s; j++) { int w = s - (j < 0 ? -j : j); d.fillRect(x - w, y + j, 2 * w + 1, 1); }
    }
    if (stage == 0) { circle(d, x, y, 4, 2); }
  }

  // draws one frame; returns milliseconds until the next one
  int render(DisplayDriver& d, uint32_t now, const Contact* c, int n, const Self& me) {
    d.startFrame();
    d.setTextSize(1);
    d.setColor(1);
    const int cx = 33, cy = 35, R = 26;
    // face: outer ring, half and quarter rings, spokes, ticks, N
    circle(d, cx, cy, R, 1); circle(d, cx, cy, R / 2, 2); circle(d, cx, cy, R / 4, 3);
    for (int a = 0; a < 360; a += 90) { float r = a * 0.0174533f; line(d, cx + (int)lroundf(sinf(r) * 3), cy - (int)lroundf(cosf(r) * 3), cx + (int)lroundf(sinf(r) * R), cy - (int)lroundf(cosf(r) * R), 3); }
    for (int a = 0; a < 360; a += 30) { float r = a * 0.0174533f; px(d, cx + (int)lroundf(sinf(r) * (R + 2)), cy - (int)lroundf(cosf(r) * (R + 2))); }
    d.setCursor(cx - 2, 0); d.print("N");   // (the ring starts 9 pixels below)

    // which contacts count, and where they go
    float far_m = 0; int rep = 0, cmp = 0, room = 0;
    static float ang[TAC_MAXC], frac[TAC_MAXC], dist[TAC_MAXC]; static bool ok[TAC_MAXC];
    float coslat = me.fix ? cosf((float)me.lat_e6 * 1e-6f * 0.0174533f) : 1.0f;
    for (int i = 0; i < n && i < TAC_MAXC; i++) {
      uint32_t age = c[i].age_ms;
      for (int k = 0; k < 16; k++) if (ping_ms[k] && ping_src[k] == c[i].k0 && (uint32_t)(now - ping_ms[k]) < age) age = now - ping_ms[k];
      ok[i] = age <= WINDOW_MS && c[i].type >= 1 && c[i].id < TAC_MAXC;
      if (!ok[i]) continue;
      if (c[i].type == 2) rep++; else if (c[i].type == 1) cmp++; else room++;
      if (me.fix && c[i].has_pos) {
        float dy = (float)(c[i].lat_e6 - me.lat_e6) * 1e-6f * 111320.0f, dx = (float)(c[i].lon_e6 - me.lon_e6) * 1e-6f * 111320.0f * coslat;
        dist[i] = sqrtf(dx * dx + dy * dy); ang[i] = atan2f(dx, dy); if (ang[i] < 0) ang[i] += 6.2831853f;
        if (dist[i] > far_m) far_m = dist[i];
      } else {
        dist[i] = -1; uint32_t h = (uint32_t)c[i].k0 * 2654435761u + (uint32_t)c[i].id * 40503u; ang[i] = (float)(h >> 8 & 0xFFFF) / 65536.0f * 6.2831853f;
      }
    }
    static const float RINGS[] = { 250, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000 };
    float want = 1000; if (far_m > 0) { want = 500000; for (float r : RINGS) if (r >= far_m * 1.08f) { want = r; break; } }
    range_m += (want - range_m) * 0.1f;
    for (int i = 0; i < n && i < TAC_MAXC; i++) {
      if (!ok[i]) continue;
      float fr = dist[i] >= 0 ? dist[i] / range_m : 0.9f - 0.75f * (c[i].snr < -20 ? 0 : c[i].snr > 15 ? 1 : (c[i].snr + 20) / 35.0f);
      frac[i] = fr > 0.95f ? 0.95f : fr;
    }
    shown_rep = rep; shown_cmp = cmp; shown_room = room;

    // sweep: a bright line with a thinning trail
    float sig = (float)(now % PERIOD_MS) / PERIOD_MS * 6.2831853f, was = prev; prev = sig;
    for (int t = 4; t >= 0; t--) {
      float a = sig - t * 0.12f;
      line(d, cx, cy, cx + (int)lroundf(sinf(a) * (R - 1)), cy - (int)lroundf(cosf(a) * (R - 1)), t == 0 ? 1 : (t < 3 ? 2 : 4));
    }

    // contacts: lit when the sweep crosses them, then they fade until the next pass
    for (int i = 0; i < n && i < TAC_MAXC; i++) {
      if (!ok[i]) { if (c[i].id < TAC_MAXC) lit[c[i].id] = 0; continue; }           // no longer heard: gone
      uint8_t id = c[i].id;
      if (litk[id] != c[i].k0) { litk[id] = c[i].k0; lit[id] = 0; }
      float a = ang[i];
      bool crossed = was <= sig ? (was <= a && a < sig) : (a >= was || a < sig);
      if (crossed) lit[id] = now | 1u;
      if (!lit[id]) continue;
      float f = (float)(uint32_t)(now - lit[id]) / PERIOD_MS;
      int stage = f < 0.14f ? 0 : f < 0.5f ? 1 : f < 0.85f ? 2 : f < 1.0f ? 3 : -1;
      if (stage < 0) continue;
      int x = cx + (int)lroundf(sinf(a) * frac[i] * R), y = cy - (int)lroundf(cosf(a) * frac[i] * R);
      shape(d, c[i].type, x, y, stage);
    }
    d.fillRect(cx - 1, cy - 1, 3, 3);                                                  // this radio, in the middle

    // right-hand readout
    char t[24];
    d.setCursor(70, 0); d.print("TACTICAL");
    d.fillRect(70, 9, 56, 1);
    snprintf(t, sizeof(t), "REP  %d", rep);  d.setCursor(70, 13); d.print(t);
    snprintf(t, sizeof(t), "CMP  %d", cmp);  d.setCursor(70, 23); d.print(t);
    snprintf(t, sizeof(t), "ROOM %d", room); d.setCursor(70, 33); d.print(t);
    if (range_m < 1000) snprintf(t, sizeof(t), "RNG %dm", (int)(range_m + 0.5f)); else snprintf(t, sizeof(t), "RNG %dkm", (int)(range_m / 1000 + 0.5f));
    d.setCursor(70, 44); d.print(t);
    d.setCursor(70, 55); d.print(me.fix ? "GPS FIX" : "NO FIX");
    return 70;
  }
};

}  // namespace tac
