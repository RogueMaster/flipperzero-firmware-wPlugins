# Fake Chip Detector

Tells you whether an I²C sensor really is the chip it was sold as — before you solder it in.

Modules sold as a BME280 frequently carry a BMP280 die. "MPU9250" boards often contain an
MPU6500 with no magnetometer in it. Plug the module into the GPIO header, scan, and the app
reads the chip's factory ID register — the number burned into the die rather than printed on the
package — names the part, and then asks the one question it cannot answer itself: is this what
you bought?

- **80 chips** in the database, 51 identified by an ID register and 29 by address alone. Every
  constant is taken from the manufacturer's datasheet and cited in `chip_db.c`.
- **A report** written for a seller: plain statement first, why a factory ID cannot be forged
  second, register values last. Readable on the device, saved to
  `/ext/apps_data/fake_chip_detector/`.
- **Wiring diagnosis** with live per-line detection — missing pull-up, line shorted to ground,
  SDA shorted to SCL, or the module plugged into the wrong header pins entirely.
- **13 live tests.** An ID register is one byte and a byte can be copied; a working sensor
  cannot. Breathe on an AHT or SHT, cover a BH1750, tip an MPU6050 or ADXL345, wave at an
  APDS9960, point an MLX90614 at your palm, watch a DS3231 tick, blink an SSD1306, turn a BNO055
  through a figure-8, hold a hand in front of a VL6180X.
- **1-Wire scanning** on pin 17 with a real temperature conversion, so a DS18S20 sold as a
  DS18B20 is caught.
- Tests can also be **loaded from the SD card as .fal plugins** — see [LIVE_TESTS.md](LIVE_TESTS.md).

It refuses to overclaim: a chip with no ID register is reported as present, never as genuine; a
device matching nothing is unidentified, not "fake"; a failed read is shown as a failure.

## Wiring

Pin 8 → GND, pin 9 → 3V3, pin 15 → SDA, pin 16 → SCL. **Ground first, signals last.** The GPIO
pins are 3.3 V and not 5 V tolerant.

## Elsewhere

Source, a beginner's guide with screenshots, and releases for all three firmwares:
[github.com/hleserg/flipper-fake-chip-detector](https://github.com/hleserg/flipper-fake-chip-detector)

Full chip list: [SUPPORTED_CHIPS.md](SUPPORTED_CHIPS.md).

MIT licensed.
