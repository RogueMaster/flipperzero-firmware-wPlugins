# Changelog

## v2.0

### New
- **Area measurement** — walk a perimeter, get area in m². Adaptive 30-vertex polygon with Visvalingam simplification. Settings → Start area calc.
- **LED status on main screen** — red (no signal), blue×2 (no fix), green (fix acquired)
- **Persistent settings** — baud rate, timezone, speed units saved to SD card and restored on launch
- **POI target preserved** — selected POI is remembered across mode switches and restarts

### Changed
- NMEA dump tool removed (use v1.0 for diagnostics)
- Settings redesigned: cursor navigation ↑↓, value change ←→, OK=save, Back=cancel
- Breadcrumb counter fixed (was showing 0/0)
- POI save from main screen: OK saves immediately, ←→ activates name editor
- GPS coordinates from last known fix used when current signal lost (saves POI even indoors)

### Fixed
- Baudrate not applied correctly on startup
- POI index shifted after sort on NAV entry
- LED not working (wrong notification API usage)
- Name input cursor misaligned with proportional font
- Duplicate function definitions causing build failures

---

## v1.0

Initial release.

- Breadcrumb trail recording and navigation
- POI save and navigation  
- Direction arrow with compass / relative bearing modes
- NMEA dump tool for GPS module diagnostics
- LED navigation guidance
- Settings: speed units, baud rate, deep sleep, timezone
