# Find out what chip you were actually sold

Modules sold as a BME280 frequently carry a BMP280 die. "MPU9250" boards often contain an
MPU6500 with no magnetometer in it. A relabelled part looks identical, works partly, and reveals
itself after you have soldered it in.

Plug the module into the GPIO header, press **Scan I2C bus**, and this app reads the chip's
factory ID register — the number burned into the die, not printed on the package — and names the
part. Then it asks the one question it cannot answer for you: **is this what you bought?**

Five seconds, at the pickup counter, before you pay.

## What it does

- Identifies **80 chips** by their ID registers, each with a plain-language line about what it
  actually does. Address collisions are resolved by probing every candidate.
- Produces a **report you can show a seller**: plain statement first, why a factory ID cannot be
  forged second, register values last. Readable on the Flipper and saved to the SD card.
- **Diagnoses the wiring** before blaming the sensor: a missing pull-up, a line shorted to
  ground, SDA shorted to SCL, or the module plugged into the wrong pins entirely — it sweeps the
  other header pins to find where your pull-ups actually are.
- **Proves the part works, not just that it answers.** An ID register is one byte and a byte can
  be copied; a working sensor cannot be. Thirteen live tests run on nothing but your hand and
  your breath: breathe on an AHT or SHT and watch the humidity climb, cover a BH1750, tip an
  MPU6050 or ADXL345 and watch gravity move axes, wave at an APDS9960, point an MLX90614 at your
  palm, watch a DS3231 tick, make an SSD1306 blink, turn a BNO055 through a figure-8, hold your
  hand in front of a VL6180X.
- **Scans 1-Wire too**, on pin 17: decodes the family code and runs a real temperature
  conversion, so a DS18S20 sold as a DS18B20 is caught.
- **Takes tests written by other people.** A live test can be built as a `.fal` and dropped into
  *apps_data/fake_chip_detector/tests/* with no rebuild of the app. Tests from the card are
  marked **SD**, because a built-in test was reviewed in the repository and one from the card is
  somebody else's code.

## What it refuses to do

Overclaim. A chip with no ID register is reported as **present**, never as genuine. A device
matching nothing is **unidentified**, not "fake" — far more often that is a gap in the database.
A failed read is shown as a failure. Only a partial match, where some of a known chip's IDs are
right and others wrong, is called out as a likely counterfeit.

It reads what the silicon says about itself. It cannot see the silkscreen, the packaging or the
listing, so the comparison with what you *paid for* stays yours to make.

## Wiring

Pin 8 to GND, pin 9 to 3V3, pin 15 to SDA, pin 16 to SCL — **ground first, signals last**. The
built-in wiring guide draws each line broken until that connection goes live, so you can see
which wire is not in yet.

The GPIO pins are 3.3 V and not 5 V tolerant.

## This is a beta

Version 0.7, and the number is honest: one part has been driven end to end on real silicon, and
twelve of the thirteen live tests have never met the chip they were written for. If you run it,
please say what happened — a chip it did not recognise is a useful report, not a nuisance.

Source, a step-by-step guide with screenshots, and the full chip list:
[github.com/hleserg/flipper-fake-chip-detector](https://github.com/hleserg/flipper-fake-chip-detector)
