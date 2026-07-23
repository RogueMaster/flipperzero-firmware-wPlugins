# VK Thermo Flipper

Read temperature from your VivoKey Thermo with Flipper Zero!

Brought to you by [VivoKey Technologies](https://vivokey.com/).

## Features

- Instant temperature reading in Celsius and Fahrenheit
- Continuous scanning mode - auto-restarts after each read
- Multi-Thermo support - tracks different devices by UID
- Scan history with timestamps and min/max stats
- Graph view with Bezier curves and comparison mode
- Per-Thermo CSV export to SD card
- Sound, LED, and haptic feedback on reads
- Configurable energy harvesting timeout

## Usage

1. Launch app - scanning starts automatically
2. Hold Flipper's NFC antenna near your VivoKey Thermo
3. Temperature displays with sound/LED confirmation
4. Scanning resumes automatically

### Navigation

| Screen | Left      | OK           | Right     | Back |
| ------ | --------- | ------------ | --------- | ---- |
| Scan   | Graph     | Log          | Settings  | Exit |
| Log    | Cycle UID | (long) Clear | Cycle UID | Scan |
| Graph  | Cycle UID | Compare      | Cycle UID | Back |

## Data Export

Each Thermo's readings are saved to a separate CSV file on the SD card:

**Location**: `/ext/apps_data/vk_thermo/<UID>.csv`

Format: `timestamp,device_type,celsius,fahrenheit,celsius2,fahrenheit2`

Example (`E0040150A1B2C3D4.csv`):
```
timestamp,device_type,celsius,fahrenheit,celsius2,fahrenheit2
1706918400,thermo112,36.52,97.74,0.00,0.00
1706918520,temptress,36.48,97.66,36.51,97.72
```

**Note**: Single-sensor devices (TMP112/117/119) have `0.00` in the second temperature columns. Temptress dual-sensor devices show both sensor readings.

## Supported Devices

- **VivoKey Thermo** (NTAG5Link + TMP112/117/119 sensor)
  - TMP112: 12-bit resolution, 0.0625°C per LSB
  - TMP117: 16-bit resolution, 0.0078125°C per LSB
  - TMP119: 16-bit resolution, 0.0078125°C per LSB
- **Temptress** (NTAG5Link + dual TMP117 sensors)
  - Two sensors at I2C addresses 0x49 and 0x4A
  - Average temperature displayed, both readings stored

## Installation

### From Pre-built FAP

Copy `vk_thermo.fap` to `/ext/apps/NFC/` on your Flipper Zero.

### Building from Source

```bash
# Build for official firmware
./build.sh official

# Build for other firmwares
./build.sh momentum
./build.sh unleashed

# Build for specific firmware version
./build.sh official --tag 1.4.3
```

## Supported Firmwares

- Official (primary target)
- Unleashed
- Momentum

## Technical Details

The devices use:
- **NTAG5Link** chip for NFC communication (ISO15693)
- **TMP112/117/119** temperature sensor(s) connected via I2C
- NXP custom commands for I2C passthrough over NFC
- Energy harvesting from NFC field to power the sensor(s)
- Single-shot conversion mode with polling for fresh readings

## License

This project is open-source and may be used freely.

## Credits

- [VivoKey Technologies](https://vivokey.com/)
- Based on [Flipper Zero FAP Boilerplate](https://github.com/leedave/flipper-zero-fap-boilerplate) by leedave
- NFC protocol reference from [VivoKey ntag5sensor](https://github.com/VivoKey/ntag5sensor)
- ISO15693 implementation pattern from [Flipper Wedge](https://github.com/DangerousThings/flipper-wedge)
