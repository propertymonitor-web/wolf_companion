#!/bin/bash
# build and test every pet on the PC (address sanitizer on).  usage: test/run_all.sh [MeshCore/src]
MC=${1:-/tmp/mc/src}; OUT=${TMPDIR:-/tmp}/pet_tests; mkdir -p $OUT; cd "$(dirname "$0")/.."
rc=0
for sp in WOLF FOX CAT BEAR BUTTERFLY GATOR CROC PIG DONKEY UNICORN DRAGON KITSUNE GRIFFIN PHOENIX HORSE; do
  flag=-DPET_$sp=1; [ $sp = WOLF ] && flag=""
  ( g++ -std=c++17 -g -fsanitize=address,undefined -I src -I $MC -I test $flag -o $OUT/hs_$sp test/host_sim.cpp src/WolfCore.cpp src/WolfScene.cpp src/WolfInsect.cpp 2>&1 | grep -E "error"
    r="$($OUT/hs_$sp logic 2>&1 | tail -1) | $($OUT/hs_$sp mix 2>&1 | grep -c STUCK) stuck | egg: $($OUT/hs_$sp egg 2>&1 | grep -c FAIL) fail | mirror: $($OUT/hs_$sp mirror $OUT/m_$sp.txt 2>&1 | grep -c -i fail) fail"
    echo "$sp: $r" ) &
done
wait
g++ -std=c++17 -g -fsanitize=address,undefined -I src -I $MC -I test -DPET_MYSTERY=1 test/mystery_sim.cpp src/WolfCore.cpp src/Mystery[A-Z]*.cpp -o $OUT/mystery_sim 2>&1 | grep -E "error"
echo "MYSTERY: $($OUT/mystery_sim 2>&1 | tail -1)"
for f in "" "-DPET_FOX=1" "-DPET_BUTTERFLY=1" "-DPET_DRAGON=1" "-DPET_HORSE=1" "-DPET_MYSTERY=1"; do
  g++ -std=c++17 -g -fsanitize=address,undefined -I src -I $MC -I test $f test/cross_sim.cpp src/WolfCore.cpp -o $OUT/cross_sim 2>&1 | grep -E "error"
  echo "CROSS ${f:-wolf}: $($OUT/cross_sim 2>&1 | tail -1)"
done
g++ -std=c++17 -g -fsanitize=address,undefined -I src -I $MC -I test test/hud_sim.cpp src/WolfCore.cpp -o $OUT/hud_sim 2>&1 | grep -E "error"
echo "HUD: $($OUT/hud_sim 2>&1 | tail -1)"
