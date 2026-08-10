# Changelog

## 1.0

- Initial release.
- I2C bus scan over the valid 7-bit address range (0x08-0x77) with a live progress display.
- Chip identification against a database of 70+ parts using WHO_AM_I / chip-ID registers,
  including 16-bit ID registers, masked revision fields and address ranges.
- Verdicts: GENUINE, WRONG CHIP, DETECTED (no ID reg), UNKNOWN, NO ANSWER. Chips without an
  ID register are never reported as genuine.
- Address-collision resolution: every candidate sharing an address is probed and the one whose
  ID matches wins.
- Electrical bus diagnostics that tell a missing pull-up apart from a line shorted low, with
  wiring hints derived from the measurement.
- Animated wiring screen that detects the sensor the moment it is plugged in.
- BNO055 live test: NDOF mode, heading readout, compass needle, magnetometer calibration level
  and an animated figure-8 calibration prompt.
- Feedback via melody, RGB LED and vibration, each independently switchable.
- Settings: sound, vibration, LED, backlight, probe speed and automatic log saving.
- Scan logs saved to /ext/apps_data/i2c_chipid/ as plain text.
