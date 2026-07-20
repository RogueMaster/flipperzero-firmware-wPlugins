# FM Radio

Turn your Flipper Zero into an FM radio. Controls TEA5767 and Si4703 tuner boards over I2C, with automatic chip detection at startup.

## Features

- Supports **TEA5767** and **Si4703** FM tuner boards (auto-detected, or force a chip in Settings)
- Station presets loaded from editable .txt files on the SD card, organized by region
- Seek up/down with adjustable seek strength (1-15)
- Fine tuning in 0.1 MHz steps
- Audio mode: Stereo, Mono (Left), or Mono (Right)
- Mute on exit toggle (radio standby when leaving the app)
- In-app station file viewer
- Settings saved automatically to SD card

## Wiring

Both chips connect to the I2C bus:

- VCC to 3V3 (Pin 9)
- GND to GND (Pin 18)
- SCL to C0 (Pin 16)
- SDA to C1 (Pin 15)

The Si4703 additionally **requires**:

- RST to A4 (Pin 4)
- SEN tied to 3.3V (already tied HIGH on most breakout boards)

## Station Lists

Station files live at /ext/apps_data/fm_radio/ and are auto-created on first run. Each line is frequency|Station Name, for example 101.1|Jazz Radio. Edit them or add your own regions - up to 100 stations per file.

## Controls

- Up / Down: preset up / down
- Left / Right: seek down / up
- OK: toggle mute
