#pragma once
// Which pet is this firmware?  Build with exactly one of -DPET_FOX=1  -DPET_CAT=1  -DPET_BEAR=1
// (nothing = the wolf).  Everything that differs between pets lives here: the radio packet family
// (so different pets never confuse each other), the save-file name, the sprite art, what the pet
// says, and how likely each activity is.

#define PET_EV_BUNNY   0     // activity numbers (must match WolfScene.cpp)
#define PET_EV_COUNT   14
#define PET_MIN_SHADE  0     // lowest coat density (bears are never hollow)
#define PET_INSECT     0     // 1 = an insect (own scene, own life cycle)
#define PET_HIDDEN_FROM 0    // months during which the pet stays put (a chrysalis does not go visiting)
#define PET_HIDDEN_TO   0

// weights, per growth stage (pup, adolescent, adult), for the 14 activities in this order:
//  prey-visit  bird  flyer  dig  sleep  rest  call  shake  yawn  chase-tail  sniff  stretch  zoomies  pounce

#if defined(PET_FOX)
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
    { 2, 2, 2, 2, 4, 1, 1, 2, 2, 3, 2, 2, 4, 4 }, \
    { 2, 2, 2, 3, 3, 1, 2, 2, 2, 2, 2, 2, 3, 4 }, \
    { 2, 2, 1, 3, 3, 2, 3, 2, 2, 1, 3, 2, 1, 5 } }
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
    { 2, 2, 3, 0, 5, 3, 1, 1, 3, 4, 2, 3, 4, 4 }, \
    { 2, 2, 3, 0, 4, 3, 1, 1, 3, 3, 2, 3, 3, 4 }, \
    { 2, 2, 2, 0, 4, 4, 1, 1, 3, 1, 2, 3, 2, 5 } }
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
    { 3, 1, 4, 2, 4, 2, 1, 2, 2, 3, 3, 2, 2, 0 }, \
    { 3, 1, 4, 3, 3, 2, 1, 2, 2, 2, 3, 2, 1, 0 }, \
    { 3, 1, 3, 4, 3, 3, 2, 2, 3, 1, 4, 2, 0, 0 } }
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
  #define PET_WEIGHTS { {1,1,1,1,1,1,1,1,1,1,1,1,1,1}, {1,1,1,1,1,1,1,1,1,1,1,1,1,1}, {1,1,1,1,1,1,1,1,1,1,1,1,1,1} }
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
    { 3, 2, 3, 2, 4, 1, 1, 2, 2, 4, 2, 2, 4, 0 }, \
    { 3, 2, 3, 3, 3, 1, 2, 2, 2, 3, 2, 2, 3, 0 }, \
    { 3, 2, 2, 3, 3, 2, 3, 2, 2, 1, 3, 2, 1, 0 } }
#endif
