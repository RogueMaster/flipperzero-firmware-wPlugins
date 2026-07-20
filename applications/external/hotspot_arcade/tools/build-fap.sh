#!/usr/bin/env bash
# Build the Hotspot Arcade .fap with the ESP firmware bundled inside it.
#
# The .fap ships the ESP32-S2 firmware images via fap_file_assets (see
# application.fam): the loader extracts them to /ext/apps_assets/hotspot_arcade/ on
# launch, so a fresh install of just the .fap makes "Install Firmware" work with no
# SD setup. Firmware images are build artifacts, so this wrapper regenerates them
# into flipper/hotspot-arcade/assets/firmware/ (next to the committed flash.txt)
# before running ufbt. CI should call this instead of bare `ufbt`.
#
# Steps: build the ESP firmware (arduino-cli) -> copy its images + the core's
# boot_app0.bin into assets/firmware/ -> run ufbt.
#
# If arduino-cli isn't available it falls back to the already-built images in
# esp32/hotspot-arcade-fw/build/ (and errors if those are missing too).
#
# Usage: tools/build-fap.sh
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ESP_DIR="$REPO/esp32/hotspot-arcade-fw"
ESP_BUILD="$ESP_DIR/build"
ASSETS_FW="$REPO/flipper/hotspot-arcade/assets/firmware"
FQBN="esp32:esp32:esp32s2:PartitionScheme=huge_app"
CORE_VER="2.0.17"

# The three arduino-built images (boot_app0.bin comes from the core, not the build).
IMAGES=(
    "hotspot-arcade-fw.ino.bootloader.bin"
    "hotspot-arcade-fw.ino.partitions.bin"
    "hotspot-arcade-fw.ino.bin"
)

# --- locate arduino-cli (not on PATH by default) ---
ACLI="${ACLI:-}"
if [ -z "$ACLI" ]; then
    if command -v arduino-cli >/dev/null 2>&1; then
        ACLI="$(command -v arduino-cli)"
    elif [ -x "/private/tmp/claude-501/-Users-tarikbc/4f52fb49-2f3f-4cb5-bda7-f9bf98b3001b/scratchpad/bin/arduino-cli" ]; then
        ACLI="/private/tmp/claude-501/-Users-tarikbc/4f52fb49-2f3f-4cb5-bda7-f9bf98b3001b/scratchpad/bin/arduino-cli"
    fi
fi
# Default the core data dir to the same scratchpad if the env var is unset.
if [ -z "${ARDUINO_DIRECTORIES_DATA:-}" ] && [ -n "$ACLI" ]; then
    if [ -d "/private/tmp/claude-501/-Users-tarikbc/4f52fb49-2f3f-4cb5-bda7-f9bf98b3001b/scratchpad/arduino15" ]; then
        export ARDUINO_DIRECTORIES_DATA="/private/tmp/claude-501/-Users-tarikbc/4f52fb49-2f3f-4cb5-bda7-f9bf98b3001b/scratchpad/arduino15"
    fi
fi

# --- build the ESP firmware (or fall back to an existing build) ---
if [ -n "$ACLI" ]; then
    echo "==> Building ESP firmware with arduino-cli ($ACLI)"
    "$ACLI" compile --fqbn "$FQBN" \
        --libraries "$REPO/esp32/libs" \
        --output-dir "$ESP_BUILD" \
        "$ESP_DIR"
else
    echo "==> arduino-cli not found; using existing images in $ESP_BUILD"
fi

for img in "${IMAGES[@]}"; do
    if [ ! -f "$ESP_BUILD/$img" ]; then
        echo "ERROR: missing ESP image $ESP_BUILD/$img" >&2
        echo "       build the ESP firmware first (see CLAUDE.md) or install arduino-cli." >&2
        exit 1
    fi
done

# --- locate the core's boot_app0.bin ---
find_boot_app0() {
    local rel="packages/esp32/hardware/esp32/$CORE_VER/tools/partitions/boot_app0.bin"
    local roots=()
    [ -n "${ARDUINO_DIRECTORIES_DATA:-}" ] && roots+=("$ARDUINO_DIRECTORIES_DATA")
    roots+=("$HOME/Library/Arduino15" "$HOME/.arduino15")
    for r in "${roots[@]}"; do
        if [ -f "$r/$rel" ]; then echo "$r/$rel"; return 0; fi
    done
    return 1
}
BOOT_APP0="$(find_boot_app0 || true)"
if [ -z "$BOOT_APP0" ]; then
    echo "ERROR: could not find boot_app0.bin for the esp32 $CORE_VER core." >&2
    echo "       set ARDUINO_DIRECTORIES_DATA to the arduino data dir." >&2
    exit 1
fi

# --- populate assets/firmware/ (flash.txt is already committed there) ---
echo "==> Copying firmware images into $ASSETS_FW"
mkdir -p "$ASSETS_FW"
for img in "${IMAGES[@]}"; do
    cp "$ESP_BUILD/$img" "$ASSETS_FW/$img"
done
cp "$BOOT_APP0" "$ASSETS_FW/boot_app0.bin"
if [ ! -f "$ASSETS_FW/flash.txt" ]; then
    echo "ERROR: $ASSETS_FW/flash.txt is missing (it should be committed)." >&2
    exit 1
fi
ls -la "$ASSETS_FW"

# --- build the fap (bundles assets/) ---
echo "==> Running ufbt"
cd "$REPO/flipper/hotspot-arcade"
ufbt "$@"
echo "==> Done: flipper/hotspot-arcade/dist/hotspot_arcade.fap"
