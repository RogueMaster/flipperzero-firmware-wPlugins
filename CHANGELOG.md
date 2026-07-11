# Changelog

All notable changes to Trident are documented here.

## [1.0] — 2026-07-11

Initial release.

### ESP32 (Wi-Fi / Bluetooth / GPS)
- Full ESP32 Marauder controller over the GPIO UART (115200 8N1).
- Wi-Fi: scan APs/stations, channel analyzer, set channel, targeting/select,
  sniffers (beacon/probe/deauth/PMKID/pwnagotchi/ESP/raw).
- Attacks gated behind a confirmation: deauth, beacon spam (list/random/AP),
  probe flood, Rickroll.
- Bluetooth: sniff, skimmer detect, AirTag scan, BLE Spam (Apple/Samsung/Google/Windows/all).
- GPS: live data and AP/station wardrive.
- Device: help, board settings, clear lists, SD update, reboot.
- Live serial console for any raw Marauder command.
- Selectable UART pins (13/14 or 15/16).

### NRF24 (2.4 GHz)
- Read-only nRF24L01+ spectrum analyzer over the external SPI bus.
- 126-channel sweep (2400–2525 MHz) using the Received Power Detector, with a
  decaying activity accumulator and peak marker.

### CC1101 (Sub-GHz)
- RSSI sweep analyzer over 300–348 / 387–464 / 779–928 MHz.
- Internal or external CC1101, selectable in Settings (external uses the
  firmware's `cc1101_ext` device when available, with fallback to internal).

### App
- Unified home screen, shared spectrum view with peak-hold, settings and about.
- Branded assets: app icon, banner, social preview, UI mockups.
- CI builds the FAP on the release and dev SDK channels.
