# Changelog

All notable changes to GhostTag are documented here.
This project adheres to [Semantic Versioning](https://semver.org/).

## [1.0.0] — 2026-06-24

### Added
- Initial release. 🎉
- **Flipper FAP**
  - Animated radar scan view (sweep, range rings, signal-mapped blips, pulsing followers).
  - Detections list with vendor icon, signal bars and follower flag.
  - Per-device detail (MAC, RSSI, time-tracked, sighting count, status).
  - Full-screen strobing **TRACKER ALERT** with configurable sound / vibration / LED.
  - Settings: detection range, dwell time before alert, alert channels.
  - "Following you" heuristic based on persistence + repeated sightings.
- **ESP32 companion firmware** (NimBLE)
  - Continuous BLE scan with vendor classification (Apple Find My, Tile, Samsung SmartTag).
  - Plain-ASCII UART line protocol (`GT1` detections, `GTHELLO`, `START`/`STOP`).
- Pluggable radio source (`helpers/uart_link.c`) ready for a future native BLE backend.
