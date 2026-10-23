#!/bin/bash
# usage: tools/preview.sh <species> <PETFLAG>   e.g. donkey DONKEY   -> /tmp/claude-0/pv/<species>.png
set -e
cd "$(dirname "$0")/.."
sp=$1; flag=$2; out=${3:-/tmp/claude-0/pv}; mkdir -p $out
python3 tools/gen_wolf_art.py --species $sp
g++ -std=c++17 -I src -I /tmp/mc/src -DPET_$flag=1 -o $out/hs_$sp test/host_sim.cpp src/WolfCore.cpp src/WolfScene.cpp src/WolfInsect.cpp 2>&1 | grep -E "error" | head || true
$out/hs_$sp sheet $out/$sp.txt >/dev/null
python3 tools/frames_to_png.py $out/$sp.txt $out/${sp}_sheet.png --cols 5 --scale 3 --filter pose_st2
python3 tools/frames_to_png.py $out/$sp.txt $out/${sp}_all.png --cols 5 --scale 2 --filter stage
