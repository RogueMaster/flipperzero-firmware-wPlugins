#!/bin/sh
# Build and run the desktop simulator; writes tools/sim/sheet.png.
set -e
cd "$(dirname "$0")"
mkdir -p out build
rm -f out/*.pbm
c++ -std=c++20 -O1 -Wall -Wextra -Wno-unused-parameter -Istubs -I../.. sim.cpp -o build/sim
./build/sim > /dev/null
python3 -I sheet.py out sheet.png
