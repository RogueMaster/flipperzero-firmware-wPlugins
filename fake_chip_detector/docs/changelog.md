# Changelog

## 1.0

- Initial release.
- Identifies I2C chips by their factory ID registers and reports whether the silicon really is
  the part it claims to be. 80 chips in the database, each with a plain-language description of
  what it does.
- Asks whether the part found is the one the user ordered, since the app cannot see the label,
  and turns a "no" into a report written for a seller or a courier.
- Reports are readable on screen and saved to /ext/apps_data/fake_chip_detector/ as evidence:
  plain statement first, an explanation of why a factory ID cannot be forged, technical detail
  last.
- Supports 8-bit and 16-bit register indices and 16-bit values, covering ST time-of-flight
  parts and TI power monitors alongside the usual WHO_AM_I chips.
- Resolves address collisions by probing every candidate that shares an address.
- Never overclaims: chips without an ID register are reported as present, not genuine; a device
  matching nothing is unidentified rather than fake; a failed read is shown as a failure.
- Wiring guide with live per-line detection, a stray-pull-up sweep for the wrong pins, and
  SDA/SCL short detection.
- Live tests: after a part checks out, the app offers to prove it actually works rather than
  merely identifying itself. Ten parts are covered, and every test can be run standing at a
  pickup counter before paying, with nothing but a hand and a breath: breathe on an AHT10/20 or
  SHT3x/4x, cover a BH1750, tip an MPU6050/6500/9250 or ADXL345, wave at an APDS9960, point an
  MLX90614 at your palm, watch a DS3231 tick, make an SSD1306 blink, rotate a BNO055 through a
  figure-8, or hold a hand in front of a VL6180X. Each test is a self-contained module, so
  adding one for another part touches nothing else.
- Live tests matter most for the chips with no ID register, where the app can otherwise only
  report presence — six of the ten cover exactly those parts, and such a part is now reported as
  IT ANSWERS rather than being told it is the real deal.
- The DS3231 test writes nothing whatsoever, so it cannot disturb a clock already keeping time.
- The OLED test never claims a pass: a display has no readback, so the verdict is the user's.
- Browsable list of every known chip.
- Melody, LED and vibration feedback, each switchable in Settings.
