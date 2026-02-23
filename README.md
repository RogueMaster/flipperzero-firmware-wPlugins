# flipper-air-stats

> **⚠️ ALPHA — work in progress.**
> Only tested with Unleashed firmware. Currently supports two sensors (MH-Z19C and BME280) — both require manual calibration/tuning. Expect bugs and breaking changes.

Flipper Zero app for monitoring CO2 (PWM/UART), temperature, humidity and pressure via GPIO.

## Sensors

| Sensor | Interface | Pin | Measures |
|--------|-----------|-----|----------|
| MH-Z19C | PWM | PA6 (pin 3) | CO2 (ppm) |
| BME280 | I2C | SDA/SCL | Temperature, humidity, pressure |

## Hardware

Tested with "FlipperZero CO2 Sensor" MH-Z19C module:
```
5V  → pin 1
PWM → pin 3 (PA6)
GND → pin 8
```

BME280 on external I2C bus (0x76 or 0x77).

## Build

```bash
# Unleashed firmware (default)
bash tools/build.sh

# Install on device
bash tools/build.sh -i

# Standard firmware
bash tools/build.sh -f standard
```

Requires `.unleashed-firmware` submodule:
```bash
git submodule update --init
```

## Display

- CO2 value + bar graph (0–2000 ppm)
- Temperature (°C), humidity (%), pressure (hPa)
- Back button to exit

## Notes

- CO2 PWM algorithm verbatim from [flipper-zero-mh-z19](https://github.com/meshchaninov/flipper-zero-mh-z19)
- PWM accuracy: ±(40 ppm + 3%) per MH-Z19C datasheet
- BME280 uses Bosch compensation formulas
