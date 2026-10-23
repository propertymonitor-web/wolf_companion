#pragma once
// The text at the top-left of the pet screen:  "F14mo Rex Wolf 2/3"  (sex+age, the owner's name, the kind of pet, which pet).
// Whatever does not fit before the battery symbol is dropped, longest first.  Used by both scenes.
#include <helpers/ui/DisplayDriver.h>
#include "WolfCore.h"
#include <stdio.h>

inline void petHudText(DisplayDriver& d, const WolfGame& g, const Wolf& w, int right_limit_px, char* out, size_t cap) {
  uint8_t sp = WolfGame::speciesOf(w);
  char head[24];
  if (WolfGame::isEgg(w)) snprintf(head, sizeof(head), "EGG %uh", (unsigned)((WolfGame::eggLeftSecs(w) + 3599) / 3600));
  else snprintf(head, sizeof(head), "%s%umo", w.sex ? "F" : "M", (unsigned)WolfGame::ageMonths(w));
  const char* nm = w.name;
  char idx[12] = "";
  if (!WolfGame::isEgg(w) && g.count > 1) snprintf(idx, sizeof(idx), " %u/%u", (unsigned)(g.current + 1), (unsigned)g.count);
  const bool egg = WolfGame::isEgg(w);
  const char* kinds[3] = { egg ? "" : petTitleOf(sp), egg ? "" : petShortOf(sp), "" };
  // try, in order: everything / short kind name / no pet index / no kind
  struct Try { int kind; bool index; };
  static const Try order[] = { {0, true}, {0, false}, {1, true}, {1, false}, {2, true}, {2, false} };
  for (unsigned t = 0; t < sizeof(order) / sizeof(order[0]); t++) {
    const char* k = kinds[order[t].kind];
    snprintf(out, cap, "%s%s%s%s%s%s", head, *nm ? " " : "", nm, *k ? " " : "", k, order[t].index ? idx : "");
    if (d.getTextWidth(out) <= right_limit_px) return;
  }
  snprintf(out, cap, "%s", head);     // nothing else fits (a very long head on a tiny screen)
}
