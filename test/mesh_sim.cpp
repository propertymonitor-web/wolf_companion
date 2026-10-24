// g++ -std=c++17 -I src test/mesh_sim.cpp -o mesh_sim
#include "MeshScope.h"
#include <stdio.h>
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("  (line %d)\n", __LINE__); fails++; } } while (0)
int main() {
  meshscope::Scope s; char out[2400];
  CHECK(s.line(1000, out, sizeof out, -110, 10, 250.0f, 5, false) == 0, "nothing to say when nothing was heard");
  CHECK(s.line(1000, out, sizeof out, -110, 10, 250.0f, 5, true) > 0, "a forced line goes out when quiet");
  uint8_t path[3] = {0xAB, 0xCD, 0xEF}; uint8_t adv[40] = {0x12, 0x34, 0x56};
  s.onRx(2000, -97.0f, 6.25f, 4, 1, 0, 1, path, adv, 40, 120);
  s.onRx(2100, -118.0f, -9.5f, 2, 1, 2, 1, path, adv, 30, 60);
  s.onTx(2200, 5, 1, 50);
  size_t n = s.line(2500, out, sizeof out, -111, 10, 250.0f, 5, false);
  CHECK(n > 0 && out[n - 1] == '}', "line made");
  printf("%s\n", out);
  CHECK(strstr(out, "[500,-97,25,4,0,120,1,0,\"123456\",\"12\"]"), "advert record");
  CHECK(strstr(out, "[400,-118,-38,2,2,60,1,0,\"cd\",\"34\"]"), "text via repeater cd");
  CHECK(strstr(out, "[300,0,0,5,0,50,1,1,\"\",\"\"]"), "tx record");
  CHECK(s.line(2600, out, sizeof out, -111, 10, 250.0f, 5, false) == 0, "records are sent once");
  for (int i = 0; i < 500; i++) s.onRx(3000 + i, -100, 5, 2, 1, 1, 1, path, adv, 30, 40);   // burst bigger than the ring
  n = s.line(4000, out, sizeof out, -111, 10, 250.0f, 5, false);
  CHECK(n > 0 && n < sizeof out, "burst fits (%zu)", n);
  char small[300]; s.sent = 0; n = s.line(4000, small, sizeof small, -111, 10, 250.0f, 5, false);
  CHECK(n > 0 && n < sizeof small && s.sent < s.head, "small buffer: partial, rest next time (%zu)", n);
  int guard = 0; while (s.sent < s.head && guard++ < 100) s.line(4000, small, sizeof small, -111, 10, 250.0f, 5, false);
  CHECK(s.sent == s.head, "everything gets out eventually");
  printf(fails ? "SOME CHECKS FAILED\n" : "all checks passed\n");
  return fails ? 1 : 0;
}
