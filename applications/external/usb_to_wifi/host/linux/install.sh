#!/bin/sh

set -eu

package_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
data_root=${XDG_DATA_HOME:-"${HOME:?HOME is required}/.local/share"}
application_root="$data_root/flipper-usb-internet-bridge"
desktop_root="$data_root/applications"
icon_root="$data_root/icons/hicolor/256x256/apps"
desktop_file="$desktop_root/flipper-usb-internet-bridge.desktop"

mkdir -p "$application_root" "$desktop_root" "$icon_root"
install -m 0755 "$package_dir/fib-bridge" "$application_root/fib-bridge"
install -m 0644 "$package_dir/fib-bridge.png" \
    "$icon_root/flipper-usb-internet-bridge.png"

{
    printf '%s\n' '[Desktop Entry]'
    printf '%s\n' 'Type=Application'
    printf '%s\n' 'Version=1.0'
    printf '%s\n' 'Name=Flipper USB Internet Bridge'
    printf '%s\n' 'Comment=Provide authorized HTTPS access to Flipper Zero apps'
    printf 'TryExec=%s\n' "$application_root/fib-bridge"
    printf 'Exec="%s"\n' "$application_root/fib-bridge"
    printf '%s\n' 'Icon=flipper-usb-internet-bridge'
    printf '%s\n' 'Terminal=true'
    printf '%s\n' 'Categories=Network;Utility;'
} >"$desktop_file"
chmod 0644 "$desktop_file"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$desktop_root" >/dev/null 2>&1 || true
fi

printf 'Installed Flipper USB Internet Bridge for %s.\n' "${USER:-the current user}"
printf 'Open it from the application menu or run: %s\n' "$application_root/fib-bridge"
