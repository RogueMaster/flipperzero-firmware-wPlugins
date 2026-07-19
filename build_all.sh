#!/bin/bash
# Build all 13 FAP apps and report results.
# Usage: ./build_all.sh [--lint]

set -euo pipefail

APPS=(
    badusb_pro
    ble_scanner
    ccid_emulator
    evil_ble
    flipperpwn
    hid_exfil
    nfc_fuzzer
    rayhunter_client
    rogue_ap_detector
    spi_flash_dump
    subghz_jammer
    subghz_spectrum
    uart_sniff
)

TASK="build"
if [[ "${1:-}" == "--lint" ]]; then
    TASK="lint"
fi

PASS=0
FAIL=0
FAILED_APPS=()

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

for app in "${APPS[@]}"; do
    printf "%-24s " "$app"
    if (cd "$SCRIPT_DIR/$app" && ufbt "$TASK" 2>&1 | tail -1) ; then
        echo "  OK"
        PASS=$((PASS + 1))
    else
        echo "  FAIL"
        FAIL=$((FAIL + 1))
        FAILED_APPS+=("$app")
    fi
done

echo ""
echo "Results: $PASS passed, $FAIL failed out of ${#APPS[@]} apps"

if [[ $FAIL -gt 0 ]]; then
    echo "Failed apps: ${FAILED_APPS[*]}"
    exit 1
fi
