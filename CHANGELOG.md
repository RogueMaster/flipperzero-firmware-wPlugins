# Changelog

## 1.1.0 — 2025

- Move SD card storage from /ext/flipncgm/ to /ext/apps_data/flipncgm/
  per Flipper App Catalog guidelines
- Log file is now at /ext/apps_data/flipncgm/flipncgm.log
- Settings file is now at /ext/apps_data/flipncgm/settings.ff

## 1.0.0 — 2024

- Read Abbott FreeStyle Libre 1/2/3 serial numbers via ISO 15693 NFC
- Display decoded 9-character ASCII serial number and raw UID
- Audio feedback: ascending two-note chime on successful Libre read,
  single low beep for unrecognised ISO 15693 tags
- SD card logging to /ext/flipncgm/flipncgm.log with four levels:
  Off, Error, Info, Debug
- ISO 8601 timestamps in log entries with configurable UTC offset
- UTC offset adjustable in 30-minute steps (−12:00 to +14:00) via
  LEFT/RIGHT buttons; persists in /ext/flipncgm/settings.ff
- Log level cycles with UP button; shown on scanning screen
- Long-press Back exits reliably even when NFC stack is busy
- NFC poller debounce (scan_enabled flag) prevents re-processing the
  same tag in range until the user presses OK to re-arm
- Separate log mutex so file I/O never blocks the GUI draw callback
- 25 ms draw-callback mutex timeout prevents GUI thread from hanging
