// g++ -std=c++17 -I src -I <MeshCore>/src -I test test/tac_sim.cpp -o tac_sim
#include "mock.h"
#include "PetMirror.h"
#include "TacRadar.h"
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("  (line %d)\n", __LINE__); fails++; } } while (0)
static int lit_px(const PetMirror& m) { int n = 0; for (int i = 0; i < PM_BYTES; i++) for (int b = 0; b < 8; b++) if (m.fb[i] >> b & 1) n++; return n; }
static void dump(const PetMirror& m, const char* path) { FILE* f = fopen(path, "w"); fprintf(f, "P1\n128 64\n"); for (int y = 0; y < 64; y++) { for (int x = 0; x < 128; x++) fprintf(f, "%d ", (m.fb[y * 16 + (x >> 3)] >> (7 - (x & 7))) & 1); fprintf(f, "\n"); } fclose(f); }
int main() {
  tac::Radar r; PetMirror m;
  tac::Self me = { true, 29760000, -95370000 };
  tac::Contact c[5] = {
    { 0, 2, 0xA1, 6, true, 29770000, -95360000, 10000 },      // a repeater about 1.5 km NE
    { 1, 1, 0xB2, -2, false, 0, 0, 5000 },                    // a companion with no position
    { 2, 3, 0xC3, -8, true, 29750000, -95380000, 60000 },     // a room server SW
    { 3, 2, 0xD4, -10, true, 29800000, -95330000, 3600000 },  // heard an hour ago: must not show
    { 4, 2, 0xE5, 3, true, 29740000, -95340000, 20000 } };
  // run one full sweep, find the frame just after the beam passes contact 0 and the same contact near the end of the turn
  int frames = 0, maxpix = 0, minpix = 99999; char dumped = 0;
  for (uint32_t t = 1000; t < 1000 + 3 * tac::PERIOD_MS; t += 70, frames++) {
    int nxt = r.render(m, t, c, 5, me); CHECK(nxt > 0 && nxt <= 100, "frame delay %d", nxt);
    int p = lit_px(m); if (p > maxpix) maxpix = p; if (p < minpix) minpix = p;
    if (!dumped && t > 1000 + 2 * tac::PERIOD_MS + 3000) { dump(m, "/tmp/tac_frame.pbm"); dumped = 1; }
  }
  CHECK(r.shown_rep == 2 && r.shown_cmp == 1 && r.shown_room == 1, "counts rep %d cmp %d room %d (the hour-old repeater must not count)", r.shown_rep, r.shown_cmp, r.shown_room);
  CHECK(maxpix < 1400 && minpix > 300, "pixel counts sane (%d..%d)", minpix, maxpix);
  // a contact that is no longer heard disappears: age everything past the window
  for (auto& x : c) x.age_ms = 2000000;
  r.render(m, 99000, c, 5, me); CHECK(r.shown_rep == 0 && r.shown_cmp == 0, "all gone when nothing is heard");
  // a Mesh Analyzer packet from a contact makes it heard now
  uint8_t pl[4] = { 0, 0xB2, 0, 0 }; r.onPacket(2, pl, 4, 100000);
  r.render(m, 100500, c, 5, me); CHECK(r.shown_cmp == 1, "a packet from companion B2 brings it back");
  // a lit contact fades and is gone by the next pass if not re-lit; stage ordering
  printf("%d frames, %d..%d lit pixels\n", frames, minpix, maxpix);
  printf(fails ? "SOME CHECKS FAILED\n" : "all checks passed\n");
  return fails ? 1 : 0;
}
