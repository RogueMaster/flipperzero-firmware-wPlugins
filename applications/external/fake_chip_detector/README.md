# Fake Chip Detector

A Flipper Zero app that tells you whether an I²C sensor really is the chip it was sold as —
before you solder it in, or while the courier is still at the door.

Plug the module into the GPIO header, scan, and the app names the part, says what it does in
plain words, and asks the one question it cannot answer itself: is this what you bought?

![A VL6180X live test running](fake_chip_detector/screenshots/19_live_test.gif)

An ID register can be copied. A working sensor cannot — so the app asks the part to do its job
and shows you the answer moving. Above: a VL6180X following a hand, captured off the device.

| | |
|---|---|
| ![Menu](fake_chip_detector/screenshots/01_menu.png) | ![Wiring](fake_chip_detector/screenshots/02_wiring.png) |
| The app | Wiring guide: each line is drawn broken and closes up when that connection goes live |
| ![Question](fake_chip_detector/screenshots/04_question.png) | ![All good](fake_chip_detector/screenshots/05_allgood.png) |
| What was found, and the question only you can answer | The answer you want to see |
| ![Report](fake_chip_detector/screenshots/06_report.png) | ![Live tests](fake_chip_detector/screenshots/14_live_tests.png) |
| The report, readable on screen — show it to the seller | Every test the app can run, including any found on the SD card |
| ![Stray pull-up](fake_chip_detector/screenshots/10_wiring_stray.png) | ![Wrong hole](fake_chip_detector/screenshots/11_wrong_hole.png) |
| SDA is off pin 15 — so the app goes looking, and finds the module's pull-up sitting on pin 6 | And it is nice about it |
| ![Live test](fake_chip_detector/screenshots/12_live_vl6180x.png) | ![Wrong chip](fake_chip_detector/screenshots/13_wrong_chip.png) |
| A live test: the rangefinder actually ranging, before you pay for it | 0x29 answers — it is just not the chip this test is for |

## Install

Take the build for **your** firmware from
[the latest release](https://github.com/hleserg/flipper-fake-chip-detector/releases/latest) and
copy it to `apps/GPIO/` on the SD card, or drag it onto qFlipper.

| Firmware | File | Built against |
|---|---|---|
| Unleashed | `fake_chip_detector-unleashed.fap` | `unlshd-090`, API 88.2 |
| Official | `fake_chip_detector-official.fap` | 1.4.3, API 87.1 |
| Momentum | `fake_chip_detector-momentum.fap` | `mntm-012`, API 87.1 |

A FAP is tied to the firmware API it was built against. Take the wrong one and the loader
refuses it — "App Too Old", or an API mismatch — which is annoying but harmless. Take the right
one and it just runs.

> **Only the Unleashed build has been run on hardware.** The other two compile cleanly against
> their SDKs and that is all anybody knows about them. If you run one, please say what happened —
> [@skhlebnikov](https://t.me/skhlebnikov) or an
> [issue](https://github.com/hleserg/flipper-fake-chip-detector/issues). "It works" is a useful
> report; so is a photo of it failing.

**This is 0.7, and the number is honest.** One sensor has been driven end to end on real
silicon. Twelve of the thirteen live tests have never met the chip they were written for. It
goes to 1.0 when other people's hardware has had a say.

**New to this?** **[GUIDE.md](GUIDE.md)** walks through the whole thing with screenshots at every
step — which wire goes in which hole, what the words on the screen mean, and what to do when
nothing is found. No electronics knowledge assumed.

## Why

Modules sold as BME280 frequently carry a BMP280 die. "MPU9250" boards often contain an
MPU6500. HMC5883L has been end-of-life since 2016, so a GY-271 labelled HMC5883L is usually a
QMC5883L or something else. Every one of these is distinguishable in about a second by reading
one register — this app does that and shows its work.

## What it does

- **Names the part and what it is.** Not just `VL6180X` but `VL6180X — Laser rangefinder`, for
  all 80 chips in the database. No searching a part number to find out you were sent a distance
  sensor instead of the IMU you paid for.
  → **[Full list of supported chips](fake_chip_detector/SUPPORTED_CHIPS.md)**, with the register
  and expected value used for each one.
- **Asks whether that is what you ordered.** Knowing which chip it is only answers half the
  question; the app cannot see the label, so it asks.
- **Produces a report you can read out at the front door.** Plain language first — what the chip
  is, and why a factory ID cannot be forged by a seller — with the register values last, under a
  separator. Viewable on screen and saved to the SD card as evidence.
- **Diagnoses the wiring.** Before blaming the sensor it measures both bus lines, tells a missing
  pull-up apart from a line shorted to ground, notices when the module is on the wrong pins, and
  detects SDA shorted to SCL.
- **Treats an empty scan as a question, not a verdict.** Many sensors have a pin that decides
  whether they speak I2C at all, and set the other way they have no address to answer on: healthy,
  powered and invisible. So the screen says `No I2C answer` and offers to find out. Move the pin-15
  wire onto the pad in question and the app meters it live — HIGH, LOW or FLOATING — and says what
  that level means for that kind of pin. Where it is the cause, it holds the pad the way I2C needs
  with an internal pull too weak to fight anything on the board, reads the pad back to see whether
  the hold took, walks you through restarting the sensor so a pin that is only sampled at reset
  gets sampled again, and rescans by itself once the bus is back. Every step ends on a measurement:
  a pad the board ties down in copper is reported as exactly that, and a pin that has to be held
  the whole time is called a fifth-wire job rather than offered a fix that would do nothing. The
  report can be saved from any of those screens — the person who most needs a document is the one
  whose scan came back empty.
- **Refuses to overclaim.** A chip with no ID register is reported as present, never as genuine.
  A device whose IDs match nothing is `UNIDENTIFIED`, not "fake" — that is far more often a gap
  in the database. Only a partial match, where some of a known chip's IDs are right and others
  wrong, is called out as a likely counterfeit.
- **Proves the part works, not just that it answers.** An ID register is one byte, and one byte
  is what a relabeller can copy. When a chip that checked out has a live test, the app offers to
  run it — and every test is one you can do standing at a pickup counter before you pay, with
  nothing but your hand and your breath. Thirteen parts are covered: breathe on an AHT20 or
  SHT31 and watch the humidity climb; cover a BH1750 and watch it hit the dark floor its
  datasheet specifies; tip an MPU6050, MPU6500, MPU9250 or ADXL345 and watch gravity move to
  another axis; wave at an APDS9960; point an MLX90614 at your palm; watch a DS3231 tick; make
  an SSD1306 blink; turn a BNO055 through a figure-8 until it calibrates; hold a hand in front
  of a VL6180X and watch the distance follow it. This matters most for the parts with
  **no ID register at all** — for a DS3231 or an AHT20 the app can otherwise only say "something
  is there", so a live test is the only evidence that will ever exist. **Live tests** in the
  menu lists every test and runs any of them on demand, without scanning first.
- **Your own tests, without rebuilding the app.** A test is one file, and it can be built as a
  `.fal` and dropped into `apps_data/fake_chip_detector/tests/` on the SD card — the app finds
  it, lists it and runs it. The same source compiles either into the app or out of it: a test
  never calls a function of the app by name, it is handed the bus as a table of pointers.
  [`test_plugin_template/`](test_plugin_template) is a complete working example to copy, and
  **[LIVE_TESTS.md](fake_chip_detector/LIVE_TESTS.md)** covers all of it: how to put a test from
  somebody else onto the card, how to write your own, and the rules a test is held to. Tests
  loaded from the card are marked `SD` on screen, because a built-in test was written against a
  datasheet and reviewed here and one from the card is somebody else's code.
  **If you write one that works on a real part, send it here** — in the app it ships to
  everybody and gets offered automatically after a scan, instead of living on one SD card.
- **1-Wire too.** Scans pin 17, decodes the family code and runs a real temperature conversion
  on DS18B20-class parts. A 1-Wire ID can be replayed by any microcontroller, so the app is
  explicit that this proves which *part* answered — a DS18S20 sold as a DS18B20 is caught —
  and never that it is authentic.

## Wiring

| Flipper pin | Signal |
|---|---|
| 8 | GND |
| 9 | 3.3 V |
| 15 | SDA (PC1) |
| 16 | SCL (PC0) |

Connect in that order: **ground first, power next, signals last.** Until ground is in, return
current from a signal has to flow through the chip's protection diodes, which is how modules get
damaged. Unplug in reverse.

> Note the pin order: on the Flipper header **PC0 is pin 16 and PC1 is pin 15**
> (`gpio_pins[]` in `furi_hal_resources.c`), and the external I²C bus uses PC0 for SCL
> (`furi_hal_i2c_config.h`). Several popular pinout diagrams have these two swapped.

The GPIO pins are **3.3 V only and not 5 V tolerant.** The bus runs at 100 kHz, which is what
clock-stretching parts such as the BNO055 need. Most breakout boards include pull-up resistors;
a bare chip needs 4.7 kΩ on both lines.

## Building

Requires [ufbt](https://github.com/flipperdevices/flipperzero-ufbt). The SDK must match the
firmware on the device, or the loader refuses the app with "App Too Old".

```bash
cd fake_chip_detector
ufbt update     # if the firmware was updated
ufbt            # build
ufbt launch     # build, install and run
```

Developed against Unleashed `unlshd-090`, SDK API 88.2, target f7. The other two release builds
come from the same source with a different SDK deployed — `ufbt` keeps its state in one place,
so give each firmware its own:

```bash
UFBT_HOME=~/.ufbt_official  ufbt update --channel release
UFBT_HOME=~/.ufbt_official  ufbt

UFBT_HOME=~/.ufbt_momentum  ufbt update --index-url https://up.momentum-fw.dev/firmware/directory.json --channel release
UFBT_HOME=~/.ufbt_momentum  ufbt
```

Nothing in the source is conditional on the firmware; all three are the same code.

## Verdicts

| Verdict | Meaning |
|---|---|
| `GENUINE` | Every ID register of a known chip matched. The silicon really is that part — now compare it with what the board and the listing claim. |
| `LIKELY FAKE` | Some of a known chip's IDs match and others do not. A genuine part has all of them. |
| `UNIDENTIFIED` | It answers but nothing matched. Usually a chip missing from the database; the raw bytes are shown so you can look them up. |
| `DETECTED (no ID reg)` | A known chip lives at this address but has no ID register — presence is all that can be proven. |
| `NO ANSWER` | The device acknowledged its address but no register read succeeded. |

**What the app can and cannot know.** It reads what the silicon says about itself. It cannot see
the silkscreen, the packaging or the seller's listing, so it never claims a chip matches its
label — that comparison is yours. Its job is to tell you what the part actually is.

## Limitations

Some counterfeits cannot be caught by an ID register, and the app says so rather than guessing:

- SHT30 / SHT31 / SHT35 differ only in accuracy grade and are electrically identical.
- "SSD1306" displays that are really SH1106 or SSD1315 return no identifying byte over I²C.
- Sensors needing a command sequence rather than a register read (Sensirion SHT4x/SCD4x,
  MLX90614) are presence-only.
- ADXL345 clones usually return the correct `0xE5` and only reveal themselves through drift.

A `GENUINE` verdict is strong evidence against relabelling. It is not a guarantee of quality or
of the part being new.

## Testing it yourself

[TESTING.md](TESTING.md) walks from checks that need no hardware at all, through a single jumper
wire, to a real counterfeit hunt.

Cutting a release — the tag, the three builds and the four places that carry a copy of the
app — is [RELEASING.md](RELEASING.md).

[BACKLOG.md](BACKLOG.md) is what is left to do, and — the part worth reading first — which
merged changes have never been run on real hardware.

## Contact

Questions, a chip that should be in the database, a module the app got wrong — write to
[@skhlebnikov](https://t.me/skhlebnikov) on Telegram, or open an issue.

## License

MIT — see [LICENSE](LICENSE).
