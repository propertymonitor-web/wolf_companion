// Mystery egg build test:
//   g++ -std=c++17 -I src -I <MeshCore>/src -I test -DPET_MYSTERY=1 test/mystery_sim.cpp src/WolfCore.cpp src/MysteryFactory.cpp src/Mystery[A-Z]*.cpp -o mystery_sim
#include "mock.h"
#include "WolfScene.h"
#include "PetMirror.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("  (line %d)\n", __LINE__); fails++; } } while (0)

static uint32_t tailFor(uint8_t sp) {          // an identity that hatches into this species
  for (uint32_t t = 1;; t++) if (petPickSpecies(t * 2654435761u) == sp) return t * 2654435761u;
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  printf("== which pet is in the egg ==\n");
  int cnt[14] = {0};
  for (uint32_t i = 1; i <= 28000; i++) cnt[petPickSpecies(i * 0x9E3779B1u + 12345)]++;
  for (int i = 0; i < 14; i++) { printf("  %-10s %d\n", petNameOf(i), cnt[i]); CHECK(cnt[i] > 1700 && cnt[i] < 2300, "species %d about 1/14 of the time", i); }

  printf("== every pet: egg, hatch, live ==\n");
  for (uint8_t sp = 0; sp < 14; sp++) {
    uint32_t tail = tailFor(sp);
    MockHost h(1000 + sp, tail, 7);
    WolfGame g(&h);
    g.begin(0);
    CHECK(g_mystery_sp == sp, "species %d chosen from the identity (got %d)", sp, g_mystery_sp);
    PetSceneBase* sc = makePetScene(g_mystery_sp, &g);
    CHECK(sc != nullptr, "scene for %s", petNameOf(sp));
    MockDisplay d;
    // the egg, a few frames
    uint32_t now = 1000, ek = 0;
    for (int i = 0; i < 40; i++, now += 110) { d.startFrame(0); sc->render(d, now, 3900); for (int k = 0; k < 128 * 64; k++) ek += d.px[k]; }
    CHECK(WolfGame::isEgg(g.pack[0]) && ek > 200, "%s starts as a visible egg", petNameOf(sp));
    // it shows its first packet type only after hatching: age past the egg
    uint32_t eggs = WolfGame::eggSecs(g.pack[0].genome);
    g.pack[0].age_secs = eggs - 3;
    for (int i = 0; i < 60; i++, now += 110) { d.startFrame(0); sc->render(d, now, 3900); if (i == 20) g.pack[0].age_secs = eggs + 2; }
    CHECK(!WolfGame::isEgg(g.pack[0]), "%s hatched", petNameOf(sp));
    // the packet family is that species'
    h.out.clear();
    g.pack[0].age_secs = eggs + 40u * WOLF_SECS_PER_MONTH;     // an adult
    uint32_t t = 0; bool sent = false;
    for (int sec = 0; sec < 400 && !sent; sec++) { t += 1000; g.tick(t); sent = !h.out.empty(); }
    if (sent) {
      CHECK((h.out[0][0] & 0xF0) == petPktType(sp) && h.out[0][1] == petPktVer(sp), "%s sends its own packet family (%02X %02X)", petNameOf(sp), h.out[0][0], h.out[0][1]);
    } else CHECK(sp == 4 || false, "%s sent a visit request", petNameOf(sp));
    // a minute of normal life: every frame must draw something and nothing may crash
    long ink = 0; int blank = 0;
    for (int i = 0; i < 2000; i++, now += 110) {
      d.startFrame(0); sc->render(d, now, 3900);
      long k = 0; for (int q = 0; q < 128 * 64; q++) k += d.px[q];
      ink += k; if (k < 40) blank++;
    }
    CHECK(blank < 200, "%s: %d nearly blank frames out of 2000", petNameOf(sp), blank);
    printf("  %-10s ok (egg %.1f h)\n", petNameOf(sp), eggs / 3600.0);
    delete sc;
  }
  printf("== a hybrid-born horse-type scene and a species switch ==\n");
  {
    MockHost h(4242, tailFor(1), 7); WolfGame g(&h); g.begin(0);
    MockDisplay d; uint32_t now = 1000;
    for (uint8_t sp = 0; sp < WOLF_SPECIES_COUNT; sp++) {
      g.pack[0].kind = (uint8_t)(sp + 1);
      g.pack[0].age_secs = WolfGame::eggSecs(g.pack[0].genome) + 30u * WOLF_SECS_PER_MONTH;
      PetSceneBase* sc = makePetScene(WolfGame::speciesOf(g.pack[0]), &g);
      long ink = 0;
      for (int i = 0; i < 300; i++, now += 110) { d.startFrame(0); sc->render(d, now, 3900); for (int q = 0; q < 128 * 64; q++) ink += d.px[q]; }
      CHECK(ink > 300 * 40, "%s scene draws something when it is a pet of a mixed family", petNameOf(sp));
      delete sc;
    }
  }
  printf(fails ? "SOME CHECKS FAILED\n" : "all checks passed\n");
  return fails ? 1 : 0;
}
