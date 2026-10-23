#pragma once
// Which pet is this firmware?  Build with exactly one of -DPET_FOX=1  -DPET_CAT=1  -DPET_BEAR=1
// (nothing = the wolf).  Everything that differs between pets lives here: the radio packet family
// (so different pets never confuse each other), the save-file name, the sprite art, what the pet
// says, and how likely each activity is.

#define PET_EV_BUNNY   0     // activity numbers (must match WolfScene.cpp)
#define PET_EV_COUNT   19
#define PET_PKT_VER    2     // packet version byte; also tells species within one family apart
#define PET_TREAT      0     // 0 bone, 1 apple, 2 fish
#define PET_SPLASH     0     // 1 = show the 'INSPIRED BY' card after the ORION splash
#define PET_WALLOW     0     // activities only some pets can do (weights below are 0 for the rest)
#define PET_MIN_SHADE  0     // lowest coat density (bears are never hollow)
#define PET_FIRE       0     // fire-breathing kind: 0 dragon's breath, 1 phoenix flame
#define PET_MAGIC      0     // sparkle kind: 0 unicorn rainbow + stars, 1 kitsune foxfire, 2 griffin feathers
#define PET_EGG_STYLE  0     // shell look: 0 speckled, 1 scaly, 2 flames
#define PET_EGG        0     // 1 = starts as an egg that hatches after 24..48 hours powered on
#define PET_INSECT     0     // 1 = an insect (own scene, own life cycle)
#define PET_HIDDEN_FROM 0    // months during which the pet stays put (a chrysalis does not go visiting)
#define PET_HIDDEN_TO   0

// Mystery egg: ONE firmware that carries every pet.  The radio's identity picks which pet is inside, and the
// pet is revealed when the egg hatches.  PET_MYST is true wherever the core game logic is compiled for it; the
// per-species scene files (MysteryXxx.cpp) define PET_MYSTERY_TU instead of PET_MYSTERY so they see ONE species.
#if defined(PET_MYSTERY) || defined(PET_MYSTERY_TU)
  #define PET_MYST 1
#else
  #define PET_MYST 0
#endif

// weights, per growth stage (pup, adolescent, adult), for the 19 activities in this order:
//  prey-visit  bird  flyer  dig  sleep  rest  call  shake  yawn  chase-tail  sniff  stretch  zoomies  pounce  wallow  swim  snap  fire  magic

#if defined(PET_MYSTERY) && !defined(PET_MYSTERY_TU)
  #define PET_ID            255
  #define PET_NAME          "mystery"
  #define PET_PKT_TYPE      0xF0
  #define PET_STATE_FILE    "/pet_mystery"
  #define PET_ART           "WolfArt_wolf.h"
  #define PET_BARK_TEXT     ""
  #define PET_CALL_PUP      ""
  #define PET_CALL_ADULT    ""
  #define PET_CALL_MOON     0
  #define PET_PREY          0
  #define PET_FLYER         0
  #define PET_WEIGHTS { {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1} }
#elif defined(PET_FOX)
  #define PET_ID            1
  #define PET_NAME          "fox"
  #define PET_PKT_TYPE      0xB0
  #define PET_STATE_FILE    "/pet_fox"
  #define PET_ART           "WolfArt_fox.h"
  #define PET_BARK_TEXT     "YIP!"
  #define PET_CALL_PUP      "yip~"
  #define PET_CALL_ADULT    "YIP YIP!"
  #define PET_CALL_MOON     1
  #define PET_PREY          0          // 0 rabbit, 1 mouse, 2 frog
  #define PET_FLYER         0          // 0 butterfly, 1 bee
  #define PET_WEIGHTS { \
    { 2, 2, 2, 2, 4, 1, 1, 2, 2, 3, 2, 2, 4, 4, 0, 0, 0, 0, 0 }, \
    { 2, 2, 2, 3, 3, 1, 2, 2, 2, 2, 2, 2, 3, 4, 0, 0, 0, 0, 0 }, \
    { 2, 2, 1, 3, 3, 2, 3, 2, 2, 1, 3, 2, 1, 5, 0, 0, 0, 0, 0 } }
#elif defined(PET_CAT)
  #define PET_ID            2
  #define PET_NAME          "cat"
  #define PET_PKT_TYPE      0xC0
  #define PET_STATE_FILE    "/pet_cat"
  #define PET_ART           "WolfArt_cat.h"
  #define PET_BARK_TEXT     "MEOW!"
  #define PET_CALL_PUP      "mew~"
  #define PET_CALL_ADULT    "MROOOW~"
  #define PET_CALL_MOON     0
  #define PET_PREY          1
  #define PET_FLYER         0
  #define PET_WEIGHTS { \
    { 2, 2, 3, 0, 5, 3, 1, 1, 3, 4, 2, 3, 4, 4, 0, 0, 0, 0, 0 }, \
    { 2, 2, 3, 0, 4, 3, 1, 1, 3, 3, 2, 3, 3, 4, 0, 0, 0, 0, 0 }, \
    { 2, 2, 2, 0, 4, 4, 1, 1, 3, 1, 2, 3, 2, 5, 0, 0, 0, 0, 0 } }
#elif defined(PET_BEAR)
  #define PET_ID            3
  #define PET_NAME          "bear"
  #define PET_PKT_TYPE      0xD0
  #undef  PET_MIN_SHADE
  #define PET_MIN_SHADE     2
  #define PET_STATE_FILE    "/pet_bear"
  #define PET_ART           "WolfArt_bear.h"
  #define PET_BARK_TEXT     "GRRR!"
  #define PET_CALL_PUP      "rawr~"
  #define PET_CALL_ADULT    "ROAARR!"
  #define PET_CALL_MOON     0
  #define PET_PREY          2
  #define PET_FLYER         1
  #define PET_WEIGHTS { \
    { 3, 1, 4, 2, 4, 2, 1, 2, 2, 3, 3, 2, 2, 0, 0, 0, 0, 0, 0 }, \
    { 3, 1, 4, 3, 3, 2, 1, 2, 2, 2, 3, 2, 1, 0, 0, 0, 0, 0, 0 }, \
    { 3, 1, 3, 4, 3, 3, 2, 2, 3, 1, 4, 2, 0, 0, 0, 0, 0, 0, 0 } }
#elif defined(PET_GATOR)
  #define PET_ID            5
  #define PET_NAME          "alligator"
  #define PET_PKT_TYPE      0xF0
  #undef  PET_MIN_SHADE
  #define PET_MIN_SHADE     2
  #undef  PET_SPLASH
  #define PET_SPLASH        1
  #undef  PET_TREAT
  #define PET_TREAT         2
  #define PET_STATE_FILE    "/pet_gator"
  #define PET_ART           "WolfArt_gator.h"
  #define PET_BARK_TEXT     "HISSS!"
  #define PET_CALL_PUP      "chirp~"
  #define PET_CALL_ADULT    "BELLOW!"
  #define PET_CALL_MOON     0
  #define PET_PREY          2
  #define PET_FLYER         0
  #define PET_WEIGHTS { \
    { 3, 3, 2, 1, 4, 3, 1, 1, 2, 2, 2, 2, 2, 1, 0, 4, 3, 0, 0 }, \
    { 3, 3, 2, 1, 4, 4, 1, 1, 2, 1, 2, 2, 1, 2, 0, 5, 4, 0, 0 }, \
    { 3, 3, 1, 1, 4, 5, 2, 1, 2, 0, 2, 2, 1, 2, 0, 5, 4, 0, 0 } }
#elif defined(PET_CROC)
  #define PET_ID            6
  #define PET_NAME          "crocodile"
  #define PET_PKT_TYPE      0xF0
  #undef  PET_PKT_VER
  #define PET_PKT_VER       3
  #undef  PET_MIN_SHADE
  #define PET_MIN_SHADE     1
  #undef  PET_SPLASH
  #define PET_SPLASH        1
  #undef  PET_TREAT
  #define PET_TREAT         2
  #define PET_STATE_FILE    "/pet_croc"
  #define PET_ART           "WolfArt_croc.h"
  #define PET_BARK_TEXT     "SNAP!"
  #define PET_CALL_PUP      "yelp~"
  #define PET_CALL_ADULT    "GRAAWR!"
  #define PET_CALL_MOON     0
  #define PET_PREY          2
  #define PET_FLYER         1
  #define PET_WEIGHTS { \
    { 3, 2, 2, 1, 4, 3, 1, 1, 2, 2, 2, 2, 3, 3, 0, 4, 4, 0, 0 }, \
    { 3, 2, 2, 1, 3, 4, 1, 1, 2, 1, 2, 2, 2, 4, 0, 5, 5, 0, 0 }, \
    { 3, 2, 1, 1, 3, 5, 2, 1, 2, 0, 2, 2, 1, 4, 0, 5, 5, 0, 0 } }
#elif defined(PET_PIG)
  #define PET_ID            7
  #define PET_NAME          "pig"
  #define PET_PKT_TYPE      0xF0
  #undef  PET_PKT_VER
  #define PET_PKT_VER       4
  #undef  PET_TREAT
  #define PET_TREAT         1
  #undef  PET_MIN_SHADE
  #define PET_MIN_SHADE     1
  #define PET_STATE_FILE    "/pet_pig"
  #define PET_ART           "WolfArt_pig.h"
  #define PET_BARK_TEXT     "OINK!"
  #define PET_CALL_PUP      "oink~"
  #define PET_CALL_ADULT    "OINK OINK!"
  #define PET_CALL_MOON     0
  #define PET_PREY          3
  #define PET_FLYER         1
  #define PET_WEIGHTS { \
    { 3, 1, 3, 3, 4, 2, 1, 2, 2, 4, 3, 2, 4, 0, 5, 0, 0, 0, 0 }, \
    { 3, 1, 3, 4, 3, 2, 1, 2, 2, 3, 3, 2, 3, 0, 5, 0, 0, 0, 0 }, \
    { 3, 1, 2, 4, 3, 3, 2, 2, 2, 1, 4, 2, 1, 0, 6, 0, 0, 0, 0 } }
#elif defined(PET_DONKEY)
  #define PET_ID            8
  #define PET_NAME          "donkey"
  #define PET_PKT_TYPE      0xF0
  #undef  PET_PKT_VER
  #define PET_PKT_VER       5
  #undef  PET_TREAT
  #define PET_TREAT         1
  #define PET_STATE_FILE    "/pet_donkey"
  #define PET_ART           "WolfArt_donkey.h"
  #define PET_BARK_TEXT     "HEE-HAW!"
  #define PET_CALL_PUP      "eee~"
  #define PET_CALL_ADULT    "HEE-HAW!"
  #define PET_CALL_MOON     0
  #define PET_PREY          0
  #define PET_FLYER         0
  #define PET_WEIGHTS { \
    { 3, 2, 3, 2, 4, 2, 3, 3, 2, 3, 3, 2, 4, 0, 0, 0, 0, 0, 0 }, \
    { 3, 2, 3, 3, 3, 2, 4, 3, 2, 2, 3, 2, 3, 0, 0, 0, 0, 0, 0 }, \
    { 3, 2, 2, 3, 3, 3, 5, 3, 3, 1, 3, 2, 2, 0, 0, 0, 0, 0, 0 } }
#elif defined(PET_UNICORN)
  #define PET_ID            9
  #define PET_NAME          "unicorn"
  #define PET_PKT_TYPE      0xF0
  #undef  PET_PKT_VER
  #define PET_PKT_VER       6
  #undef  PET_TREAT
  #define PET_TREAT         1
  #define PET_STATE_FILE    "/pet_unicorn"
  #define PET_ART           "WolfArt_unicorn.h"
  #define PET_BARK_TEXT     "NEIGH!"
  #define PET_CALL_PUP      "neigh~"
  #define PET_CALL_ADULT    "NEEIIGH!"
  #define PET_CALL_MOON     1
  #undef  PET_MAGIC
  #define PET_MAGIC         0
  #define PET_PREY          0
  #define PET_FLYER         0
  #define PET_WEIGHTS { \
    { 3, 2, 4, 1, 4, 2, 2, 3, 2, 3, 2, 2, 4, 0, 0, 0, 0, 0, 2 }, \
    { 3, 2, 4, 2, 3, 2, 3, 3, 2, 2, 2, 2, 3, 0, 0, 0, 0, 0, 3 }, \
    { 3, 2, 3, 2, 3, 3, 4, 3, 3, 1, 3, 2, 2, 0, 0, 0, 0, 0, 3 } }
#elif defined(PET_DRAGON)
  #define PET_ID            10
  #define PET_NAME          "dragon"
  #define PET_PKT_TYPE      0xF0
  #undef  PET_PKT_VER
  #define PET_PKT_VER       7
  #undef  PET_EGG
  #define PET_EGG           1
  #undef  PET_TREAT
  #define PET_TREAT         0
  #define PET_STATE_FILE    "/pet_dragon"
  #define PET_ART           "WolfArt_dragon.h"
  #define PET_BARK_TEXT     "ROAR!"
  #define PET_CALL_PUP      "rar~"
  #define PET_CALL_ADULT    "ROOAAR!"
  #define PET_CALL_MOON     0
  #undef  PET_FIRE
  #define PET_FIRE          0
  #undef  PET_EGG_STYLE
  #define PET_EGG_STYLE     1
  #define PET_PREY          0
  #define PET_FLYER         0
  #define PET_WEIGHTS { \
    { 3, 2, 3, 2, 4, 2, 2, 3, 2, 3, 2, 2, 3, 3, 0, 0, 0, 2, 0 }, \
    { 3, 2, 3, 3, 3, 2, 3, 3, 2, 2, 2, 2, 3, 3, 0, 0, 0, 3, 0 }, \
    { 3, 2, 2, 3, 3, 3, 4, 3, 3, 1, 3, 2, 2, 3, 0, 0, 0, 4, 0 } }
#elif defined(PET_KITSUNE)
  #define PET_ID            11
  #define PET_NAME          "kitsune"
  #define PET_PKT_TYPE      0xF0
  #undef  PET_PKT_VER
  #define PET_PKT_VER       8
  #undef  PET_TREAT
  #define PET_TREAT         0
  #define PET_STATE_FILE    "/pet_kitsune"
  #define PET_ART           "WolfArt_kitsune.h"
  #define PET_BARK_TEXT     "YIP!"
  #define PET_CALL_PUP      "kon~"
  #define PET_CALL_ADULT    "KON KON!"
  #define PET_CALL_MOON     1
  #undef  PET_MAGIC
  #define PET_MAGIC         1
  #define PET_PREY          0
  #define PET_FLYER         0
  #define PET_WEIGHTS { \
    { 2, 2, 2, 2, 4, 1, 1, 2, 2, 3, 2, 2, 4, 4, 0, 0, 0, 0, 2 }, \
    { 2, 2, 2, 3, 3, 1, 2, 2, 2, 2, 2, 2, 3, 4, 0, 0, 0, 0, 3 }, \
    { 2, 2, 1, 3, 3, 2, 3, 2, 2, 1, 3, 2, 1, 5, 0, 0, 0, 0, 3 } }
#elif defined(PET_GRIFFIN)
  #define PET_ID            12
  #define PET_NAME          "griffin"
  #define PET_PKT_TYPE      0xF0
  #undef  PET_PKT_VER
  #define PET_PKT_VER       9
  #undef  PET_EGG
  #define PET_EGG           1
  #undef  PET_TREAT
  #define PET_TREAT         2
  #define PET_STATE_FILE    "/pet_griffin"
  #define PET_ART           "WolfArt_griffin.h"
  #define PET_BARK_TEXT     "SKREE!"
  #define PET_CALL_PUP      "pip~"
  #define PET_CALL_ADULT    "SKREEE!"
  #define PET_CALL_MOON     0
  #undef  PET_MAGIC
  #define PET_MAGIC         2
  #define PET_PREY          0
  #define PET_FLYER         0
  #define PET_WEIGHTS { \
    { 3, 3, 3, 1, 4, 3, 2, 2, 2, 3, 2, 3, 3, 3, 0, 0, 0, 0, 2 }, \
    { 3, 3, 3, 2, 3, 3, 3, 2, 2, 2, 2, 3, 3, 4, 0, 0, 0, 0, 2 }, \
    { 3, 3, 2, 2, 3, 4, 4, 2, 3, 1, 3, 3, 2, 4, 0, 0, 0, 0, 3 } }
#elif defined(PET_PHOENIX)
  #define PET_ID            13
  #define PET_NAME          "phoenix"
  #define PET_PKT_TYPE      0xF0
  #undef  PET_PKT_VER
  #define PET_PKT_VER       10
  #undef  PET_EGG
  #define PET_EGG           1
  #undef  PET_TREAT
  #define PET_TREAT         0
  #define PET_STATE_FILE    "/pet_phoenix"
  #define PET_ART           "WolfArt_phoenix.h"
  #define PET_BARK_TEXT     "CAW!"
  #define PET_CALL_PUP      "tweet~"
  #define PET_CALL_ADULT    "SKREEE!"
  #define PET_CALL_MOON     1
  #undef  PET_FIRE
  #define PET_FIRE          1
  #undef  PET_EGG_STYLE
  #define PET_EGG_STYLE     2
  #define PET_PREY          0
  #define PET_FLYER         0
  #define PET_WEIGHTS { \
    { 2, 3, 3, 1, 4, 3, 2, 3, 2, 3, 2, 3, 3, 0, 0, 0, 0, 2, 0 }, \
    { 2, 3, 3, 1, 3, 3, 3, 3, 2, 2, 2, 3, 3, 0, 0, 0, 0, 3, 0 }, \
    { 2, 3, 3, 1, 3, 4, 4, 3, 3, 1, 2, 3, 2, 0, 0, 0, 0, 4, 0 } }
#elif defined(PET_BUTTERFLY)
  #undef  PET_INSECT
  #define PET_INSECT        1
  #undef  PET_HIDDEN_FROM
  #undef  PET_HIDDEN_TO
  #define PET_HIDDEN_FROM   6
  #define PET_HIDDEN_TO     9
  #define PET_ID            4
  #define PET_NAME          "butterfly"
  #define PET_PKT_TYPE      0xE0
  #define PET_STATE_FILE    "/pet_butterfly"
  #define PET_ART           "WolfArt_butterfly.h"
  #define PET_BARK_TEXT     ""
  #define PET_CALL_PUP      ""
  #define PET_CALL_ADULT    ""
  #define PET_CALL_MOON     0
  #define PET_PREY          0
  #define PET_FLYER         0
  #define PET_WEIGHTS { {1,1,1,1,1,1,1,1,1,1,1,1,1,1, 0, 0, 0 }, {1,1,1,1,1,1,1,1,1,1,1,1,1,1, 0, 0, 0 }, {1,1,1,1,1,1,1,1,1,1,1,1,1,1, 0, 0, 0 } }
#else                                    // the wolf
  #define PET_ID            0
  #define PET_NAME          "wolf"
  #define PET_PKT_TYPE      0xA0
  #define PET_STATE_FILE    "/wolf_pet"
  #define PET_ART           "WolfArt_wolf.h"
  #define PET_BARK_TEXT     "WOOF!"
  #define PET_CALL_PUP      "aroo~"
  #define PET_CALL_ADULT    "AWOOOO~"
  #define PET_CALL_MOON     1
  #define PET_PREY          0
  #define PET_FLYER         0
  #define PET_WEIGHTS { \
    { 3, 2, 3, 2, 4, 1, 1, 2, 2, 4, 2, 2, 4, 0, 0, 0, 0, 0, 0 }, \
    { 3, 2, 3, 3, 3, 1, 2, 2, 2, 3, 2, 2, 3, 0, 0, 0, 0, 0, 0 }, \
    { 3, 2, 2, 3, 3, 2, 3, 2, 2, 1, 3, 2, 1, 0, 0, 0, 0, 0, 0 } }
#endif
