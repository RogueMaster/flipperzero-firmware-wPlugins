# Changelog

## Version 0.5 - SH1106 Release Candidate

- Ported the original SSD1306 application to SH1106.
- Implemented SH1106 display initialisation.
- Implemented page-based framebuffer addressing.
- Added two-column display RAM offset.
- Implemented software horizontal mirroring.
- Replaced hardware scrolling with software scrolling.
- Added adjustable software scrolling speed.
- Replaced unsupported fade/blink commands with software effects.
- Preserved the original diagnostic features.
- Added independent application identity and data directory.
- Removed obsolete SSD1306 compatibility functions.
- Validated operation on physical SH1106 hardware.
- Verified display disconnection and reconnection recovery.

## Original Application

Based on SSD1306 Test by Martin Bogomolni.

Original source:
https://github.com/martinbogo/ssd1306_test
