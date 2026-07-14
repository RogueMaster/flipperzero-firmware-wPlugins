# Changelog

All notable changes to the BT Audio project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2] - 2025-01-25

### Fixed
 - Issue of needing to reboot (sometimes 4+ times) for ESP32 to be detected when using momentum
 
### Added
- Momentum Firmware support
- Flipper Zero OFW support

## [1.1] - 2025-01-13

### Fixed
- Fixed favorites not displaying when playing from file browser - asterisk (*) now appears correctly for favorited tracks
- Fixed hold-OK favorite toggle not updating UI when playing from file browser
- Fixed M3U playlist files not appearing in ESP32 SD card browser
- Updated max favorite tracks from 20 to 40. When favorites list is full and user tries to add a new track, there's a different vibration feedback with a red led.
- Added Roguemaster Firmware compatability.

### Changed
- Updated app version to v1.1

### Added
- M3U playlist files now visible in ESP32 SD browser alongside MP3 files
- Verification testing guide (VERIFICATION_v1.1.md)

## [1.0] - Initial Release

### Added
- Bluetooth A2DP audio streaming to headphones and speakers
- Device scanning and connection management
- MP3 playback from ESP32 SD card
- MP3 playback from Flipper SD card (streaming over UART)
- M3U playlist support (playlist1.m3u, playlist2.m3u)
- File browser for selecting music from SD cards
- Now Playing controls (play/pause, volume, next/prev track)
- Favorites system - mark tracks with hold-OK, asterisk indicator
- Test tone playback (440Hz, jingle)
- Configurable settings:
  - SD source selection (Flipper SD or ESP32 SD)
  - Bluetooth TX power adjustment
  - Initial volume setting
  - EQ controls (bass, mid, treble)
  - Backlight always-on mode
  - Background audio mode
  - Shuffle mode
  - Continuous play mode
  - Custom Bluetooth device name
- Automatic 5V GPIO power for ESP32
- Device history and quick reconnect
- QR code display for documentation
- ESP32 firmware detection and diagnostics
