#!/bin/bash
# Test that save/load order matches
echo "Checking settings save/load order..."

echo "=== SAVE ORDER ==="
grep -n "flipper_format_write" helpers/hid_device_storage.c | grep -v "if(!" | grep -E "DELIMITER|APPEND_ENTER|MODE|OUTPUT_MODE|VIBRATION|NDEF_MAX_LEN|LOG_TO_SD" | head -10

echo ""
echo "=== LOAD ORDER ==="
grep -n "flipper_format_read" helpers/hid_device_storage.c | grep -E "DELIMITER|APPEND_ENTER|MODE|OUTPUT_MODE|VIBRATION|NDEF_MAX_LEN|LOG_TO_SD|USB_DEBUG"

echo ""
echo "=== DEPRECATED READS (should be NONE) ==="
grep -n "USB_DEBUG" helpers/hid_device_storage.c | grep "flipper_format_read"

if [ $? -eq 0 ]; then
    echo "❌ ERROR: Still reading deprecated USB_DEBUG setting!"
    exit 1
else
    echo "✅ OK: Not reading deprecated USB_DEBUG"
fi

echo ""
echo "=== KEY CONSTANTS ==="
grep "define.*SETTINGS_KEY" helpers/hid_device_storage.h | grep -E "DELIMITER|APPEND|MODE|OUTPUT|VIBRATION|NDEF|LOG"
