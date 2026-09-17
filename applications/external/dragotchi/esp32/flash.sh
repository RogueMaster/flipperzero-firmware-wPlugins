#!/usr/bin/env bash
# Flash MicroPython + the Dragotchi WiFi reporter onto a Flipper ESP32-S2
# WiFi devboard. See esp32/README.md for the full procedure.
#
# Usage:  ./flash.sh [/dev/ttyACMx]
# The board must be in ROM download mode: hold BOOT, tap RST, release BOOT.
set -euo pipefail

PORT="${1:-/dev/esp32-dl}"
MPY_VERSION="ESP32_GENERIC_S2-20260824-v1.29.0"
MPY_URL="https://micropython.org/resources/firmware/${MPY_VERSION}.bin"
HERE="$(cd "$(dirname "$0")" && pwd)"
BIN="${HERE}/${MPY_VERSION}.bin"

command -v esptool >/dev/null 2>&1 || { echo "esptool not found (pip install esptool)"; exit 1; }

[ -f "$BIN" ] || { echo "Downloading MicroPython ${MPY_VERSION}..."; curl -fsSL -o "$BIN" "$MPY_URL"; }

echo "Flashing MicroPython to $PORT (must be in download mode)..."
# Default baud (no baud switch) is the most reliable over the S2's native USB.
esptool --chip esp32s2 --port "$PORT" --before no-reset --after hard-reset \
    write-flash --erase-all 0x1000 "$BIN"

echo "Waiting for the MicroPython REPL to come up..."
sleep 4
echo "Now upload the reporter with:  ./upload_main.py <repl-port>"
