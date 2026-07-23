# VK Thermo Flipper

Read temperature from your VivoKey Thermo with Flipper Zero!

Brought to you by [VivoKey Technologies](https://vivokey.com/).

## Features

- Instant temperature reading in Celsius, Fahrenheit, and Kelvin
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

**Scan screen:** Left=Graph, OK=Log, Right=Settings, Back=Exit

**Log screen:** Left/Right=Cycle UIDs, Long OK=Clear history, Back=Scan

**Graph screen:** Left/Right=Cycle UIDs, OK=Compare mode, Back=Scan

## Data Export

Each Thermo's readings are saved to a separate CSV file on the SD card at **/ext/apps_data/vk_thermo/**

## Supported Devices

- VivoKey Thermo (NTAG5Link + TMP112/117)

## Supported Firmwares

- Official (primary target)
- Unleashed
- Momentum

## Credits

- [VivoKey Technologies](https://vivokey.com/)
- NFC protocol reference from [VivoKey ntag5sensor](https://github.com/VivoKey/ntag5sensor)
- ISO15693 implementation pattern from [Flipper Wedge](https://github.com/DangerousThings/flipper-wedge)
