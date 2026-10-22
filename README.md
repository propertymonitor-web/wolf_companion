# wolf_companion

A MeshCore **companion radio** firmware for the **Heltec V4 (OLED)** where the main screen is a
pocket wolf pup. It lives on your radio, grows up, eats treats, barks at your messages, visits the
wolves on other radios running this firmware, makes friends, and (eventually, by mutual consent)
has pups.

Everything else on the radio is stock MeshCore: the wolf is one extra page at the front of the
normal home-screen pages.

## What the wolf does

| | |
|---|---|
| **Look** | Full-bodied side-view wolf, walks back and forth along a ground line. Three growth stages (pup, juvenile, adult). Sex is picked at random the first time the radio boots. |
| **Genes** | The wolf's genome is the **last 8 hex characters of the radio's public key** (the last 4 bytes). Each of the 8 characters is one gene: coat darkness, ear shape, tail shape, markings (socks / dark muzzle / chest blaze / saddle / spots), tail tip, fluffy ruff, eye shape, spot layout. No two radios look alike. |
| **Growing up** | **7 wolf-months per real day** of radio uptime. Pup under 6 months, adult at 24 months (about 3.4 days powered on). Age only advances while the radio is on. |
| **Treats** | Every **10 to 15** received messages (any channel or DM) a bone drops from the top of the screen; the wolf trots over and eats it. The count is re-rolled after each treat. |
| **Barking** | After a random **5 to 20** messages (re-rolled each time) the wolf turns its head toward you, opens its mouth and barks ("WOOF!"). |
| **Screen** | The wolf screen **never times out**. Double-click the button to turn the screen off; any click wakes it. Scrolling to any other page gives the normal timeout again. |

## Visiting, friends and breeding

Radios running this firmware find each other with tiny zero-hop LoRa control packets (they only
reach radios that can hear you directly, and stock MeshCore ignores them).

* **Visits.** Every 10 to 25 minutes one of your wolves asks to visit. If a wolf radio answers, your wolf **walks off the screen edge** and appears on the host's screen, entering from a random side. While it is away your screen says "off visiting...".
* **Always home.** The wolf always walks back in through one edge of the screen (which edge changes from time to time), whether the visit ended normally, after playing, after mating, after looking after pups, or because the two radios lost contact (no word for 25 seconds sends everyone home).
* **Memory.** Each radio remembers how many times each wolf has visited each of its wolves. **First meeting:** polite sniff and tail wags. **Second visit:** play bows and hops. **Third visit and after (or a dad visiting his own pups):** the two trade places across the screen. Dads visiting their own pups stay longer to help look after them.
* **Digital consent.** Every wolf rolls its own private number (**3 to 7**) of visits it needs from a given wolf before it will even consider mating with it. Mating is only considered when **both** wolves have reached their own number, so it can **never** happen on a first visit. The consent is exchanged between the two radios (each tells the other whether it is ready), then there is a 60% chance per visit. Only adult male + adult female, never parent/child or siblings, never a mother who is pregnant or still recovering.
* **Pups.** Gestation is about 7 hours (2 wolf-months), litter of 1 to 3, born on the mother's radio. Each pup inherits every gene from its mother or its father at random (with a small chance of a mutation), so it carries both parents' traits, and gets its own consent number. A radio keeps up to 6 wolves.
* **Seeing your wolves.** Long-press on the wolf screen to switch to the next wolf in your pack; a popup shows `sex age #genome`.

## Controls

| Button | Wolf screen | Other screens |
|---|---|---|
| click | next page | next page |
| double click | **screen off** | previous page |
| long press | next wolf in your pack | as stock MeshCore |

## Building

You can't build MeshCore without PlatformIO, so the repo ships a GitHub Action:

1. **Actions > Build wolf firmware > Run workflow** (leave the defaults for the Heltec V4 BLE companion).
2. When it is green, open the run, download the artifact, unzip it.
3. Flash `firmware-merged.bin` with <https://flasher.meshcore.io> (Custom Firmware) for a first install, or `firmware.bin` as an update.

The action runs the game-rule tests first, applies the wolf to MeshCore commit `81dbae5` (pinned, because the UI patch is made against it) and builds `heltec_v4_companion_radio_ble`. USB and WiFi companion variants are in the dropdown too.

To build locally instead: `python3 tools/apply_wolf.py --meshcore /path/to/MeshCore`, then `pio run -e heltec_v4_companion_radio_ble -t mergebin` in the MeshCore folder.

## Repo layout

```
src/WolfCore.*     game rules (genes, ageing, visits, consent, breeding, packets, save format) - no hardware
src/WolfScene.*    animation and drawing for the 128x64 OLED
src/WolfArt.h      sprite parts (generated by tools/gen_wolf_art.py)
src/WolfModule.*   glue to MeshCore (identity, radio packets, flash storage)
patches/           the small UITask patch (new first page, no timeout, double-click off)
test/host_sim.cpp  PC test harness: rules checks and frame dumps (tools/frames_to_png.py makes pictures)
```

Tuning knobs (ages, visit lengths, consent range, mating chance, message counts) are `#define`s at the top of `src/WolfCore.h`.

## Honest notes

* Written without access to a Heltec V4: the game rules were tested on a PC and the drawing was checked as rendered frames, but timing and looks on the real OLED are yours to confirm.
* The wolf is saved to the radio's flash every 30 minutes and on every event. A power cut can lose up to 30 minutes of growth.
* A wolf visit needs both radios running this firmware and in direct radio range of each other.
