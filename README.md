# I2C Chip ID

A Flipper Zero app that identifies I²C sensors by their WHO_AM_I / chip-ID registers, so you
can tell a genuine part from a marketplace counterfeit **before** you solder it into a project.

Plug the module into the Flipper's GPIO header, scan the bus, and get a verdict per device:
`GENUINE`, `WRONG CHIP`, `DETECTED (no ID reg)`, `UNKNOWN` or `NO ANSWER` — together with the
raw register bytes that produced it.

## Why

Modules sold as BME280 frequently carry a BMP280 die. "MPU9250" boards often contain an
MPU6500. HMC5883L has been end-of-life since 2016, so a GY-271 labelled HMC5883L is usually a
QMC5883L, a LIS2MDL or something else entirely. All of these are distinguishable in about a
second by reading one register — this app does that and shows its work.

## Features

- **Bus scan** across the valid 7-bit range 0x08-0x77 with live progress.
- **70+ chips** in the database: IMUs, magnetometers, pressure and environmental sensors,
  light and ToF sensors, power monitors, RTCs, EEPROMs, display and touch controllers.
- **Address-collision resolution.** 0x68 alone can be an MPU6050, MPU6500, MPU9250, MPU6886,
  ICM-20948, ICM-42605, ICM-42688-P, BMI160, BMI270, a BMI088 gyro, a DS3231 or a DS1307. Every
  candidate is probed and the one whose ID matches wins.
- **Honest verdicts.** Chips with no ID register (DS3231, SSD1306, AHT20, …) report
  `DETECTED (no ID reg)`, never `GENUINE`. A read that fails is shown as a failure, never as
  `0x00`. Failed reads are retried once so a marginal bus does not fake a counterfeit.
- **Electrical diagnostics.** Before the sweep the app measures both bus lines and tells
  "no pull-ups — sensor unpowered or not wired" apart from "line held low — short or hung chip",
  and names the offending pin.
- **Animated wiring guide** that notices the moment you plug the sensor in.
- **BNO055 live test.** Switches the sensor into NDOF fusion, shows heading with a compass
  needle, reports magnetometer calibration 0-3, and animates the figure-8 motion needed to
  calibrate it. The sensor is parked back in CONFIG mode on exit.
- **Feedback** through melody, RGB LED and vibration — rising arpeggio for genuine, falling
  phrase for fake or dead, two-tone chirp when something needs fixing. Each channel can be
  switched off.
- **Scan logs** written to `/ext/apps_data/i2c_chipid/` as plain text, handy for a dispute
  with a seller.

## Wiring

| Flipper pin | Signal |
|---|---|
| **16** | SCL (PC0) |
| **15** | SDA (PC1) |
| 9 | 3.3 V |
| 8 / 11 / 18 | GND |

> Note the pin order: on the Flipper header **PC0 is pin 16 and PC1 is pin 15**
> (`gpio_pins[]` in `furi_hal_resources.c`), and the external I²C bus uses PC0 for SCL and PC1
> for SDA (`furi_hal_i2c_config.h`). Several popular pinout diagrams have these two swapped.

The GPIO pins are **3.3 V only and not 5 V tolerant.** Most breakout boards with an onboard
regulator are happy on 3.3 V; check yours before connecting.

The external bus runs at 100 kHz, which is what clock-stretching parts such as the BNO055
require. Most breakout modules include pull-up resistors; a bare chip needs 4.7 kΩ pull-ups on
both lines.

## Building

Requires [ufbt](https://github.com/flipperdevices/flipperzero-ufbt).

```bash
cd i2c_chipid
ufbt            # build
ufbt launch     # build, install and run on a connected Flipper
```

Developed against Unleashed firmware (SDK API 87.8, target f7).

## Verdicts

| Verdict | Meaning |
|---|---|
| `GENUINE` | Every ID register of a known chip matched. |
| `WRONG CHIP` | The device answers, but its IDs match no candidate for that address. The raw bytes are shown so you can look them up. |
| `DETECTED (no ID reg)` | A known chip lives at this address but has no ID register — presence is all that can be proven. |
| `UNKNOWN` | Address not in the database. Common WHO_AM_I locations are probed and the raw bytes displayed. |
| `NO ANSWER` | The device acknowledged its address but no register read succeeded. |

## Limitations

Some counterfeits cannot be caught by an ID register, and the app says so rather than guessing:

- SHT30 / SHT31 / SHT35 differ only in accuracy grade and are electrically identical.
- "SSD1306" displays that are really SH1106 or SSD1315 return no identifying byte over I²C.
- Sensors that need a command sequence rather than a register read (Sensirion SHT4x/SCD4x,
  MLX90614) are listed as presence-only.
- ADXL345 clones usually return the correct `0xE5` and only reveal themselves through drift.

A `GENUINE` verdict means the silicon identifies itself correctly. It is strong evidence
against relabelling, not a guarantee of quality or of the part being new.

## License

MIT — see [LICENSE](LICENSE).
