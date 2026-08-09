# Changelog

## 1.2

- Fix the worker leaving the radio in a degraded state after exit (port of the firmware Frequency Analyzer fix): the active radio (internal or external CC1101) kept the analyzer's near-field AGC/bandwidth registers through sleep; the radio is now reset and the RF path re-parked, leaving the chip in the same state as after boot

## 1.1

- Add support for the 2FSK 12 kHz deviation preset

## 1.0

- Initial release: the firmware's Frequency Analyzer as a standalone app with external CC1101 module support
