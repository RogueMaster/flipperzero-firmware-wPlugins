#!/bin/sh
# Render every screenshot the Apps Catalog needs, without a device.
#
#   tools/shoot/shoot.sh
#
# Needs u8g2 - the same library the firmware draws with - which is not
# vendored here. It is fetched into /tmp on first run. See real_canvas.h
# for why this is a render and not a mock-up.
set -e
cd "$(dirname "$0")"
ROOT=$(cd ../.. && pwd)
U8G2=${U8G2_DIR:-/tmp/bb_u8g2}

if [ ! -d "$U8G2" ]; then
    echo "fetching u8g2 from the firmware tree..."
    tmp=$(mktemp -d)
    git clone --depth 1 -q --filter=blob:none --sparse \
        https://github.com/flipperdevices/flipperzero-firmware.git "$tmp/fw"
    (cd "$tmp/fw" && git sparse-checkout set lib/u8g2 > /dev/null)
    mkdir -p "$U8G2"
    cp "$tmp/fw"/lib/u8g2/*.c "$tmp/fw"/lib/u8g2/*.h "$U8G2"/
    rm -rf "$tmp"
fi

SRC=""
for f in "$U8G2"/*.c; do
    case "$f" in
        *u8g2_glue.c) continue ;;   # Flipper-specific, needs furi
    esac
    SRC="$SRC $f"
done

# renders, not screenshots: screenshots/ holds captures taken off a real
# device with qFlipper, and this tool must never be able to overwrite one
OUT=$ROOT/renders
mkdir -p "$OUT"
gcc -std=gnu11 -O2 -w -I "$U8G2" -I "$ROOT" -I "$ROOT/test/stubs" -I . \
    -o /tmp/bb_shoot shoot.c u8g2_unused.c "$ROOT/beepback_tables.c" $SRC -lm
/tmp/bb_shoot "$OUT"
python3 to_png.py "$OUT"
