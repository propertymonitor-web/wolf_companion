#pragma once
#include "PetSpecies.h"
// WolfCore: game rules for the MeshCore wolf pup.  No Arduino / MeshCore dependencies,
// so it can be unit-tested on a PC (see test/host_sim.cpp).

#include <stdint.h>
#include <string.h>

// ------------------------------------------------------------------ tuning --
#define WOLF_MAX_PACK            6
#define WOLF_SECS_PER_MONTH      12342UL   // 7 wolf-months per real day (86400 / 7 = 12342.86)
#define WOLF_PUP_MONTHS          6         // pup sprite until 6 months
#define WOLF_ADULT_MONTHS        24        // adult from 24 months (about 3.4 days powered on)
#define WOLF_GESTATION_MONTHS    2         // ~63 days -> about 7 hours
#define WOLF_COOLDOWN_MONTHS     12        // mother can't mate again for 12 wolf-months (~4 h)
#define WOLF_HYBRID_PCT          20        // (secret) of those matings between two different kinds that CAN have young
#define WOLF_CHIMERA_PCT         12        // (secret) the same for pairs that make a brand-new mixed species
#define WOLF_MATE_CHANCE_PCT     60        // chance of mating once BOTH wolves have given consent

// "Digital consent": every wolf rolls its own number of visits it needs from a given wolf before
// it will even consider mating with it.  Minimum 3, so mating can never happen on a first visit.
#define WOLF_CONSENT_MIN         3
#define WOLF_CONSENT_MAX         7
#define WOLF_MEM_SIZE            24        // remembered (my wolf, their wolf) pairs

#define WOLF_TREAT_MIN_MSGS      10        // a treat every 10..15 received messages
#define WOLF_TREAT_MAX_MSGS      15
#define WOLF_BARK_MIN_MSGS       5         // a bark every 5..20 received messages (re-rolled each time)
#define WOLF_BARK_MAX_MSGS       20

#define WOLF_HELLO_FIRST_MIN_S   45        // first outing after boot: 45..120 s
#define WOLF_HELLO_FIRST_MAX_S   120
#define WOLF_HELLO_MIN_S         600       // then every 10..25 minutes
#define WOLF_HELLO_MAX_S         1500
#define WOLF_VISIT_SECS          40        // normal visit length
#define WOLF_FAMILY_VISIT_SECS   70        // dad visiting his pups stays longer to help look after them
#define WOLF_MATE_EXTEND_SECS    20        // courtship adds a little time
#define WOLF_PING_MS             10000     // keep-alive between the two radios during a visit
#define WOLF_PEER_LOST_MS        25000     // no word from the other radio this long -> go home
#define WOLF_REQUEST_WAIT_MS     9000      // wait this long for a host to answer a visit request
#define WOLF_SAVE_EVERY_S        600       // flash save interval (also saved on events)

// ----------------------------------------------------------------- packets --
// Sent as zero-hop MeshCore control packets (payload[0] has bit 7 set, as required).
//   0xA0 VISIT   a wolf asks to visit whoever can hear it
//   0xA1 BRED    "we mated" notice to the other radio (extends the visit)
//   0xA2 ACK     host accepts the visitor and says which wolf greets it
//   0xA3 PING    keep-alive while a visit is going on
// every pet is known to every other pet: (type, version) identifies the species of whoever is talking
#define WOLF_SPECIES_COUNT 15
uint8_t petPktType(uint8_t sp);
uint8_t petPktVer(uint8_t sp);
uint8_t petHiddenFrom(uint8_t sp);
uint8_t petHiddenTo(uint8_t sp);
const char* petNameOf(uint8_t sp);
uint8_t petSpeciesFromPkt(uint8_t type, uint8_t ver);   // 0xFF = not one of ours
const char* petTitleOf(uint8_t sp);                      // "Wolf", "Alligator" ... for the screen
const char* petShortOf(uint8_t sp);                      // the same squeezed to 5 letters or fewer
#if PET_MYST
  // mystery egg: the packet family follows the pet inside (and each pet of a mixed family), so a hatched fox
  // still visits ordinary fox radios
  extern uint8_t g_mystery_sp;                       // species number (= PET_ID of that pet), chosen from the radio's identity
  uint8_t petPickSpecies(uint32_t identity);
  uint8_t petHybrid(uint8_t a, uint8_t b);           // 0xFF = these two can never have young together
  #define WOLF_MY_SPECIES          g_mystery_sp
  #define WOLF_PKT_TYPE            petPktType(_pkt_sp)
  #define WOLF_PKT_VERSION         petPktVer(_pkt_sp)
#else
  #define WOLF_MY_SPECIES          PET_ID
  #define WOLF_PKT_TYPE            PET_PKT_TYPE   // high nibble; one family per kind of pet (see PetSpecies.h)
  #define WOLF_PKT_VERSION         PET_PKT_VER
#endif
#define WOLF_PKT_VISIT           0x0       // low nibble
#define WOLF_PKT_BRED            0x1
#define WOLF_PKT_ACK             0x2
#define WOLF_PKT_PING            0x3

enum WolfStageId : uint8_t { STAGE_PUP = 0, STAGE_JUVENILE = 1, STAGE_ADULT = 2 };

struct WolfTraits {
  uint8_t shade;      // 0 dark .. 3 light  (fill density of the coat)
  uint8_t ear;        // 0 erect, 1 rounded, 2 floppy tips
  uint8_t tail;       // 0 bushy, 1 curled up, 2 short stub
  uint8_t marking;    // 0 none, 1 socks, 2 dark muzzle, 3 chest blaze, 4 saddle, 5 spots
  uint8_t tailtip;    // 0 normal, 1 light tip, 2 dark tip
  uint8_t fluff;      // 0 sleek, 1 fluffy ruff
  uint8_t eye;        // 0 round, 1 narrow, 2 tall
  uint8_t spotseed;   // seed for spot layout
};

struct Wolf {
  uint32_t genome;          // 8 nibbles; founders = last 8 hex chars of the radio's public key
  uint32_t age_secs;        // powered-on seconds lived
  uint8_t  sex;             // 0 male, 1 female
  uint8_t  founder;         // 1 = the wolf that came with this radio
  uint8_t  litter;          // pups carried (females)
  uint8_t  visits_needed;   // this wolf's private consent threshold (3..7 visits from the same wolf)
  uint32_t preg_secs_left;  // 0 = not pregnant
  uint32_t father_genome;   // of the pups being carried
  uint32_t cooldown_secs;   // time until she may mate again
  uint32_t sire;            // parents (0 = unknown / founder)
  uint32_t dam;
  uint16_t pups_born;
  uint16_t pups_sired;
  uint8_t  kind;            // species + 1 (0 = the species of this firmware); only the mystery build mixes species
  uint8_t  child_kind;      // species + 1 of the young being carried (0 = the mother's own)
  char     name[8];         // the owner's name for this pet (up to 7 characters), empty = none
};

// Who has this device's wolves met?  (my wolf, their wolf) -> number of visits.
struct WolfMemory {
  uint32_t mine, other;
  uint8_t  visits;          // 0 = slot unused
};

// Runtime-only: is this wolf out visiting another radio?
struct WolfAway {
  bool     away;
  uint32_t leave_ms;
  uint32_t return_ms;
  int8_t   leave_dir;       // edge it walks off through (-1 left, +1 right)
  int8_t   return_dir;      // edge it walks back in through
};

struct WolfVisitor {        // a wolf from another radio that is on OUR screen
  bool     active;
  uint32_t genome;
  uint8_t  sex;
  uint16_t months;
  uint8_t  species;         // what kind of pet it is (PET_ID numbering)
  uint8_t  kind;            // its kind code (see WolfGame::kindCode)
  uint8_t  owner[4];
  uint32_t until_ms, last_heard_ms, next_ping_ms;
  int8_t   enter_dir;       // +1 = walks in from the left edge, -1 = from the right edge
  int8_t   exit_dir;        // edge it leaves through
  uint8_t  visits;          // how many times this visitor has met the greeter (1 = first time)
  bool     family;          // visiting its own pups
  uint8_t  greeter;         // index of our wolf that greets it
  uint32_t id;              // changes with every accepted visit
};

// Services supplied by the platform (MeshCore glue or the PC test harness).
struct WolfHost {
  virtual ~WolfHost() {}
  virtual uint32_t random32() = 0;
  virtual uint32_t identityTail() = 0;               // last 4 bytes of the public key (big-endian) = last 8 hex chars
  virtual void ownerPrefix(uint8_t out[4]) = 0;      // first 4 bytes of the public key
  virtual bool loadState(uint8_t* buf, int maxlen, int& len) = 0;
  virtual void saveState(const uint8_t* buf, int len) = 0;   // should keep a second copy: see loadBackup
  virtual bool loadBackup(uint8_t* buf, int maxlen, int& len) { (void)buf; (void)maxlen; (void)len; return false; }   // the second copy, used when the first is damaged (power cut while saving)
  virtual void broadcast(const uint8_t* buf, int len) = 0;   // zero-hop control packet
  virtual void alert(const char* text) = 0;          // short popup on the radio's screen
};

class WolfGame {
public:
  explicit WolfGame(WolfHost* host);

  // lifecycle
  void begin(uint32_t now_ms);
  void tick(uint32_t now_ms);          // call often (every loop)
  void save();

  // inputs
  void onMessage();                                         // any received message (channel or DM)
  void onPacket(const uint8_t* p, int len, uint32_t now_ms); // control payload from another radio

  // state read by the screen
  Wolf        pack[WOLF_MAX_PACK];
  WolfAway    away[WOLF_MAX_PACK];
  uint8_t     count;
  uint8_t     current;                 // wolf shown on the main screen
  uint8_t     treat_pending;           // treats waiting to be handed out
  bool        bark_pending;
  uint32_t    hearts_until_ms;
  WolfVisitor visitor;
  WolfMemory  mem[WOLF_MEM_SIZE];

  // helpers
  static void     traitsOf(uint32_t genome, WolfTraits& t);
  // eggs: some pets (and every mystery pet) start as an egg that hatches after 24..48 hours powered on
  static uint32_t eggSecs(uint32_t genome) {
#if PET_MYST || PET_EGG
    uint32_t x = genome ^ 0x5EEDC0DEu; x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu; x ^= x >> 16;
    return (24u + x % 25u) * 3600UL;
#else
    (void)genome; return 0;
#endif
  }
  static bool     isEgg(const Wolf& w) { uint32_t e = eggSecs(w.genome); return e && w.age_secs < e; }
  static uint32_t eggLeftSecs(const Wolf& w) { uint32_t e = eggSecs(w.genome); return (e && w.age_secs < e) ? e - w.age_secs : 0; }
  static uint32_t ageMonths(const Wolf& w) { uint32_t e = eggSecs(w.genome); return (w.age_secs > e ? w.age_secs - e : 0) / WOLF_SECS_PER_MONTH; }
  static uint8_t  stageOf(const Wolf& w);
  static uint8_t  stageFromMonths(uint32_t months);
  static bool     isAdult(const Wolf& w) { return ageMonths(w) >= WOLF_ADULT_MONTHS; }
#if PET_MYST
  static bool     hidden(const Wolf& w) { uint32_t m = ageMonths(w); uint8_t sp = speciesOf(w); return isEgg(w) || (m >= petHiddenFrom(sp) && m < petHiddenTo(sp)); }
#else
  static bool     hidden(const Wolf& w) { uint32_t m = ageMonths(w); return isEgg(w) || (m >= PET_HIDDEN_FROM && m < PET_HIDDEN_TO); }   // an egg or a chrysalis stays put
#endif
  // give the pet at `idx` a name (blank clears it); letters, digits, space and . - _ ' ! ? only, 7 characters at most
  bool            setName(uint8_t idx, const char* text);
  // `kind` (see Wolf): 0 = the species of this firmware, 1..15 = species + 1, 16..240 = a mixed species (body * 15 + head + 16)
  static uint8_t  speciesOf(const Wolf& w) {          // whose body (and packet family) the pet has
    if (w.kind == 0) return (uint8_t)WOLF_MY_SPECIES;
    return w.kind <= WOLF_SPECIES_COUNT ? (uint8_t)(w.kind - 1) : (uint8_t)((w.kind - 16) / WOLF_SPECIES_COUNT);
  }
  static uint8_t  headOf(const Wolf& w) {              // whose head it has, 0xFF if it is not a mixed species
    return w.kind > WOLF_SPECIES_COUNT ? (uint8_t)((w.kind - 16) % WOLF_SPECIES_COUNT) : (uint8_t)0xFF;
  }
  static uint8_t  kindCode(const Wolf& w) { return w.kind ? w.kind : (uint8_t)(WOLF_MY_SPECIES + 1); }
  static uint8_t  chimeraKind(uint8_t body, uint8_t head) { return (uint8_t)(16 + body * WOLF_SPECIES_COUNT + head); }
  static bool     chimeraOk(uint8_t body, uint8_t head) { return body != head && body < WOLF_SPECIES_COUNT && head < WOLF_SPECIES_COUNT && body != 4 && head != 4; }   // 4 = butterfly: insects cannot be mixed
  static bool     kindValid(uint8_t k) { return k <= WOLF_SPECIES_COUNT || (k >= 16 && k <= 16 + WOLF_SPECIES_COUNT * WOLF_SPECIES_COUNT - 1 && chimeraOk((uint8_t)((k - 16) / WOLF_SPECIES_COUNT), (uint8_t)((k - 16) % WOLF_SPECIES_COUNT))); }
  static void     titleOf(const Wolf& w, char* out, size_t cap);   // "Wolf", or for a mixed species a made-up name like "Wolbear"
  static bool     canPair(uint8_t kind_a, uint8_t kind_b);        // by kind code (see kindCode)
  int             youngKind(uint8_t mother_kind, uint8_t father_kind);   // child kind (0 = the mother's own), or -1 for no young
  static bool     related(uint32_t g1, uint32_t s1, uint32_t d1, uint32_t g2, uint32_t s2, uint32_t d2);
  uint32_t mixGenome(uint32_t mother, uint32_t father);
  void     cycleWolf();
  void     describe(const Wolf& w, char* out, int outlen) const;   // e.g. "F 14mo #A3F9C1D2"
  uint8_t  pairVisits(uint32_t mine, uint32_t other) const;
  bool     consents(const Wolf& w, uint32_t other) const;           // reached its own visit threshold?
  bool     isAway(int idx) const { return idx >= 0 && idx < count && away[idx].away; }

  // (de)serialisation, public for tests
  int  serialize(uint8_t* buf, int maxlen) const;
  bool deserialize(const uint8_t* buf, int len);

private:
  WolfHost* _h;
  uint32_t _last_ms, _ms_acc, _secs_since_save;
  uint32_t _treat_cnt, _treat_target, _bark_cnt, _bark_target;
  uint32_t _next_trip_ms, _visit_seq;
  uint8_t  _trip_idx;
  struct Seen { uint8_t owner[4]; uint32_t at_ms; bool used; } _seen[8];
  struct Trip {                 // OUR wolf visiting another radio
    bool active, confirmed;
    uint8_t wolf;
    uint8_t host[4];
    uint32_t greeter;
    uint32_t request_ms, until_ms, last_heard_ms, next_ping_ms, extend_ms;
  } _trip;
  struct Outbox { bool used; uint32_t due_ms; uint8_t pkt[40]; int len; } _out[4];
  void queue(const uint8_t* pkt, int len, uint32_t due_ms);
  bool _dirty;
  uint8_t _pkt_sp, _rx_sp, _rx_kind;      // species used for the packet being built / species of the packet being handled

  uint32_t rnd(uint32_t lo, uint32_t hi);          // inclusive
  uint8_t  rollNeed() { return (uint8_t)rnd(WOLF_CONSENT_MIN, WOLF_CONSENT_MAX); }
  void createFounder();
  void ageOneSecond();
  void giveBirth(Wolf& mother);
  bool addWolf(const Wolf& w);
  void rollTreat();
  void rollBark();

  uint8_t bumpVisits(uint32_t mine, uint32_t other);
  bool sendRequest(uint32_t now);
  void sendPing(const uint8_t* target, uint32_t now);
  void endTrip(uint32_t now);
  void handleRequest(const uint8_t* p, int len, uint32_t now);
  void handleAck(const uint8_t* p, int len, uint32_t now);
  void handleBred(const uint8_t* p, int len, uint32_t now);
  void handlePing(const uint8_t* p, int len, uint32_t now);
  void sendBred(const uint8_t* target, uint32_t father, uint32_t mother);
  void makePregnant(Wolf& mother, uint32_t father_genome, uint8_t child_kind);
};
