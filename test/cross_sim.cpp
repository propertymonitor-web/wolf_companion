// Cross-species visiting and breeding rules (core only, no pictures):
//   g++ -std=c++17 -I src -I <MeshCore>/src -I test [-DPET_FOX=1 | -DPET_MYSTERY=1] test/cross_sim.cpp src/WolfCore.cpp -o cross_sim
#include "mock.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("  (line %d)\n", __LINE__); fails++; } } while (0)

static uint32_t g_serial = 1;

// a female, grown-up radio whose wolf is ready to mate with anyone it has met
struct Home {
  MockHost h; WolfGame g;
  Home(uint32_t tail) : h(77 + tail, tail, 0x11111111u), g(&h) { g.begin(0); }
  void ready(uint32_t mate_genome) {
    Wolf& w = g.pack[0];
    w.sex = 1; w.preg_secs_left = 0; w.cooldown_secs = 0; w.visits_needed = WOLF_CONSENT_MIN;
    w.age_secs = WolfGame::eggSecs(w.genome) + 40u * WOLF_SECS_PER_MONTH;
    memset(g.mem, 0, sizeof(g.mem));
    g.mem[0].mine = w.genome; g.mem[0].other = mate_genome; g.mem[0].visits = 200;
    g.visitor.active = false;
  }
};

// an adult male of species `sp` asks to visit; returns the packet we answered with (empty if none)
static std::vector<uint8_t> visit(Home& hm, uint8_t sp, uint32_t genome, uint32_t now) {
  uint8_t p[40]; memset(p, 0, sizeof(p));
  p[0] = petPktType(sp) | WOLF_PKT_VISIT; p[1] = petPktVer(sp);
  uint32_t pre = 0x22000000u + (g_serial++);              // a different radio every time, so nothing counts as "met recently"
  memcpy(&p[2], &pre, 4); memcpy(&p[6], &genome, 4);
  p[10] = 2;                                               // male, adult
  p[11] = 100;
  p[21] = 1; memcpy(&p[22], &hm.g.pack[0].genome, 4);      // he has consented to her
  hm.h.out.clear();
  hm.g.onPacket(p, 26, now);
  std::vector<uint8_t> ack;
  for (uint32_t t = now; t < now + 2500 && ack.empty(); t += 100) {
    hm.g.tick(t);
    for (auto& q : hm.h.out) if ((q[0] & 0x0F) == WOLF_PKT_ACK) { ack = q; break; }
  }
  return ack;
}

int main() {
  uint32_t now = 100000;
  printf("== species table ==\n");
  for (uint8_t a = 0; a < WOLF_SPECIES_COUNT; a++) {
    CHECK(petSpeciesFromPkt(petPktType(a), petPktVer(a)) == a, "%s round-trips through its packet family", petNameOf(a));
    for (uint8_t b = a + 1; b < WOLF_SPECIES_COUNT; b++) CHECK(!(petPktType(a) == petPktType(b) && petPktVer(a) == petPktVer(b)), "%s and %s share a packet family", petNameOf(a), petNameOf(b));
  }
  CHECK(petSpeciesFromPkt(0xF0, 99) == 0xFF && petSpeciesFromPkt(0x90, 2) == 0xFF, "unknown packet families are ignored");

  printf("== every kind of pet may visit; only the same kind breeds ==\n");
  uint32_t tail = 0x13572468u;
  for (uint8_t sp = 0; sp < WOLF_SPECIES_COUNT; sp++) {
    Home hm(tail);
    uint8_t mine = WolfGame::speciesOf(hm.g.pack[0]);
    uint32_t mate = 0xABCD0000u + sp;
    int visits_ok = 0, pregnant = 0;
    const int N = 120;
    for (int i = 0; i < N; i++, now += 600000) {
      hm.ready(mate);
      auto ack = visit(hm, sp, mate, now);
      if (!ack.empty() && hm.g.visitor.active && hm.g.visitor.species == sp) visits_ok++;
      if (!ack.empty()) CHECK(petSpeciesFromPkt(ack[0] & 0xF0, ack[1]) == mine, "our answer carries our own family");
      if (hm.g.pack[0].preg_secs_left) pregnant++;
    }
    bool same = sp == mine;
#if PET_MYST
    bool maybe = same || petHybrid(mine, sp) != 0xFF || WolfGame::chimeraOk(mine, sp);
#else
    bool maybe = same;
#endif
    CHECK(visits_ok == N, "a %s visiting a %s radio is welcome (%d of %d)", petNameOf(sp), petNameOf(mine), visits_ok, N);
    if (same) CHECK(pregnant > N / 3, "same kind (%s) breeds: %d of %d", petNameOf(sp), pregnant, N);
    else if (!maybe) CHECK(pregnant == 0, "%s + %s must never breed (%d)", petNameOf(mine), petNameOf(sp), pregnant);
    printf("  %-10s visiting %-10s: visits %3d/%d, litters %3d\n", petNameOf(sp), petNameOf(mine), visits_ok, N, pregnant);
  }

#if PET_MYST
  printf("== the secret hybrids (mystery build only) ==\n");
  CHECK(petHybrid(1, 2) == 11 && petHybrid(2, 1) == 11, "fox + cat -> kitsune, either way round");
  int pairs = 0;
  for (uint8_t a = 0; a < WOLF_SPECIES_COUNT; a++) for (uint8_t b = 0; b < WOLF_SPECIES_COUNT; b++) {
    CHECK(petHybrid(a, b) == petHybrid(b, a), "hybrid table is symmetric");
    if (a < b && petHybrid(a, b) != 0xFF) pairs++;
    CHECK(a != b ? true : petHybrid(a, b) == 0xFF, "a kind never hybridises with itself");
  }
  printf("  %d hybrid pairs\n", pairs);
  {
    int made = 0, tried = 0;
    for (uint8_t body = 0; body < WOLF_SPECIES_COUNT; body++) for (uint8_t head = 0; head < WOLF_SPECIES_COUNT; head++) {
      if (!WolfGame::chimeraOk(body, head) || petHybrid(body, head) != 0xFF || body == 14) continue;   // horse is not in the egg pool
      uint32_t t2 = 0; for (uint32_t t = 1;; t++) if (petPickSpecies(t * 2654435761u) == body) { t2 = t * 2654435761u; break; }
      Home hm(t2);
      uint32_t mate = 0xC0DE0000u + head;
      tried++;
      for (int i = 0; i < 600; i++, now += 600000) {
        hm.ready(mate); visit(hm, head, mate, now);
        if (hm.g.pack[0].preg_secs_left) {
          uint8_t k = hm.g.pack[0].child_kind;
          CHECK(k >= 16 && WolfGame::kindValid(k), "chimera kind %d valid", k);
          CHECK(k == WolfGame::chimeraKind(body, head), "kind code");
          made++; break;
        }
      }
    }
    CHECK(made == tried && tried > 100, "every mixable pair can make a new species (%d of %d)", made, tried);
  }
  for (int pair = 0; pair < 14 * WOLF_SPECIES_COUNT; pair++) {
    uint8_t hh = (uint8_t)(pair / WOLF_SPECIES_COUNT), other = (uint8_t)(pair % WOLF_SPECIES_COUNT);
    // find an identity of the right kind of mystery pet
    uint32_t t2 = 0; for (uint32_t t = 1;; t++) if (petPickSpecies(t * 2654435761u) == hh) { t2 = t * 2654435761u; break; }
    Home hm(t2);
    uint8_t mine = WolfGame::speciesOf(hm.g.pack[0]);
    uint8_t hy = petHybrid(mine, other);
    if (hy == 0xFF || other == mine) continue;
    uint32_t mate = 0xFEED0000u + other;
    int got = 0; uint8_t kid = 0xFF;
    for (int i = 0; i < 400 && !got; i++, now += 600000) {
      hm.ready(mate);
      visit(hm, other, mate, now);
      if (hm.g.pack[0].preg_secs_left) { got = 1; kid = hm.g.pack[0].child_kind; }
    }
    CHECK(got && kid == hy + 1, "%s + %s -> %s expected, child_kind %d", petNameOf(mine), petNameOf(other), petNameOf(hy), kid);
    // save while pregnant, reload, give birth
    hm.g.save();
    MockHost h2(5, t2, 0x11111111u); h2.saved = hm.h.saved; h2.backup = hm.h.backup;
    WolfGame g2(&h2); g2.begin(0);
    CHECK(g2.pack[0].child_kind == kid && g2.pack[0].preg_secs_left, "pregnancy keeps its kind through a reboot");
    g2.pack[0].preg_secs_left = 2;
    for (int s = 0; s < 5; s++) g2.tick(now + 1000 * (s + 1));
    CHECK(g2.count >= 2, "young were born");
    if (g2.count >= 2) {
      CHECK(WolfGame::speciesOf(g2.pack[1]) == hy, "the young are a %s (got %s)", petNameOf(hy), petNameOf(WolfGame::speciesOf(g2.pack[1])));
      CHECK(WolfGame::isEgg(g2.pack[1]), "the new kind starts as an egg");
    }
    printf("  %s + %s -> %s ok\n", petNameOf(mine), petNameOf(other), petNameOf(hy));
  }
  printf("== saves ==\n");
  {
    uint32_t t3 = 0x0F0F1234u;
    MockHost h(9, t3, 1); WolfGame g(&h); g.begin(0);
    Wolf c; memset(&c, 0, sizeof(c)); c.genome = 0xCAFE0001; c.kind = 12; c.age_secs = 5; c.visits_needed = 4; g.pack[g.count++] = c;
    g.save();
    MockHost h2(10, t3, 1); h2.saved = h.saved; WolfGame g2(&h2); g2.begin(0);
    CHECK(g2.count == 2 && WolfGame::speciesOf(g2.pack[1]) == 11 && g2.pack[1].kind == 12, "a pet's kind is saved");
    // an old (version 2) file must still load, with every pet the household's own kind
    std::vector<uint8_t> v2; v2.assign(h.saved.begin(), h.saved.begin() + 6); v2[3] = 2;
    const uint8_t* p = h.saved.data() + 6;
    for (int i = 0; i < g.count; i++) { v2.insert(v2.end(), p, p + 36); p += 46; }
    v2.insert(v2.end(), p, (const uint8_t*)(h.saved.data() + h.saved.size()));
    MockHost h3(11, t3, 1); h3.saved = v2; h3.backup.clear(); WolfGame g3(&h3); g3.begin(0);
    CHECK(g3.count == 2 && g3.pack[0].genome == g.pack[0].genome && g3.pack[1].genome == 0xCAFE0001, "old save files still load");
    CHECK(g3.pack[1].kind == 0 && WolfGame::speciesOf(g3.pack[1]) == g_mystery_sp, "old pets keep the household's kind");
  }
#endif
  printf("== names ==\n");
  {
    MockHost h(21, 0x00C0FFEEu, 1); WolfGame g(&h); g.begin(0);
    Wolf c; memset(&c, 0, sizeof(c)); c.genome = 0xBEEF0002; c.visits_needed = 4; g.pack[g.count++] = c;
    struct { const char* in; const char* want; } T[] = {
      {"Rex", "Rex"}, {"  Rex  ", "Rex"}, {"Wolfgang", "Wolfgan"}, {"Mr Fluff", "Mr Fluf"}, {"a\"b\\c", "abc"}, {"", ""}, {"   ", ""},
      {"Sir <b>", "Sir b"}, {"Ro-ro_1.", "Ro-ro_1"}, {"Zoe!?", "Zoe!?"}, {"\x01\x02ok", "ok"} };
    for (auto& t : T) { g.setName(0, t.in); CHECK(!strcmp(g.pack[0].name, t.want), "name '%s' -> '%s' (wanted '%s')", t.in, g.pack[0].name, t.want); }
    CHECK(!g.setName(9, "x"), "no such pet");
    g.setName(0, "Rex"); g.setName(1, "Pup");
    g.save();
    MockHost h2(22, 0x00C0FFEEu, 1); h2.saved = h.saved; WolfGame g2(&h2); g2.begin(0);
    CHECK(g2.count == 2 && !strcmp(g2.pack[0].name, "Rex") && !strcmp(g2.pack[1].name, "Pup"), "names are saved");
    // a version 3 and a version 2 file (no names) still load and leave the names empty
    for (int ver = 3; ver >= 2; ver--) {
      std::vector<uint8_t> old(h.saved.begin(), h.saved.begin() + 6); old[3] = (uint8_t)ver;
      const uint8_t* q = h.saved.data() + 6;
      for (int i = 0; i < g.count; i++) { old.insert(old.end(), q, q + (ver == 3 ? 38 : 36)); q += 46; }
      old.insert(old.end(), q, (const uint8_t*)(h.saved.data() + h.saved.size()));
      MockHost h3(23, 0x00C0FFEEu, 1); h3.saved = old; h3.backup.clear(); WolfGame g3(&h3); g3.begin(0);
      CHECK(g3.count == 2 && g3.pack[1].genome == 0xBEEF0002 && g3.pack[0].name[0] == 0, "version %d files still load", ver);
    }
    // a damaged name in the file is cleaned up, not trusted
    std::vector<uint8_t> bad = h.saved; bad[6 + 36 + 2] = '"'; bad[6 + 36 + 3] = 'x';
    MockHost h4(24, 0x00C0FFEEu, 1); h4.saved = bad; h4.backup.clear(); WolfGame g4(&h4); g4.begin(0);
    CHECK(g4.count == 2 && g4.pack[0].name[0] == 0, "a name with a quote in it is dropped");
  }
  printf(fails ? "SOME CHECKS FAILED\n" : "all checks passed\n");
  return fails ? 1 : 0;
}
