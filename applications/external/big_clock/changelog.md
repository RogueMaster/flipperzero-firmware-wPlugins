# Changelog

All notable changes to Big Clock will be documented in this file.

## [1.3] - 2026-01-24

**Fixed**
- **Brightness stability bug** - Fixed brightness reverting to 100% after ~1 hour of use
  - Root cause: Firmware notification system resets brightness after internal timeout
  - Solution: Periodic brightness reapplication every 60 seconds (power efficient)

**Changed**
- **Start at system brightness** - App now respects system backlight setting on startup
  - Previously forced 100% brightness when opened
  - Now reads and rounds to nearest 5% step
- **5% brightness increments** - Finer control with 21 levels (was 10% / 11 levels)
- **Power efficient updates** - Screen updates every 60 seconds (only shows HH:MM)
- **Improved brightness indicator** - Timestamp-based 500ms display after adjustment
- **2-second startup flash** - Shows current brightness level when app opens

**Technical**
- Replaced counter-based indicator with `furi_get_tick()` timestamp approach
- Removed initial `backlight_set_brightness()` call to preserve system setting
- `BRIGHTNESS_DISPLAY_MS` constant for indicator duration

---

## [1.2] - 2026-01-20

**Fixed**
- fixed screen flickering issue; tested on official firmware and momentum custom firmware.


## [1.1] - 2026-01-19

**Added**
- 0% brightness option (screen completely off while clock runs)

**Changed**
- Switched to direct hardware brightness control for smoother operation

**Known Issues**
- Brief flicker may occur on button press/release due to Flipper Zero's backlight system

---

## [1.0] - 2026-01-19

**Added**
- Initial release
- Full-screen digital clock with custom 24x48 pixel digits
- Adjustable brightness (0-100% in 10% increments)
- Visual brightness indicator with progress bar
- Always-on backlight mode
