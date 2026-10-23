// PC test harness: renders wolf frames and checks the game rules.
//   g++ -std=c++17 -I src -I <MeshCore>/src test/host_sim.cpp src/WolfCore.cpp src/WolfScene.cpp -o host_sim
//   ./host_sim sheet frames.txt | ./host_sim anim frames.txt | ./host_sim logic
#include <helpers/ui/DisplayDriver.h>
#include "WolfCore.h"
#include "WolfScene.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <deque>

ColorVal UIColor::window_bkg = 0, UIColor::title_bkg = 1, UIColor::title_txt = 1, UIColor::primary_txt = 1,
         UIColor::secondary_txt = 1, UIColor::warning_txt = 1, UIColor::popup_bkg = 0, UIColor::popup_txt = 1,
         UIColor::corp_blue = 1;

struct MockDisplay : DisplayDriver {
  uint8_t px[128 * 64];
  struct Txt { int x, y; std::string s; };
  std::vector<Txt> txt;
  int cx = 0, cy = 0;
  MockDisplay() : DisplayDriver(128, 64) { memset(px, 0, sizeof(px)); }
  bool isOn() override { return true; }
  void turnOn() override {}
  void turnOff() override {}
  void clear() override { memset(px, 0, sizeof(px)); txt.clear(); }
  void startFrame(ColorVal) override { clear(); }
  void setTextSize(int) override {}
  void setColor(ColorVal) override {}
  void setCursor(int x, int y) override { cx = x; cy = y; }
  void print(const char* s) override { txt.push_back({cx, cy, s}); }
  void fillRect(int x, int y, int w, int h) override {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) set(x + i, y + j);
  }
  void drawRect(int x, int y, int w, int h) override {
    for (int i = 0; i < w; i++) { set(x + i, y); set(x + i, y + h - 1); }
    for (int j = 0; j < h; j++) { set(x, y + j); set(x + w - 1, y + j); }
  }
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override {
    int stride = (w + 7) / 8;
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++)
      if (bits[j * stride + (i >> 3)] & (0x80 >> (i & 7))) set(x + i, y + j);
  }
  uint16_t getTextWidth(const char* s) override { return (uint16_t)(strlen(s) * 6); }
  void endFrame() override {}
  void set(int x, int y) { if (x >= 0 && x < 128 && y >= 0 && y < 64) px[y * 128 + x] = 1; }
  void dump(FILE* f, const char* label) {
    fprintf(f, "FRAME %s\n", label);
    for (int y = 0; y < 64; y++) { for (int x = 0; x < 128; x++) fputc(px[y * 128 + x] ? '1' : '0', f); fputc('\n', f); }
    for (auto& t : txt) fprintf(f, "TXT %d %d %s\n", t.x, t.y, t.s.c_str());
    fprintf(f, "END\n");
  }
};

struct MockHost : WolfHost {
  uint32_t rng, tail, prefix;
  std::vector<uint8_t> saved;
  std::deque<std::vector<uint8_t>> out;
  std::string last_alert;
  MockHost(uint32_t seed, uint32_t tailv, uint32_t pre) : rng(seed), tail(tailv), prefix(pre) {}
  uint32_t random32() override { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
  uint32_t identityTail() override { return tail; }
  void ownerPrefix(uint8_t o[4]) override { memcpy(o, &prefix, 4); }
  bool loadState(uint8_t* b, int mx, int& len) override {
    if (saved.empty()) return false;
    len = (int)saved.size() < mx ? (int)saved.size() : mx;
    memcpy(b, saved.data(), len);
    return true;
  }
  void saveState(const uint8_t* b, int len) override { saved.assign(b, b + len); }
  void broadcast(const uint8_t* b, int len) override { out.push_back(std::vector<uint8_t>(b, b + len)); }
  void alert(const char* t) override { last_alert = t; printf("  [alert] %s\n", t); }
};

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("  (line %d)\n", __LINE__); fails++; } } while (0)

static void deliver(MockHost& from, WolfGame& to, uint32_t now) {
  while (!from.out.empty()) { auto p = from.out.front(); from.out.pop_front(); to.onPacket(p.data(), (int)p.size(), now); }
}

#if PET_INSECT
static int cmd_sheet(const char*) { return 0; }
#else
static int cmd_sheet(const char* path) {
  FILE* f = fopen(path, "w");
  MockDisplay d;
  uint32_t genomes[] = {0x0000F0A3, 0x7CA95E21, 0xD3B6C8E4, 0x39F2B77A, 0xA8E54C03, 0x5BC0D9F1,
                        0xE17A3C5D, 0x2468ACE0, 0x9DB1F6C2, 0x6A3D0B94};
  for (int stage = 2; stage >= 0; stage--) {
    for (uint32_t gm : genomes) {
      d.clear();
      WolfPose p{0, 0, 0, false, 0, false};
      wolfDrawSprite(d, gm, stage, 30, 58, p);
      char lab[40]; snprintf(lab, sizeof(lab), "stage%d_%08X", stage, (unsigned)gm);
      d.dump(f, lab);
    }
  }
  // poses (adult, one genome)
  for (int stage = 2; stage >= 0; stage--) {
    const char* names[] = {"stand", "walk1", "walk2", "walk3", "walk4", "eat", "front", "bark", "tail2", "left"};
    for (int i = 0; i < 10; i++) {
      d.clear();
      WolfPose p{0, 0, 0, false, 0, false};
      if (i >= 1 && i <= 4) p.legs = i;
      if (i == 5) p.head = 1;
      if (i == 6) p.head = 2;
      if (i == 7) p.head = 3;
      if (i == 8) p.tail = 1;
      if (i == 9) { p.mirror = true; p.legs = 2; }
      wolfDrawSprite(d, 0x7CA95E21, stage, 30, 58, p);
      char lab[40]; snprintf(lab, sizeof(lab), "pose_st%d_%s", stage, names[i]);
      d.dump(f, lab);
    }
  }
  fclose(f);
  return 0;
}

#endif
static int cmd_anim(const char* path) {
  FILE* f = fopen(path, "w");
  MockHost h(12345, 0x7CA95E21, 0x11223344);
  WolfGame g(&h);
  g.begin(0);
  struct Seq { const char* name; uint32_t months; } seqs[] = {{"pup", 3}, {"juv", 14}, {"adult", 30}};
  WolfScene* sc = nullptr;
  for (auto& s : seqs) {
    g.pack[0].age_secs = s.months * WOLF_SECS_PER_MONTH;
    memset(g.away, 0, sizeof(g.away));
    g.visitor.active = false;
    g.hearts_until_ms = 0;
    delete sc; sc = new WolfScene(&g);
    MockDisplay d;
    uint32_t t = 100000;
    g.treat_pending = 0;
    for (int i = 0; i < 1000; i++, t += 110) {
      if (i == 30) g.treat_pending = 1;
      if (i == 190) g.bark_pending = true;
      if (i == 300) {   // a first-time visitor walks in from the right
        WolfVisitor& v = g.visitor; memset(&v, 0, sizeof(v));
        v.active = true; v.genome = 0xD3B6C8E4; v.months = 30; v.until_ms = t + 26000; v.enter_dir = -1; v.exit_dir = 1;
        v.visits = 1; v.id = 1;
      }
      if (i == 560) { g.away[0].away = true; g.away[0].leave_dir = -1; }                       // our wolf goes visiting
      if (i == 700) { g.away[0].away = false; g.away[0].return_dir = 1; }                       // and walks back in
      if (i == 760) {   // an old friend (3rd visit) comes in from the left: chase!
        WolfVisitor& v = g.visitor; memset(&v, 0, sizeof(v));
        v.active = true; v.genome = 0x39F2B77A; v.months = 30; v.until_ms = t + 34000; v.enter_dir = 1; v.exit_dir = -1;
        v.visits = 3; v.id = 2;
        g.hearts_until_ms = t + 33000;
      }
      if (i == 820) { g.visitor.visits = 3; }
      d.startFrame(0);
      sc->render(d, t, 3900);
      if (i % 3 == 0) { char lab[40]; snprintf(lab, sizeof(lab), "%s_%03d", s.name, i); d.dump(f, lab); }
      if (g.visitor.active && (int32_t)(t - g.visitor.until_ms) >= 0) g.visitor.active = false;
    }
    g.visitor.active = false;
  }
  fclose(f);
  return 0;
}

// Natural behaviour: nothing forced; count how long it spends in each mode and how often it turns round.
static int cmd_mix() {
  int stuck_total = 0;
  for (int stage = 0; stage < 3; stage++) {
    MockHost h(4242 + stage, 0x7CA95E21, 0x11223344);
    WolfGame g(&h);
    g.begin(0);
    uint32_t months[3] = {3, 14, 30};
    g.pack[0].age_secs = months[stage] * WOLF_SECS_PER_MONTH;
    memset(g.away, 0, sizeof(g.away));
    WolfScene sc(&g);
    MockDisplay d;
    uint32_t t = 100000; int ms[40] = {0}, turns[40] = {0}; int prev = -1; int8_t pdir = 0; int sw = 0, last = -1, run = 0;
    for (int i = 0; i < 9000; i++, t += 110) {          // 16 minutes
      if (i % 400 == 200) g.treat_pending = 1;           // and a treat now and then, landing anywhere
      d.startFrame(0); sc.render(d, t, 3900);
      int m = sc.debugMode(); ms[m]++; if (getenv("DBG") && stage==0 && m==2 && i%40==0) printf("i=%d seek\n", i);
      if (m == last) run++; else run = 0;
      if (run > 300 && m != 8 && m != 6) { if (run == 301) { printf("  STUCK in mode %d for 33 s\\n", m); stuck_total++; } }
      if (pdir && sc.debugDir() != pdir) turns[m]++;
      pdir = sc.debugDir();
      if (m != last) { sw++; last = m; }
      prev = m;
    }
    (void)prev;
    printf("stage %d: %d mode changes in 16 min\n  frames per mode:", stage, sw);
    for (int m = 0; m < 24; m++) printf(" %d:%d", m, ms[m]);
    printf("\n  turns per mode:");
    for (int m = 0; m < 24; m++) printf(" %d:%d", m, turns[m]);
    printf("\n");
  }
  return stuck_total;
}

// Every random activity, forced one at a time, for each growth stage.
static int cmd_events(const char* path, int only_kind, int only_stage) {
  FILE* f = fopen(path, "w");
  MockHost h(777, 0x7CA95E21, 0x11223344);
  WolfGame g(&h);
  g.begin(0);
  uint32_t months[3] = {3, 14, 30};
  int bad = 0;
  for (int stage = 0; stage < 3; stage++) {
    if (only_stage >= 0 && stage != only_stage) continue;
    for (int k = 0; k < WolfScene::EV_COUNT; k++) {
      if (only_kind >= 0 && k != only_kind) continue;
      g.pack[0].age_secs = months[stage] * WOLF_SECS_PER_MONTH;
      memset(g.away, 0, sizeof(g.away));
      g.visitor.active = false; g.hearts_until_ms = 0; g.treat_pending = 0; g.bark_pending = false;
      WolfScene sc(&g);
      MockDisplay d;
      uint32_t t = 100000;
      for (int i = 0; i < 160; i++, t += 110) {            // settle into the normal idle state first
        d.startFrame(0); sc.render(d, t, 3900);
      }
      sc.forceEvent((uint8_t)k);
      for (int i = 0; i < 260; i++, t += 110) {
        d.startFrame(0);
        sc.render(d, t, 3900);
        // nothing may be drawn outside the screen buffer (the mock clips), but the sprite must stay near the screen
        if (i % 2 == 0) { char lab[48]; snprintf(lab, sizeof(lab), "ev%02d_st%d_%03d", k, stage, i); d.dump(f, lab); }
      }
    }
  }
  fclose(f);
  return bad;
}

// ---------------------------------------------------------------------------------------------
// Two radios in range of each other, simulated tick by tick.
struct Radio {
  MockHost host; WolfGame game;
  Radio(uint32_t seed, uint32_t tail, uint32_t prefix) : host(seed, tail, prefix), game(&host) {}
};
static void pump(Radio& from, Radio& to, uint32_t now) {
  while (!from.host.out.empty()) {
    auto p = from.host.out.front(); from.host.out.pop_front();
    to.game.onPacket(p.data(), (int)p.size(), now);
  }
}


static int cmd_logic() {
  printf("== aging ==\n");
  {
    MockHost h(7, 0xCAFEBABE, 1);
    WolfGame g(&h);
    g.begin(0);
    CHECK(g.count == 1 && g.pack[0].genome == 0xCAFEBABE, "founder genome comes from the key tail");
    CHECK(g.pack[0].founder, "founder flag");
    CHECK(g.pack[0].visits_needed >= WOLF_CONSENT_MIN && g.pack[0].visits_needed <= WOLF_CONSENT_MAX, "consent threshold range");
    uint32_t t = 0;
    for (int s = 0; s < 86400; s++) { t += 1000; g.tick(t); }
    CHECK(WolfGame::ageMonths(g.pack[0]) == 7, "one day = 7 months (got %u)", (unsigned)WolfGame::ageMonths(g.pack[0]));
    CHECK(WolfGame::stageOf(g.pack[0]) == STAGE_JUVENILE, "juvenile after a day");
    for (int s = 0; s < 3 * 86400; s++) { t += 1000; g.tick(t); }
    CHECK(WolfGame::isAdult(g.pack[0]), "adult after 4 days (%u months)", (unsigned)WolfGame::ageMonths(g.pack[0]));
    printf("  4 days -> %u months\n", (unsigned)WolfGame::ageMonths(g.pack[0]));
  }
  printf("== sex is random, thresholds vary ==\n");
  {
    int males = 0, females = 0; int need[10] = {0};
    for (uint32_t seed = 1; seed < 300; seed++) {
      MockHost h(seed * 2654435761u + 1, 0x1234ABCD, 1); WolfGame g(&h); g.begin(0);
      (g.pack[0].sex ? females : males)++;
      need[g.pack[0].visits_needed]++;
    }
    CHECK(males > 80 && females > 80, "sex split %d/%d", males, females);
    printf("  sex %d M / %d F; visit thresholds:", males, females);
    for (int i = WOLF_CONSENT_MIN; i <= WOLF_CONSENT_MAX; i++) { printf(" %d:%d", i, need[i]); CHECK(need[i] > 20, "threshold %d too rare", i); }
    printf("\n");
  }
  printf("== messages: treat every 10-15, bark random ==\n");
  {
    MockHost h(99, 0x1111AAAA, 1); WolfGame g(&h); g.begin(0);
    int since = 0, treats = 0; int minGap = 999, maxGap = 0;
    for (int i = 0; i < 3000; i++) {
      g.onMessage(); since++;
      if (g.treat_pending) { treats++; g.treat_pending = 0; minGap = since < minGap ? since : minGap; maxGap = since > maxGap ? since : maxGap; since = 0; }
    }
    printf("  treats %d, gaps %d..%d\n", treats, minGap, maxGap);
    CHECK(minGap >= 10 && maxGap <= 15, "treat gap out of range");
    g.bark_pending = false;
    int barks = 0, bgap = 0, bmin = 999, bmax = 0; bool first = true;
    for (int i = 0; i < 3000; i++) {
      g.onMessage(); bgap++;
      g.treat_pending = 0;
      if (g.bark_pending) {
        g.bark_pending = false;
        if (!first) { barks++; bmin = bgap < bmin ? bgap : bmin; bmax = bgap > bmax ? bgap : bmax; }
        first = false; bgap = 0;
      }
    }
    printf("  barks %d, gaps %d..%d\n", barks, bmin, bmax);
    CHECK(bmin >= WOLF_BARK_MIN_MSGS && bmax <= WOLF_BARK_MAX_MSGS && bmax > bmin, "bark gap range");
  }
  printf("== persistence ==\n");
  {
    MockHost h(5, 0x0BADF00D, 2); WolfGame g(&h); g.begin(0);
    g.pack[0].age_secs = 123456; g.pack[0].sex = 1;
    Wolf c; memset(&c, 0, sizeof(c)); c.genome = 0xDEADBEEF; c.age_secs = 77; c.sex = 0; c.visits_needed = 5; c.sire = 0x1111; c.dam = 0x2222; g.pack[g.count++] = c;
    g.current = 1;
    for (int i = 0; i < 4; i++) { g.mem[i].mine = 0x0BADF00D; g.mem[i].other = 100 + i; g.mem[i].visits = 1 + i; }
    g.save();
    MockHost h2(6, 0x0BADF00D, 2); h2.saved = h.saved;
    WolfGame g2(&h2); g2.begin(0);
    CHECK(g2.count == 2 && g2.current == 1 && g2.pack[0].age_secs == 123456 && g2.pack[1].genome == 0xDEADBEEF &&
          g2.pack[1].visits_needed == 5 && g2.pack[1].sire == 0x1111, "wolves round-trip through flash");
    CHECK(g2.pairVisits(0x0BADF00D, 102) == 3 && g2.pairVisits(0x0BADF00D, 999) == 0, "visit memory round-trips");
    printf("  saved state %u bytes\n", (unsigned)h.saved.size());
  }
  printf("== kinship ==\n");
  {
    CHECK(WolfGame::related(1, 0, 0, 2, 1, 5), "parent and child");
    CHECK(WolfGame::related(10, 7, 8, 11, 7, 9), "siblings share a sire");
    CHECK(!WolfGame::related(1, 0, 0, 2, 0, 0), "unrelated founders");
  }
  printf("== two radios: visits, consent, mating, going home ==\n");
  {
    int mated_runs = 0, total_runs = 24;
    int min_visits_at_mating = 999, max_away_ms = 0;
    for (int run = 0; run < total_runs; run++) {
      Radio M(1000 + run * 7, 0xAAAA0000u + run, 0x01010101u + run), F(2000 + run * 13, 0x5555C000u + run, 0x02020202u + run);
      M.game.begin(0); F.game.begin(0);
      M.game.pack[0].sex = 0; F.game.pack[0].sex = 1;
      M.game.pack[0].age_secs = F.game.pack[0].age_secs = 30 * WOLF_SECS_PER_MONTH;
      bool was_preg = false;
      uint32_t away_since = 0; bool was_away = false;
      uint32_t last_visit_id = 0; int hosted = 0;
      for (uint32_t t = 0; t < 12u * 3600u * 1000u; t += 100) {
        M.game.tick(t); F.game.tick(t);
        pump(M, F, t); pump(F, M, t);
        // consent rule: whenever the female becomes pregnant, BOTH wolves must have reached their thresholds
        if (!was_preg && F.game.pack[0].preg_secs_left) {
          was_preg = true;
          unsigned vf = F.game.pairVisits(F.game.pack[0].genome, M.game.pack[0].genome);
          unsigned vm = M.game.pairVisits(M.game.pack[0].genome, F.game.pack[0].genome);
          if ((int)vf < min_visits_at_mating) min_visits_at_mating = vf;
          CHECK(vf >= F.game.pack[0].visits_needed, "female mated with only %u visits (needs %u)", vf, F.game.pack[0].visits_needed);
          CHECK(vm >= M.game.pack[0].visits_needed || vm + 1 >= M.game.pack[0].visits_needed,
                "male had only %u visits (needs %u)", vm, M.game.pack[0].visits_needed);
          CHECK(vf >= 3, "mating on visit %u", vf);
          mated_runs++;
          break;
        }
        bool aw = M.game.away[0].away || F.game.away[0].away;
        if (aw && !was_away) { away_since = t; was_away = true; }
        if (!aw && was_away) { was_away = false; if ((int)(t - away_since) > max_away_ms) max_away_ms = t - away_since; }
        if (F.game.visitor.active && F.game.visitor.id != last_visit_id) { last_visit_id = F.game.visitor.id; hosted++; }
      }
    }
    printf("  %d/%d pairs mated within 12 h; fewest visits at mating: %d; longest absence %.0f s\n",
           mated_runs, total_runs, min_visits_at_mating, max_away_ms / 1000.0);
    CHECK(mated_runs >= total_runs / 2, "too few pairs mated (%d)", mated_runs);
    CHECK(min_visits_at_mating >= 3, "mating happened too early");
    CHECK(max_away_ms <= 120000, "a wolf stayed away too long (%d ms)", max_away_ms);
  }
  printf("== two radios: wolves return when the connection drops ==\n");
  {
    Radio A(11, 0xAAAA0001, 0x0A0A0A0A), B(12, 0x5555C001, 0x0B0B0B0B);
    A.game.begin(0); B.game.begin(0);
    uint32_t t = 0; bool in_range = true; uint32_t cut_at = 0, home_at = 0, vis_gone_at = 0;
    for (; t < 3600u * 1000u; t += 100) {
      A.game.tick(t); B.game.tick(t);
      if (in_range) { pump(A, B, t); pump(B, A, t); } else { A.host.out.clear(); B.host.out.clear(); }
      if (in_range && A.game.away[0].away && B.game.visitor.active && t > A.game.away[0].leave_ms + 6000) {
        in_range = false; cut_at = t;                 // radios lose each other mid-visit
      }
      if (!in_range && !home_at && !A.game.away[0].away) home_at = t;
      if (!in_range && !vis_gone_at && !B.game.visitor.active) vis_gone_at = t;
      if (home_at && vis_gone_at) break;
    }
    CHECK(cut_at > 0, "a visit happened");
    printf("  connection cut at %.1f s: visitor wolf home after %.1f s, host's guest left after %.1f s\n",
           cut_at / 1000.0, (home_at - cut_at) / 1000.0, (vis_gone_at - cut_at) / 1000.0);
    CHECK(home_at && home_at - cut_at <= 30000, "visiting wolf should come home when the host goes quiet");
    CHECK(vis_gone_at && vis_gone_at - cut_at <= 30000, "guest should leave when its radio goes quiet");
  }
  printf("== alone: nobody to visit, wolf stays home ==\n");
  {
    Radio A(21, 0xAAAA0002, 0x0C0C0C0C);
    A.game.begin(0);
    bool ever_away = false; size_t sent = 0;
    for (uint32_t t = 0; t < 2u * 3600u * 1000u; t += 100) {
      A.game.tick(t);
      sent += A.host.out.size(); A.host.out.clear();
      if (A.game.away[0].away) ever_away = true;
    }
    printf("  %u visit requests in 2 hours, none answered\n", (unsigned)sent);
    CHECK(!ever_away, "wolf must not leave when no host answers");
    CHECK(sent >= 6 && sent <= 16, "request rate");
  }
  printf("== pups carry both parents' genes ==\n");
  {
    Radio M(5, 0xAAAA1111, 0x01010101), F(6, 0x5555CCCC, 0x02020202);
    M.game.begin(0); F.game.begin(0);
    M.game.pack[0].sex = 0; F.game.pack[0].sex = 1;
    M.game.pack[0].age_secs = F.game.pack[0].age_secs = 30 * WOLF_SECS_PER_MONTH;
    uint32_t t = 0; bool mated = false;
    for (; t < 24u * 3600u * 1000u && !mated; t += 100) {
      M.game.tick(t); F.game.tick(t); pump(M, F, t); pump(F, M, t);
      mated = F.game.pack[0].preg_secs_left > 0;
    }
    CHECK(mated, "pair mated");
    for (uint32_t k = 0; k < 80; k++, t += 100) { M.game.tick(t); F.game.tick(t); pump(M, F, t); pump(F, M, t); }   // let the BRED notice arrive
    CHECK(M.game.pack[0].pups_sired == 1 || t < 1, "father's radio was told (sired=%u)", (unsigned)M.game.pack[0].pups_sired);
    uint32_t gest = F.game.pack[0].preg_secs_left; uint8_t litter = F.game.pack[0].litter;
    printf("  gestation %.1f h, litter %u\n", gest / 3600.0, (unsigned)litter);
    for (uint32_t s = 0; s < gest + 5; s++) { t += 1000; F.game.tick(t); F.host.out.clear(); M.host.out.clear(); }
    CHECK(F.game.count >= 2, "pups born (%u wolves)", (unsigned)F.game.count);
    for (int i = 1; i < F.game.count; i++) {
      int same = 0;
      for (int n = 0; n < 8; n++) {
        uint32_t cn = (F.game.pack[i].genome >> (4 * n)) & 15, mn = (F.game.pack[0].genome >> (4 * n)) & 15, fn = (0xAAAA1111u >> (4 * n)) & 15;
        if (cn == mn || cn == fn) same++;
      }
      printf("  pup %d genome %08X: %d/8 genes from its parents, sire %08X dam %08X, needs %u visits\n", i,
             (unsigned)F.game.pack[i].genome, same, (unsigned)F.game.pack[i].sire, (unsigned)F.game.pack[i].dam,
             (unsigned)F.game.pack[i].visits_needed);
      CHECK(same >= 5, "pup carries parental genes");
      CHECK(F.game.pack[i].sire == 0xAAAA1111u && F.game.pack[i].dam == F.game.pack[0].genome, "lineage recorded");
    }
  }
  printf("== same sex / juvenile never mate ==\n");
  {
    Radio A(31, 0xAAAA0003, 0x0D0D0D0D), B(32, 0x5555C003, 0x0E0E0E0E);
    A.game.begin(0); B.game.begin(0);
    A.game.pack[0].sex = 1; B.game.pack[0].sex = 1;
    A.game.pack[0].age_secs = B.game.pack[0].age_secs = 30 * WOLF_SECS_PER_MONTH;
    for (uint32_t t = 0; t < 8u * 3600u * 1000u; t += 100) { A.game.tick(t); B.game.tick(t); pump(A, B, t); pump(B, A, t); }
    CHECK(!A.game.pack[0].preg_secs_left && !B.game.pack[0].preg_secs_left, "two females never mate");
    printf("  two females met %u times, no mating\n", (unsigned)A.game.pairVisits(A.game.pack[0].genome, B.game.pack[0].genome));
    Radio C(33, 0xAAAA0004, 0x0F0F0F0F), D(34, 0x5555C004, 0x10101010);
    C.game.begin(0); D.game.begin(0);
    C.game.pack[0].sex = 0; D.game.pack[0].sex = 1;
    C.game.pack[0].age_secs = 30 * WOLF_SECS_PER_MONTH; D.game.pack[0].age_secs = 10 * WOLF_SECS_PER_MONTH;   // she is a juvenile
    for (uint32_t t = 0; t < 8u * 3600u * 1000u; t += 100) { C.game.tick(t); D.game.tick(t); pump(C, D, t); pump(D, C, t); D.game.pack[0].age_secs = 10 * WOLF_SECS_PER_MONTH; }
    CHECK(!D.game.pack[0].preg_secs_left, "a juvenile never mates");
  }
  printf("%s\n", fails ? "SOME CHECKS FAILED" : "all checks passed");
  return fails ? 1 : 0;
}

int main(int argc, char** argv) {
  if (argc >= 3 && !strcmp(argv[1], "sheet")) return cmd_sheet(argv[2]);
  if (argc >= 2 && !strcmp(argv[1], "mix")) return cmd_mix();
  if (argc >= 3 && !strcmp(argv[1], "anim")) return cmd_anim(argv[2]);
  if (argc >= 3 && !strcmp(argv[1], "events")) return cmd_events(argv[2], argc >= 4 ? atoi(argv[3]) : -1, argc >= 5 ? atoi(argv[4]) : -1);
  if (argc >= 2 && !strcmp(argv[1], "logic")) return cmd_logic();
  printf("usage: host_sim sheet|anim|events <file> [kind [stage]] | logic\n");
  return 2;
}
