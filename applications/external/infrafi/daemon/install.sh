#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== InfraFi Daemon Installer ==="

# Check for root
if [ "$(id -u)" -ne 0 ]; then
    echo "Error: must be run as root"
    exit 1
fi

# Check for LIRC device
if [ ! -e /dev/lirc0 ]; then
    echo "Warning: /dev/lirc0 not found. The daemon will fail to start without it."
    echo "Ensure your IR receiver is connected and the kernel module is loaded."
fi

# Configure IR receiver for RC-6 only
RC_DIR="/sys/class/rc/rc0"
if [ -d "$RC_DIR" ]; then
    echo "Configuring IR receiver for RC-6 protocol..."
    echo rc-6 > "$RC_DIR/protocols"
    echo "  Active protocols: $(cat "$RC_DIR/protocols")"

    # Persist via udev rule so it survives reboot
    UDEV_RULE="/etc/udev/rules.d/99-infrafid-rc6.rules"
    echo 'ACTION=="add", SUBSYSTEM=="rc", ATTR{protocols}="rc-6"' > "$UDEV_RULE"
    echo "  Created udev rule: $UDEV_RULE"
else
    echo "Warning: $RC_DIR not found. You may need to configure RC-6 manually."
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
