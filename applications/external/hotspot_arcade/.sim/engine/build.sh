#!/usr/bin/env bash
# Compile the real game engine to WebAssembly for the local harness.
#
#   sim/engine/build.sh          normal build
#   sim/engine/build.sh --asan   + AddressSanitizer/UBSan
#
# The --asan build is the point of hosting the engine off-target: it indexes fixed
# arrays by player id (_p[pid], ready[HA_MAX_PLAYERS+1], DUEL_MAX_CELLS), and on the
# ESP32 an out-of-bounds write silently corrupts a neighbour. Here it aborts at the line.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$REPO/sim/web"
mkdir -p "$OUT"

if ! command -v emcc >/dev/null 2>&1; then
    echo "ERROR: emcc not found. Install emscripten (brew install emscripten)." >&2
    exit 1
fi

EXPORTS='["_ha_reset","_ha_tick","_ha_input","_ha_disconnect","_ha_select_game","_ha_trivia_clear","_ha_trivia_add_topic","_ha_trivia_add_q","_ha_round_end","_ha_reset_scores","_ha_drain","_ha_content_clear","_ha_content_pack","_ha_content_item"]'

FLAGS=(-std=c++17 -O2 -sALLOW_MEMORY_GROWTH=1)
if [ "${1:-}" = "--asan" ]; then
    # ASan needs room to live; the default 16 MB heap is not enough.
    FLAGS=(-std=c++17 -O1 -g -fsanitize=address,undefined -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728)
    echo "==> building with ASan/UBSan"
fi

emcc "$REPO/sim/engine/ha_sim.cpp" \
    -I "$REPO/sim/engine" \
    "${FLAGS[@]}" \
    -sMODULARIZE=1 \
    -sEXPORT_ES6=1 \
    -sENVIRONMENT=web,node \
    -sEXPORTED_FUNCTIONS="$EXPORTS" \
    -sEXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
    -o "$OUT/engine.js"

echo "==> built $OUT/engine.js"
