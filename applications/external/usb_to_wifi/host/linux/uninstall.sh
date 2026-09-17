#!/bin/sh

set -eu

data_root=${XDG_DATA_HOME:-"${HOME:?HOME is required}/.local/share"}

rm -f "$data_root/applications/flipper-usb-internet-bridge.desktop"
rm -f "$data_root/icons/hicolor/256x256/apps/flipper-usb-internet-bridge.png"
rm -f "$data_root/flipper-usb-internet-bridge/fib-bridge"
rmdir "$data_root/flipper-usb-internet-bridge" 2>/dev/null || true

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$data_root/applications" >/dev/null 2>&1 || true
fi

printf '%s\n' 'Flipper USB Internet Bridge was removed.'
