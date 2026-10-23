#pragma once
// WolfScene: animates and draws the wolf screen on a 128x64 monochrome display.

#include <helpers/ui/DisplayDriver.h>
#include "WolfCore.h"


#if PET_INSECT
// ---- the insect version of the scene (butterfly life cycle).  Same public face as the quadruped one.
class WolfScene {
public:
  explicit WolfScene(WolfGame* game);

  enum { EV_COUNT = 7 };
  uint8_t debugMode() const { return _w.mode; }
  int8_t  debugDir() const { return _w.dir; }
  void    forceEvent(uint8_t kind) { _force = kind + 1; }

  // Draw one frame (UITask has already cleared the screen).  Returns ms until the next frame.
  int render(DisplayDriver& d, uint32_t now_ms, uint16_t batt_mv);

private:
  enum Mode : uint8_t { I_IDLE, I_WANDER, I_SEEK, I_FEED, I_HAPPY, I_FLOURISH, I_GREET, I_LEAVE, I_AWAY, I_RETURN,
                        I_BASK, I_DANCE, I_LADY, I_CRAWL, I_REAR, I_CURL };
  enum VPhase : uint8_t { V_NONE, V_ENTER, V_STAY, V_LEAVE };
  enum Form : uint8_t { F_CATERPILLAR, F_CHRYSALIS, F_YOUNG, F_ADULT };

  struct Actor {
    float    x, y;               // butterfly: the thorax; caterpillar: left edge of the sprite and the ground row
    int8_t   dir;                // +1 / -1 (caterpillars face it; butterflies drift that way)
    uint8_t  mode, phase;
    uint32_t t0, until, pt0;
    float    ax, ay;             // a target / a second position
    uint8_t  frame;              // animation frame
    uint32_t frame_ms;
    bool     moving;
  };
  struct Food { bool active, grown; float x; uint32_t t0, eat0; };
  struct Lady { bool show; float x, y; int8_t dir; uint8_t frame; };

  WolfGame* g;
  bool      _init;
  uint32_t  _last_ms, _rng;
  uint8_t   _shown_idx, _vphase, _form, _last_ev, _force;
  uint32_t  _shown_genome, _vseen_id;
  Actor     _w, _v;
  Food      _food;
  Lady      _lady;
  float     _trail[6][2];
  uint8_t   _trail_n;

  uint32_t rnd(uint32_t lo, uint32_t hi);
  void     startMode(uint8_t m, uint32_t now, uint32_t dur_ms);
  uint8_t  formOf(uint32_t months) const;
  void     update(uint32_t now, uint32_t dt);
  void     updateVisitor(uint32_t now, uint32_t dt);
  bool     flyTo(Actor& a, float tx, float ty, float speed, uint32_t dt);
  void     crawl(Actor& a, uint32_t dt, float speed, int w);
  void     beginEvent(uint8_t kind, uint32_t now);
  void     drawCreature(DisplayDriver& d, uint32_t genome, uint8_t form, const Actor& a, uint32_t now, bool resident);
  void     drawProps(DisplayDriver& d, uint32_t now);
  void     drawHud(DisplayDriver& d, const Wolf& w, uint16_t batt_mv);
};
#else
struct WolfPose {
  uint8_t head;    // 0 side, 1 side-down (eating), 2 front, 3 front + open mouth, 4 side, head up (howl)
  uint8_t legs;    // 0 stand, 1..4 walk cycle
  uint8_t tail;    // 0 / 1 (wag)
  bool    mirror;  // true = facing left
  uint8_t lie;     // 1 = lying down (zero when brace-initialised with fewer values)
  bool    blink;   // eyes closed
  uint8_t sub;     // sunk into water / mud: this many rows below the ground line (and cut off)
};

// Draws one wolf into the display.  (x, ground_y) = left edge of the sprite canvas and the row the
// paws stand on.  Returns the sprite canvas width.
int wolfDrawSprite(DisplayDriver& d, uint32_t genome, uint8_t stage, int x, int ground_y, const WolfPose& pose);

class WolfScene {
public:
  explicit WolfScene(WolfGame* game);

  enum { EV_COUNT = 17 };
  uint8_t debugMode() const { return _w.mode; }
  int8_t  debugDir() const { return _w.dir; }
  void forceEvent(uint8_t kind) { _force = kind + 1; }     // for tests: start this activity at the next frame

  // Draw one frame (UITask has already cleared the screen).  Returns ms until the next frame.
  int render(DisplayDriver& d, uint32_t now_ms, uint16_t batt_mv);

private:
  enum Mode : uint8_t { M_IDLE, M_WALK, M_SEEK, M_EAT, M_HAPPY, M_BARK, M_GREET, M_LEAVE, M_AWAY, M_RETURN,
                        // random things a wolf does (one per EvKind, in the same order)
                        M_BUNNY, M_BIRD, M_FLY, M_DIG, M_SLEEP, M_REST, M_HOWL, M_SHAKE, M_YAWN, M_CHASE,
                        M_SNIFF, M_STRETCH, M_ZOOM, M_POUNCE, M_WALLOW, M_SWIM, M_SNAP };
  enum VPhase : uint8_t { V_NONE, V_ENTER, V_STAY, V_LEAVE };

  struct Actor {
    int32_t  x16;        // left edge of the canvas, 1/16 px
    int8_t   dir;        // +1 facing right, -1 facing left
    uint8_t  mode;
    uint32_t t0, until;
    uint32_t frame_ms;   // walk-cycle accumulator
    uint8_t  legf;
    uint8_t  tailf;
    bool     moving;
    uint32_t tail_ms;
  };
  struct Treat { bool active; bool landed; int32_t x16, y16; };

  WolfGame* g;
  bool      _init;
  uint32_t  _last_ms, _rng;
  uint8_t   _shown_idx;
  uint32_t  _shown_genome;
  Actor     _w;           // resident wolf
  Actor     _v;           // visitor
  uint8_t   _vphase;
  uint32_t  _vseen_id, _play_t0;
  int       _cp0[2];          // chase phase at the start of play
  uint8_t   _play_level;
  Treat     _treat;
  // random activities
  struct Ev { uint8_t phase; uint32_t pt0; int32_t px16, py16; int16_t vx, vy; int8_t pdir; uint8_t frame; bool show; uint32_t aux, aux2; };
  struct Hole { bool active; int x; uint8_t depth; uint32_t until; };
  struct Pool { bool active; uint8_t kind; int x, w; uint32_t until; };   // kind 0 = mud, 1 = water
  Pool      _pool;
  Ev        _ev;
  Hole      _hole;
  uint8_t   _last_ev, _force, _stage;
  int       _rw;
  uint32_t  _eat_start;

  uint32_t rnd(uint32_t lo, uint32_t hi);
  void     startMode(uint8_t m, uint32_t now, uint32_t dur_ms);
  void     update(uint32_t now, uint32_t dt, uint8_t stage, int rw, int vw);
  void     updateVisitor(uint32_t now, uint32_t dt, int rw, int vw);
  int      restX(bool resident, int rw, int vw) const;   // standing spot while greeting
  void     walkStep(Actor& a, uint32_t dt, int speed_pxs, int cw);
  void     beginEvent(uint8_t kind, uint32_t now, uint8_t stage);
  uint8_t  pickEvent(uint8_t stage);
  void     updateEvent(uint32_t now, uint32_t dt, uint8_t stage, int rw);
  void     drawEventProps(DisplayDriver& d, uint32_t now, int rw, int ch);
  void     drawHud(DisplayDriver& d, const Wolf& w, uint16_t batt_mv);
};

#endif   // !PET_INSECT
