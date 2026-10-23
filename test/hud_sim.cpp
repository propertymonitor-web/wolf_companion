// The top line of the pet screen must never run into the battery symbol:
//   g++ -std=c++17 -I src -I <MeshCore>/src -I test [-DPET_xxx=1] test/hud_sim.cpp src/WolfCore.cpp -o hud_sim
#include "mock.h"
#include "PetHud.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("  (line %d)\n", __LINE__); fails++; } } while (0)

int main() {
  MockDisplay d;
  MockHost h(5, 0x12345678u, 1); WolfGame g(&h); g.begin(0);
  const char* names[] = { "", "A", "Rex", "Wolfgan", "WWWWWWW", "Mr Fluf", "iiiiiii" };
  int shown_full = 0, shown_total = 0;
  for (uint8_t sp = 0; sp < WOLF_SPECIES_COUNT; sp++) for (const char* nm : names) for (int preg = 0; preg < 2; preg++) for (int count = 1; count <= 6; count += 5)
    for (int months : { 3, 14, 99, 999 }) for (int egg = 0; egg < 2; egg++) {
      Wolf w; memset(&w, 0, sizeof(w)); w.genome = 0x1234; w.sex = 1; w.kind = (uint8_t)(sp + 1);
      strncpy(w.name, nm, 7);
      w.age_secs = egg ? 3600u * 5 : WolfGame::eggSecs(w.genome) + (uint32_t)months * WOLF_SECS_PER_MONTH;
      w.preg_secs_left = preg ? 100 : 0;
      g.count = (uint8_t)count; g.current = (uint8_t)(count - 1);
      char out[48];
      int limit = 128 - 16 - 3 - (preg ? 12 : 0);
      petHudText(d, g, w, limit, out, sizeof(out));
      CHECK((int)strlen(out) * 6 <= limit, "'%s' is too wide (%d px, limit %d) for %s / %s", out, (int)strlen(out) * 6, limit, petTitleOf(sp), nm);
      CHECK(strlen(out) > 0, "empty hud text");
      if (!egg && *nm && !preg && count == 1 && sp != 4) { shown_total++; if (strstr(out, nm) && strstr(out, petTitleOf(sp))) shown_full++; }
    }
  Wolf w; memset(&w, 0, sizeof(w)); w.genome = 7; w.sex = 0; w.age_secs = WolfGame::eggSecs(7) + 14u * WOLF_SECS_PER_MONTH; strcpy(w.name, "Rex");
  g.count = 1; g.current = 0; char out[48];
  petHudText(d, g, w, 109, out, sizeof(out));
  printf("  %s\n", out);
  CHECK(!strcmp(out, "M14mo Rex Wolf"), "the usual line is 'M14mo Rex Wolf' (got '%s')", out);
  printf("  name and kind both fit %d of %d times\n", shown_full, shown_total);
  printf(fails ? "SOME CHECKS FAILED\n" : "all checks passed\n");
  return fails ? 1 : 0;
}
