v1.3:
- Added six English built-in diagnostic script presets
- Bundled presets are copied to /ext/apps_data/zero_buster/payloads on startup

v1.2:
- All app files moved to /ext/apps_data/zero_buster (payloads, loot, custom_hid.txt)
- Keyboard layouts are now loaded from the stock BadUSB .kl files in
  /ext/badusb/assets/layouts, so every firmware layout is supported
- Default layout is en-US (built-in en-US map is used if no .kl files are found)
- Received host output is saved as loot/log_YYYYMMDD_HHMMSS.txt (timestamped)
- UI text fully in English

v1.1:
- Initial release
- Bidirectional USB keyboard input and serial communication
- Dual keyboard layout support (ES and US)
- USB device profiles (Logitech K120, Dell, Apple, Flipper, Custom)
- On-device Custom HID configuration editor
- Script file selection
- Transfer status screen and About/Credits view
