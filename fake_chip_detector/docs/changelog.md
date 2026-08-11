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
- A Live tests screen listing every test the app can run, built in or found on the SD card, and
  running any of them on demand without scanning first. Launching one probes the addresses that
  test declares and refuses to start if nothing answers, rather than writing configuration
  registers to whatever else is on the bus.
- Tests can be written by anyone and dropped onto the card as .fal plugins in
  apps_data/fake_chip_detector/tests/ — no rebuild of the app. The same source file compiles
  either into the app or out of it, because a test is handed the bus as a table of pointers
  instead of calling the app by name. A complete template to copy ships in the repository.
- Tests loaded from the card are marked SD in the list and on the test screen: a built-in test
  was written against a datasheet and reviewed in the repository, and one from the card is
  somebody else's code. A plugin built against an older version of the contract is refused with
  a reason rather than run.
- A live test tells "the sensor fell off" apart from "that is not the part". If the ID register
  cannot be read at all the screen says the sensor dropped off and to check the wires; if it
  reads back the wrong value it says WRONG CHIP instead, because the wiring is fine and the
  advice would otherwise send somebody to reseat a jumper that was never loose. It matters at
  the crowded addresses: 0x68 carries a DS3231 and ten IMUs, 0x28/0x29 a BNO055 and a VL6180X.
- Browsable list of every known chip.
- Melody, LED and vibration feedback, each switchable in Settings.
