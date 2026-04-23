#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== InfraFi Daemon Installer ==="

# Check for root
if [ "$(id -u)" -ne 0 ]; then
    echo "Error: must be run as root"
    exit 1
fi

# Detect input mode — LIRC vs evdev
HAS_LIRC=false
if ls /dev/lirc* >/dev/null 2>&1; then
    HAS_LIRC=true
fi

if [ "$HAS_LIRC" = true ]; then
    # Configure IR receiver for RC-6 + NEC (both supported via LIRC)
    RC_DIR="/sys/class/rc/rc0"
    if [ -d "$RC_DIR" ]; then
        echo "Configuring IR receiver for RC-6 and NEC protocols..."
        echo "rc-6 nec" > "$RC_DIR/protocols"
        echo "  Active protocols: $(cat "$RC_DIR/protocols")"

        # Persist via udev rule so it survives reboot
        UDEV_RULE="/etc/udev/rules.d/99-infrafid-ir.rules"
        echo 'ACTION=="add", SUBSYSTEM=="rc", ATTR{protocols}="rc-6 nec"' > "$UDEV_RULE"
        echo "  Created udev rule: $UDEV_RULE"
    else
        echo "Warning: $RC_DIR not found. You may need to configure protocols manually."
    fi
else
    echo "Warning: no /dev/lirc* device found."
    echo "  If your device uses evdev (e.g. Squeezebox Touch), configure it via:"
    echo "  echo 'INFRAFID_ARGS=\"--evdev /dev/input/event1\"' > /etc/default/infrafid"
    echo "  Then: systemctl restart infrafid"
fi

# Build
echo "Building infrafid..."
cd "$SCRIPT_DIR"
make clean
make

# Install
echo "Installing..."
make install

# Enable and start service
echo "Enabling systemd service..."
systemctl daemon-reload
systemctl enable infrafid
systemctl start infrafid

echo ""
echo "=== Installation complete ==="
echo "Status: systemctl status infrafid"
echo "Logs:   journalctl -u infrafid -f"
