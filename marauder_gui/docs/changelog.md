# Changelog

## v1.1

- Added PCAP-to-SD capture, with a live per-AP dashboard (packet counts by frame type) while
  capturing instead of a raw byte feed.
- Added a saved-.pcap viewer: filter by frame type and SSID, view a packet's parsed header and a
  Wireshark-style hex+ASCII dump, rename or delete a saved capture.
- Added PMKID options, GPS, Recon and Tools menus.
- Split Recon into separate WiFi and BLE screens, each a live discovery list (found APs/devices)
  under their own WiFi/Bluetooth menus; moved GPS to the main menu.
- Added 5V OTG auto power on launch and a ported on-device keyboard; fixed several UI overflow
  issues.
- Fixed two crash bugs found on real hardware: a corrupted/truncated old .pcap file could hang
  and trigger a watchdog reset, and a large fixed-size allocation could exceed free heap and
  crash the device outright; both are now bounded against the file/heap instead of guessed
  constants.
- Fixed a keyboard bug where typing "_" as the first character of any text field produced a space.
- Slowed down the marquee scroll speed on long list rows app-wide.

## v1.0

- Initial public release.
- Added GPIO UART control for compatible ESP32 Marauder boards.
- Added Wi-Fi, Bluetooth, detector, network, device, and terminal menus.
- Added persistent Turkish and English language selection.
- Added animated status screens.
