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
ASSETS_WEB="$REPO/flipper/hotspot-arcade/assets/web"
ASSETS_PACKS="$REPO/flipper/hotspot-arcade/assets/packs"
WEB_DIST="$REPO/web/dist"
CORE_VER="2.0.17"
# The ESP32-C5 is not in the 2.x cores at all, so it builds against a 3.x one.
CORE_VER_C5="${CORE_VER_C5:-3.3.11}"

# Boards to build, "<fqbn>|<assets subdir>[|<core version>]". All build from the same
# sketch; the fap bundles one image set per board and the on-device board picker chooses
# which to flash. Adding a board is one more line here (+ its flash_*.txt and a picker row).
#
# The core version is optional and defaults to CORE_VER. The C5 needs it: that chip only
# exists in the 3.x cores, while the S2/WROOM images stay on the pinned 2.0.17 so this
# doesn't quietly re-cut them on a core with different WiFi/USB behaviour.
BOARDS=(
    "esp32:esp32:esp32s2:PartitionScheme=huge_app,PSRAM=enabled|official_devboard"
    "esp32:esp32:esp32|wroom"
    "esp32:esp32:esp32c5:PartitionScheme=huge_app,CDCOnBoot=default|c5|$CORE_VER_C5"
)

# Short tag used in the flashed filenames, per assets subdir. The Flipper shows the
# filename while flashing, and "hotspot-arcade-fw.ino.bootloader.bin" is far wider
# than the screen, so the images are renamed on the way into assets/firmware/.
board_tag() {
    case "$1" in
        official_devboard) echo "s2" ;;
        *)                 echo "$1" ;;
    esac
}

# The three arduino-built images (boot_app0.bin comes from the core, not the build).
IMAGES=(
    "hotspot-arcade-fw.ino.bootloader.bin"
    "hotspot-arcade-fw.ino.partitions.bin"
    "hotspot-arcade-fw.ino.bin"
)

# Build name -> flashed name. Keep in step with each board's flash_*.txt.
flashed_name() {
    local img="$1" tag="$2"
    case "$img" in
        *.ino.bootloader.bin) echo "ha-boot-$tag.bin" ;;
        *.ino.partitions.bin) echo "ha-part-$tag.bin" ;;
        boot_app0.bin)        echo "ha-app0-$tag.bin" ;;
        *.ino.bin)            echo "ha-fw-$tag.bin" ;;
        *)                    echo "$img" ;;
    esac
}

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

# Which esp32 core version arduino-cli currently has installed (empty if none).
installed_core() {
    [ -n "$ACLI" ] || return 0
    "$ACLI" core list 2>/dev/null | awk '$1 == "esp32:esp32" { print $2; exit }'
}

# --- locate the core's boot_app0.bin (only needed when actually building) ---
find_boot_app0() {
    local ver="${1:-$CORE_VER}"
    local rel="packages/esp32/hardware/esp32/$ver/tools/partitions/boot_app0.bin"
    local roots=()
    [ -n "${ARDUINO_DIRECTORIES_DATA:-}" ] && roots+=("$ARDUINO_DIRECTORIES_DATA")
    roots+=("$HOME/Library/Arduino15" "$HOME/.arduino15")
    for r in "${roots[@]}"; do
        if [ -f "$r/$rel" ]; then echo "$r/$rel"; return 0; fi
    done
    return 1
}
# boot_app0 is resolved per board below, since they can build against different cores.

# --- build each board and populate assets/firmware/<subdir>/ ---
# flash_official.txt / flash_wroom.txt are committed as the source of truth; only the
# .bin images (and boot_app0) are (re)generated here.
for entry in "${BOARDS[@]}"; do
    IFS='|' read -r fqbn subdir board_core <<< "$entry"
    board_core="${board_core:-$CORE_VER}"
    out="$ASSETS_FW/$subdir"
    BOOT_APP0=""
    if [ -n "$ACLI" ]; then
        # arduino-cli keeps ONE version of a platform installed at a time, and these
        # boards deliberately build against different ones: the C5 exists only in the
        # 3.x cores, while the S2/WROOM stay pinned to 2.0.17 for its WiFi/USB
        # behaviour. So the core has to be swapped mid-run, exactly as build.yml does
        # by hand between its two compile steps. BOARDS is grouped by core version, so
        # this swaps at most once per run. Without it the script simply cannot produce
        # a full image set on a dev machine: it reached the C5 with 2.0.17 installed
        # and died looking for a boot_app0.bin that only ships with the 3.x core.
        if [ "$(installed_core)" != "$board_core" ]; then
            echo "==> switching to the esp32 $board_core core (for $subdir)"
            "$ACLI" core install "esp32:esp32@$board_core"
        fi
        BOOT_APP0="$(find_boot_app0 "$board_core" || true)"
        if [ -z "$BOOT_APP0" ]; then
            echo "ERROR: could not find boot_app0.bin for the esp32 $board_core core." >&2
            echo "       install it (arduino-cli core install esp32:esp32@$board_core)" >&2
            echo "       or set ARDUINO_DIRECTORIES_DATA to the arduino data dir." >&2
            exit 1
        fi
    fi
    if [ -n "$ACLI" ]; then
        build="$ESP_BUILD/$subdir"
        echo "==> Building $subdir firmware ($fqbn)"
        "$ACLI" compile --fqbn "$fqbn" \
            --libraries "$REPO/esp32/libs" \
            --output-dir "$build" \
            "$ESP_DIR"
    else
        echo "==> arduino-cli not found; keeping the committed $subdir images"
        build="$out"
    fi
    tag="$(board_tag "$subdir")"
    if [ -n "$ACLI" ]; then
        # Fresh build: the images are still arduino-named in $build, and get their
        # short flashed names as they are copied into assets/firmware/<subdir>/.
        for img in "${IMAGES[@]}"; do
            if [ ! -f "$build/$img" ]; then
                echo "ERROR: missing $subdir ESP image $build/$img" >&2
                exit 1
            fi
        done
        mkdir -p "$out"
        for img in "${IMAGES[@]}"; do
            cp "$build/$img" "$out/$(flashed_name "$img" "$tag")"
        done
        cp "$BOOT_APP0" "$out/$(flashed_name boot_app0.bin "$tag")"
    else
        # Fallback: the committed images are already in $out, already renamed.
        for img in "${IMAGES[@]}" boot_app0.bin; do
            fn="$(flashed_name "$img" "$tag")"
            if [ ! -f "$out/$fn" ]; then
                echo "ERROR: missing $subdir ESP image $out/$fn" >&2
                echo "       build the ESP firmware first (see CLAUDE.md) or install arduino-cli." >&2
                exit 1
            fi
        done
    fi
    if ! ls "$out"/flash_*.txt >/dev/null 2>&1; then
        echo "ERROR: $out is missing its committed flash_*.txt." >&2
        exit 1
    fi
done

# Record which ESP sources these images came from, so CI can catch a stale commit.
"$REPO/tools/asset-stamp.sh" > "$REPO/flipper/hotspot-arcade/.bundled-fw.sha256"
ls -la "$ASSETS_FW"

# --- populate assets/web/ and assets/packs/ ---
# These ship inside the fap too, so a fresh install has something to serve without any
# SD setup. Like the firmware images they are build//source copies, kept committed so
# the app catalog (which builds flipper/hotspot-arcade/ alone, without this script) gets
# them. Users can still override them from /ext/apps_data/hotspot_arcade/.
echo "==> Copying web bundle into $ASSETS_WEB"
if [ ! -f "$WEB_DIST/manifest.json" ]; then
    echo "ERROR: $WEB_DIST/manifest.json is missing. Build it first: cd web && node build.mjs" >&2
    exit 1
fi
mkdir -p "$ASSETS_WEB"
rm -f "$ASSETS_WEB"/*.gz "$ASSETS_WEB"/manifest.json
cp "$WEB_DIST/manifest.json" "$ASSETS_WEB/"
# The uncompressed index.html is a debug artifact; only the .gz files are served.
for gz in "$WEB_DIST"/*.gz; do
    [ -f "$gz" ] && cp "$gz" "$ASSETS_WEB/"
done

echo "==> Copying content packs into $ASSETS_PACKS"
rm -rf "$ASSETS_PACKS"
for gamedir in "$REPO"/packs/*/; do
    [ -d "$gamedir" ] || continue
    game=$(basename "$gamedir")
    mkdir -p "$ASSETS_PACKS/$game"
    for pack in "$gamedir"*.txt; do
        [ -f "$pack" ] && cp "$pack" "$ASSETS_PACKS/$game/"
    done
    # Translated packs live in packs/<game>/<lang>/ subdirs; mirror them so the host
    # can stream a language and fall back to the English root.
    for langdir in "$gamedir"*/; do
        [ -d "$langdir" ] || continue
        lang=$(basename "$langdir")
        for pack in "$langdir"*.txt; do
            [ -f "$pack" ] || continue
            mkdir -p "$ASSETS_PACKS/$game/$lang"
            cp "$pack" "$ASSETS_PACKS/$game/$lang/"
        done
    done
done
ls -la "$ASSETS_WEB"
ls -la "$ASSETS_PACKS"/*

# --- build the fap (bundles assets/) ---
echo "==> Running ufbt"
cd "$REPO/flipper/hotspot-arcade"
ufbt "$@"
echo "==> Done: flipper/hotspot-arcade/dist/hotspot_arcade.fap"
