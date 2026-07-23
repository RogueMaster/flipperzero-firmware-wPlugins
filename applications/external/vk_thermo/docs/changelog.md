# Changelog

All notable changes to VK Thermo Flipper will be documented in this file.

Format based on [Keep a Changelog](https://keepachangelog.com/).

## [1.1] - 2026-02-15

### Added
- TMP119 temperature sensor support (16-bit resolution, 0.0078125°C per LSB)
- Temptress dual-sensor device support (two TMP117 sensors at I2C addresses 0x49 and 0x4A)
- Multi-address sensor detection (probes 0x48, 0x49, 0x4A to identify device type)
- Device ID register reading for sensor identification (TMP117: 0x0117, TMP119: 0x0119)
- Single-shot conversion mode with polling for all sensor types (TMP112/117/119)
- Dual-sensor temperature reading with parallel configuration and synchronized timestamps
- Device type tracking in log entries ("thermo112", "thermo117", "thermo119", "temptress")
- Dual temperature storage in log entries (average + individual readings for Temptress)
- Updated CSV format with 6 columns: timestamp, device_type, celsius, fahrenheit, celsius2, fahrenheit2
- Backwards-compatible CSV loading (supports 3-column, 5-column, and 6-column formats)
- Log view displays both individual sensor readings for Temptress devices
- Graph view displays average temperature for Temptress (individual sensor comparison mode planned for future)

### Changed
- TMP112 now uses single-shot mode instead of continuous conversion
- Temperature readings are fresh conversions each scan (not cached values)
- CSV export includes device type and dual temperature support
- Application description updated to mention Temptress and all TMP sensor variants

## [1.0] - 2025-02-05

### Added
- ISO15693 NFC scanning with auto-restart
- TMP112 temperature reading via NTAG5Link I2C passthrough
- Energy harvesting with configurable timeout (1s, 2s, 5s, 10s, 30s, indefinite)
- Temperature display in Celsius and Fahrenheit
- VivoKey branded scan screen with animated NFC wave arcs
- "Reading..." state with distinct tag-detection feedback (blue LED, chirp, haptic)
- Scan history log with timestamps and min/max stats
- Multi-Thermo support with UID-based filtering
- Graph view with Bezier curve interpolation
- Graph comparison mode (solid/dashed/dotted lines per Thermo)
- UID selector in log and graph views (Left/Right to cycle)
- Per-UID CSV export to SD card (`/ext/apps_data/vk_thermo/<UID>.csv`)
- Legacy CSV migration (auto-converts old `readings.csv` format)
- Settings: temperature unit, energy harvesting timeout, haptic, sound, LED, debug
- Settings persistence to SD card
- Clear history with long-press OK confirmation dialog
- Debug diagnostics toggle (NFC command tests gated behind setting)
- Cooperative NFC cancellation (responsive Back button during energy harvesting)
- Cross-firmware support: Official, Unleashed, Momentum

### Supported Firmware Versions
- Official: 1.4.3
- Unleashed: release branch
- Momentum: release branch
