# Changelog

All notable changes to Xbox POST Code Reader are documented here.

## 0.14 - 2026-07-23

### Added

- Three-line repair information on the Known Code detail screen.
- Header-level `OK Open` hint on the Known Codes browser.
- Complete README instructions for installation, wiring, live capture, offline
  decoding, SD logs, and WebXboxPOSTTool pairing.

### Changed

- Reworked Known Codes spacing to use the full display for three evenly spaced
  records.
- Moved the selected-row highlight up two pixels and kept it one pixel taller
  for improved text alignment.
- Updated the companion website download to the v0.14 FAP.

### Fixed

- Removed the bottom Open button that competed with the third Known Codes row.
- Updated Known Code scrolling limits for the new three-line detail layout.

## 0.13 - 2026-07-23

### Added

- Auto-numbered SD-card capture logs and an on-device saved-log viewer.
- Web Serial identity, version, configuration, and live POST streaming.
- Xbox-style application icon.

### Fixed

- Prevented an empty capture save from locking the app.
- Prevented log-list scrolling from blocking or locking navigation.
- Separated POST capture and input queues so controls remain responsive during
  heavy traffic.
- Corrected Reader button spacing, Decode return navigation, and main-menu
  title overflow.

## 0.12 - 2026-07-23

### Added

- Rolling 128-entry capture history with single-code and live-log screens.
- Offline decode and a 452-record console-aware database.
- Known Codes categories for All, POST, Error, SMC, SP, CPU, and OS.
- Wiring, Web Decoder QR, and About pages.
- Xbox One, One S, One X, Series S, and Series X console selection.

## 0.1 - 2026-07-22

- Initial Flipper Zero port of `coolshrimp/Xbox-POST-tool`.
- MAX6958-compatible I2C capture at address `0x38`.
