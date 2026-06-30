# Changelog

All notable changes to the Flipper Wedge project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1] - 2025-02-04

### Added
- **Keyboard Layout Support** ([#2](https://github.com/DangerousThings/flipper-wedge/issues/2))
  - Built-in layouts: Default (QWERTY) and NumPad
  - Custom layout files loaded from SD card (`/ext/apps_data/flipper_wedge/layouts/`)
  - Support for international keyboards: AZERTY, QWERTZ, Dvorak, Colemak, and more
  - Companion repository with pre-made layouts: [flipper-wedge-keyboard-layouts](https://github.com/dangerous-tac0s/flipper-wedge-keyboard-layouts)

### Changed
- Settings file version bumped to 6 (layout settings added)
- HID typing functions now use configurable keyboard layout

---

## [1.0] - 2024-11-24

### Added
- **Multi-mode NFC/RFID scanning**
  - NFC Only mode (ISO14443A/B, MIFARE, NTAG)
  - RFID Only mode (EM4100, HID Prox, Indala)
  - NDEF Mode (dedicated NDEF text record parsing)
  - NFC + RFID combo mode (scan both, output combined UIDs)
  - RFID + NFC combo mode (reverse order)
  - Mode persistence - remembers last selected scan mode

- **NDEF Support**
  - Type 2 NDEF text records (MIFARE Ultralight, NTAG series)
  - Type 4 NDEF support with APDU commands (ISO14443-4A)
  - Type 5 NDEF support (ISO15693 tags)
  - Automatic NDEF parsing and text extraction
  - NDEF error type distinction for unsupported NFC Forum Types
  - Retry logic for robust NDEF reading

- **HID Output**
  - USB HID keyboard emulation
  - Bluetooth HID keyboard emulation
  - Dual output support (USB + BT simultaneously)
  - Connection status detection and display
  - Automatic typing with configurable delimiter

- **User Interface**
  - Main scanning screen with real-time status
  - Mode selection menu
  - Advanced Settings screen
    - Custom delimiter selection (space, colon, dash, none)
    - Enter key append toggle
    - Bluetooth enable/disable toggle
    - USB debug mode toggle
  - Bluetooth pairing screen
  - Visual feedback (LED: green/red for success/error)
  - Haptic feedback on scans
  - Optional audio feedback

- **Reliability Features**
  - Automatic error recovery for NFC poller failures
  - Timeout handling for combo modes (5 seconds)
  - Tag removal detection
  - Cooldown period to prevent accidental re-scans
  - Settings persistence across app restarts

### Technical Details
- Built on official Flipper Zero firmware (0.105.0+)
- Compatible with Unleashed, Xtreme, and RogueMaster firmwares
- Modular architecture with separate helpers for NFC, RFID, HID, and formatting
- Comprehensive error handling and user feedback
- Clean separation of concerns (scenes, views, helpers)

### Project History
- **November 2024**: Initial development
  - Project started from flipper-zero-fap-boilerplate template
  - Renamed from "Contactless HID Reader" to "Flipper Wedge"
  - Implemented core NFC/RFID reading functionality
  - Added USB and Bluetooth HID output
  - Developed 5-mode scanning system
  - Implemented NDEF parsing for Types 2, 4, and 5
  - Added comprehensive settings and configuration
  - Polished UI/UX with proper feedback mechanisms

---

## Future Considerations

### Potential Enhancements
- Additional NDEF record types (URI, Smart Poster, etc.)
- Custom output formatting templates
- Multiple tag memory (batch scanning)
- Export scan history to file
- Password-protected NDEF tags support
- Raw UID output modes (decimal, binary)

### Known Limitations
- NFC and RFID cannot poll simultaneously (hardware limitation)
- Some exotic RFID protocols not yet supported
- NDEF writing not implemented (read-only)
- Complex NDEF structures may not parse correctly

---

## Version History Summary

| Version | Date       | Key Features |
|---------|------------|--------------|
| 1.1     | 2025-02-04 | Keyboard layout support for international keyboards |
| 1.0     | 2024-11-24 | Initial release with full NFC/RFID/NDEF support |

---

For detailed commit history, see the [Git repository](../../commits/).
