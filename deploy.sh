#!/usr/bin/env bash
# Build and deploy nfc_canary to a connected Flipper Zero.
#
# The SDK must match the device's API version exactly or the loader rejects the
# FAP with "Invalid file". This device runs Unleashed unlshd-089e = API 87.8,
# and ufbt's published channels only carry 090 (API 88.2) -- hence the pinned
# local SDK below. Re-pin if you update firmware.
#
# Usage: ./deploy.sh [serial_port]
set -euo pipefail

APP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FAP="$APP_DIR/dist/nfc_canary.fap"
DEST="/ext/apps/NFC/nfc_canary.fap"

# The Flipper's tty number moves around between reboots and replugs, so resolve
# it by USB ID rather than hardcoding one. An explicit argument still wins.
if [ $# -ge 1 ]; then
    PORT="$1"
else
    LINK="$(ls /dev/serial/by-id/ 2>/dev/null | grep -i flipper | head -1 || true)"
    if [ -z "$LINK" ]; then
        echo "error: no Flipper found in /dev/serial/by-id/" >&2
        echo "       check the cable (many charge-only cables carry no data)," >&2
        echo "       that the device is powered on, and that it is not wedged" >&2
        echo "       (power-cycle: hold Back + Left for ~5s)." >&2
        exit 1
    fi
    PORT="$(readlink -f "/dev/serial/by-id/$LINK")"
    echo "==> Found Flipper at $PORT"
fi

echo "==> Building"
cd "$APP_DIR"
ufbt

echo "==> Deploying to $PORT"
python3 "$APP_DIR/tools/flipper_push.py" "$PORT" "$FAP" "$DEST"

echo "==> Done. Launch from: Apps > NFC > NFC Canary"
