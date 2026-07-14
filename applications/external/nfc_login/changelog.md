v1.1.2:
BLE cleanup and OFW compatibility fixes
- **Fix**: Properly restore the original BLE profile on app exit so the Flipper reconnects without toggling Bluetooth
- **Fix**: Adjusted icon include logic to build cleanly on Official Firmware while still supporting Momentum assets

v1.1.1:
Code cleanup and optimizations
- **Code cleanup**: Removed redundant comments, debug logs, and unused code
- **Code optimizations**: Refactored duplicate code into helper functions, improved maintainability

v1.1.0:
Major code refactoring and modularization + BLE HID support
- **NEW: BLE HID Support**: Added Bluetooth Low Energy HID keyboard mode
  - Switch between USB and BLE HID modes in settings
  - BLE device advertises as **Control (Flipper Name)**
  - Works on iOS, Android, and PC devices
  - BLE advertising starts immediately when BLE mode is selected
  - Compatible with both Official and Momentum firmware
- Split monolithic main.c into feature-based modules
- Organized code into crypto/, storage/, scan/, hid/, settings/ folders
- Moved UI rendering into scenes/ folder with scene manager
- Separated enrollment logic into scenes/enroll/
- Moved card editing callbacks to scenes/cards/edit_callbacks.c
- Improved code organization and maintainability
- Fixed crash/freeze issue when spamming back during HID operations
- Added proper HID cleanup on scan thread interruption
- Fixed card list scrolling and selection issues
- Fixed edit menu entry requiring double-click
- Added support for variable-length UIDs (4, 7, or up to 10 bytes)
- Fixed arrow icon display (using I_ButtonRight_4x7)
- Added app icon (10x10px)
- Added comprehensive logging for BLE HID operations

v1.0.0:
Initial release
- NFC card management with encrypted storage
- Automatic password typing via USB HID
- Card selection and persistence
- Keyboard layout support
- Import NFC cards from .nfc files
- Configurable settings (input delay, enter key behavior)

