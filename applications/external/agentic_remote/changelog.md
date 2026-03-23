## v0.28
- Rename from Claupper to Agentic Remote (trademark-safe)
- BLE build: OK → USB remote, Right → BT remote
- Lazy USB HID init on BLE build (qFlipper screenshots work)
- Settings reordered: OS, Bluetooth, Haptics, LED, Tour, DblClk
- Manual and settings show 4 items instead of 3
- Selection boxes no longer overlap scrollbar
- Fix triple-tap detection (3rd tap was being swallowed)
- Fix double-click OK switching windows twice
- New AR icon, updated splash tagline
- Submitted USB build to official Flipper App Catalog

## v0.27
- Fix combo state (held-key flags) leaking across mode transitions
- Macros screen now portrait with 9 visible items instead of 4
- Hold Up/Down in macros list for continuous scrolling
- Macros opened from Remote returns to Remote on Back
- Hint bar shows both combos (Hotkeys and Macros)
- Slower marquee scroll for readability

## v0.26
- Dim backlight in remote mode to save battery
- BLE default transport
- Triple-tap Left for Ctrl+N (end of line)
- Restored voice dictation on Down
- BLE release_all on disconnect

## v0.25
- Left+Down opens Macros from remote mode
- Multi-page hotkey overlay (Right+Down)
- BLE crash fix on disconnect
- Macro improvements and reorder

## v0.24
- Tour screen on first launch
- Claude Code skill (/claupper)
- npx claupper installer

## v0.23
- Settings: Haptics, LED, OS (Mac/Win/Linux)
- Double-click speed setting
- Macros from SD card (up to 24)
- Hold Left for backspace repeat
- Long-press Back for Escape

## v0.2
- Quiz mode with Easy/Medium/Hard difficulty
- Mac-style answer modal

## v0.1
- Initial release
- Remote control with double-click actions
- Offline Claude Code manual
- Voice dictation
