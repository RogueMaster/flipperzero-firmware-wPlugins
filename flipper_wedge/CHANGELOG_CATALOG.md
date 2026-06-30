# Changelog

All notable changes to the Flipper Wedge project will be documented in this file.

## v1.1 - 2025-02-04

**Keyboard Layout Support**
- Built-in layouts: Default (QWERTY) and NumPad
- Custom layout files loaded from SD card
- Support for international keyboards: AZERTY, QWERTZ, Dvorak, Colemak, and more

**Bug Fixes**
- Added 5-second timeout for combo scan modes (NFC+RFID, RFID+NFC)
- Fixed resource leak in scan logging module
- Updated settings storage path from legacy naming
- Removed dead BLE key migration code

## v1.0 - 2024-11-24

**Multi-mode NFC/RFID scanning**
- NFC Only mode (ISO14443A/B, MIFARE, NTAG)
- RFID Only mode (EM4100, HID Prox, Indala)
- NDEF Mode (dedicated NDEF text record parsing)
- NFC + RFID combo mode (scan both, output combined UIDs)
- RFID + NFC combo mode (reverse order)
- Mode persistence - remembers last selected scan mode

**NDEF Support**
- Type 2 NDEF text records (MIFARE Ultralight, NTAG series)
- Type 4 NDEF support with APDU commands (ISO14443-4A)
- Type 5 NDEF support (ISO15693 tags)
- Automatic NDEF parsing and text extraction

**HID Output**
- USB HID keyboard emulation
- Bluetooth HID keyboard emulation
- Dual output support (USB + BT simultaneously)
- Connection status detection and display
- Automatic typing with configurable delimiter

**Configuration**
- Custom delimiter selection (space, colon, dash, none)
- Enter key append toggle
- Output mode switching (USB/BLE) without app restart
- NDEF max length limits (250/500/1000 chars)
- Vibration level control
- Optional scan logging to SD card
- Mode startup behavior (remember last or default)
