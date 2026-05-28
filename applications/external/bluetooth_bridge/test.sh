#!/usr/bin/env bash
set -euo pipefail

echo "=== Static checks ==="

# Check that entry point matches application.fam
ENTRY=$(grep 'entry_point=' application.fam | sed 's/.*entry_point="\([^"]*\)".*/\1/')
if ! grep -q "int32_t ${ENTRY}(void\* p)" bluetooth_bridge.c; then
    echo "FAIL: entry point '${ENTRY}' not found in bluetooth_bridge.c"
    exit 1
fi
echo "PASS: entry point matches"

# Check for common C issues
if grep -n 'malloc(' bluetooth_bridge.c | grep -v 'free\|// ' | head -5 > /dev/null; then
    MALLOCS=$(grep -c 'malloc(' bluetooth_bridge.c)
    FREES=$(grep -c 'free(' bluetooth_bridge.c)
    if [ "$MALLOCS" -gt "$FREES" ]; then
        echo "WARN: $MALLOCS malloc(s) but only $FREES free(s) — check for leaks"
    else
        echo "PASS: malloc/free balance ($MALLOCS/$FREES)"
    fi
fi

# Check that all furi_record_open have matching furi_record_close
OPENS=$(grep -c 'furi_record_open' bluetooth_bridge.c)
CLOSES=$(grep -c 'furi_record_close' bluetooth_bridge.c)
if [ "$OPENS" -ne "$CLOSES" ]; then
    echo "FAIL: $OPENS furi_record_open but $CLOSES furi_record_close"
    exit 1
fi
echo "PASS: furi_record open/close balance ($OPENS/$CLOSES)"

# Check serial acquire/release balance
ACQUIRES=$(grep -c 'serial_control_acquire' bluetooth_bridge.c)
RELEASES=$(grep -c 'serial_control_release' bluetooth_bridge.c)
if [ "$ACQUIRES" -ne "$RELEASES" ]; then
    echo "FAIL: $ACQUIRES serial acquire but $RELEASES release"
    exit 1
fi
echo "PASS: serial acquire/release balance ($ACQUIRES/$RELEASES)"

# Check that bt_profile_restore_default is called on cleanup
if ! grep -q 'bt_profile_restore_default' bluetooth_bridge.c; then
    echo "FAIL: bt_profile_restore_default not called — BLE profile leak"
    exit 1
fi
echo "PASS: BLE profile restored on exit"

echo ""
echo "=== Build check ==="
ufbt build
echo ""
echo "=== All checks passed ==="
