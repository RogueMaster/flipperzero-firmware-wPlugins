## [0.1.0] - 2026-07-28

### Added
- Vertical transmit UI, settings screen, multi-shocker sync groups, shocker naming (#2)

### Fixed
- Restore periodic redraw so the receive screen updates live
- Only set s->transmitting after pulses are successfully generated
- Harden storage/settings input validation

### Chores
- Fix CI lint failures, scope workflow token permissions, and add release automation

## 0.1

Initial release:
- Transmit commands to supported shockers over 433 MHz
- Receive and decode shocker transmissions
- Save and load shocker configurations to SD card
- Support for CaiXianlin, Petrainer, Petrainer 998DR, T330, and D80 shockers
