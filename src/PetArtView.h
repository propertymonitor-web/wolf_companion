#pragma once
// Mixed species (mystery egg build): a "chimera" wears the body, legs and tail of one pet and the head and ears of
// another.  Every pet's art is compiled inside its own namespace, so the pictures are reached through these plain
// look-alike structures (the layouts are checked with static_asserts in PetFactory.inc).
#include <stdint.h>
#include <string.h>

struct PaPart { const uint8_t* bits; int8_t x, y; uint8_t w, h; };
struct PaKey { int8_t eye1x, eye1y, eye2x, eye2y, nosex, nosey, mx, my, mw, mh; };
struct PaStage {
  uint8_t cw, ch;
  PaPart body;
  PaPart legs[5];
  PaPart head[5];
  PaKey  key[5];
  PaPart ear[3][5];
  PaPart tail[3][2];
};
struct PaChimera {
  PaStage st[3];
  const PaStage* ptr[3];
};

// Build the mixed pictures.  Returns false (and leaves `out` unused) if the result would not fit the drawing buffers.
inline bool paBuildChimera(const PaStage* const* body, const PaStage* const* head, PaChimera& out, int max_cw, int max_ch) {
  for (int s = 0; s < 3; s++) {
    const PaStage& B = *body[s];
    const PaStage& H = *head[s];
    PaStage& O = out.st[s];
    O = B;
    // the neck joins the body at its front, top corner: move the other pet's head there
    const int dx = (B.body.x + B.body.w) - (H.body.x + H.body.w);
    const int dy = B.body.y - H.body.y;
    for (int p = 0; p < 5; p++) {
      O.head[p] = H.head[p]; O.head[p].x = (int8_t)(O.head[p].x + dx); O.head[p].y = (int8_t)(O.head[p].y + dy);
      O.key[p] = H.key[p];
      O.key[p].eye1x = (int8_t)(O.key[p].eye1x + dx); O.key[p].eye1y = (int8_t)(O.key[p].eye1y + dy);
      if (O.key[p].eye2x >= 0) { O.key[p].eye2x = (int8_t)(O.key[p].eye2x + dx); O.key[p].eye2y = (int8_t)(O.key[p].eye2y + dy); }
      O.key[p].nosex = (int8_t)(O.key[p].nosex + dx); O.key[p].nosey = (int8_t)(O.key[p].nosey + dy);
      O.key[p].mx = (int8_t)(O.key[p].mx + dx); O.key[p].my = (int8_t)(O.key[p].my + dy);
      for (int e = 0; e < 3; e++) { O.ear[e][p] = H.ear[e][p]; O.ear[e][p].x = (int8_t)(O.ear[e][p].x + dx); O.ear[e][p].y = (int8_t)(O.ear[e][p].y + dy); }
    }
    // make room: if anything now sticks out left or above, slide the whole picture right / down (the paws stay on the bottom row)
    int minx = 0, miny = 0, maxx = B.cw;
    auto grow = [&](const PaPart& q) {
      if (!q.w || !q.h) return;
      if (q.x < minx) minx = q.x;
      if (q.y < miny) miny = q.y;
      if (q.x + q.w > maxx) maxx = q.x + q.w;
    };
    grow(O.body);
    for (int p = 0; p < 5; p++) { grow(O.legs[p]); grow(O.head[p]); for (int e = 0; e < 3; e++) grow(O.ear[e][p]); }
    for (int t = 0; t < 3; t++) for (int f = 0; f < 2; f++) grow(O.tail[t][f]);
    const int sx = -minx, sy = -miny;
    const int cw = maxx + sx, ch = B.ch + sy;
    if (cw > max_cw || ch > max_ch) return false;
    if (sx || sy) {
      auto mv = [&](PaPart& q) { q.x = (int8_t)(q.x + sx); q.y = (int8_t)(q.y + sy); };
      mv(O.body);
      for (int p = 0; p < 5; p++) {
        mv(O.legs[p]); mv(O.head[p]);
        O.key[p].eye1x = (int8_t)(O.key[p].eye1x + sx); O.key[p].eye1y = (int8_t)(O.key[p].eye1y + sy);
        if (O.key[p].eye2x >= 0) { O.key[p].eye2x = (int8_t)(O.key[p].eye2x + sx); O.key[p].eye2y = (int8_t)(O.key[p].eye2y + sy); }
        O.key[p].nosex = (int8_t)(O.key[p].nosex + sx); O.key[p].nosey = (int8_t)(O.key[p].nosey + sy);
        O.key[p].mx = (int8_t)(O.key[p].mx + sx); O.key[p].my = (int8_t)(O.key[p].my + sy);
        for (int e = 0; e < 3; e++) mv(O.ear[e][p]);
      }
      for (int t = 0; t < 3; t++) for (int f = 0; f < 2; f++) mv(O.tail[t][f]);
    }
    O.cw = (uint8_t)cw; O.ch = (uint8_t)ch;
    out.ptr[s] = &out.st[s];
  }
  return true;
}
