# Switch Controller - Flipper Zero App

A Flipper Zero application that emulates a Nintendo Switch Pro Controller over USB with macro recording and playback capabilities.

**✅ Compatible with Official Firmware and Momentum Firmware**

## Features

- **Manual Controller Mode**: Use your Flipper Zero as a Nintendo Switch controller
  - Three control modes: D-Pad, Left Stick, Right Stick
  - Toggle modes with long press Back button
  - OK button → A button
  - Back button → B button
  - Long press OK → Menu for additional buttons (X, Y, L, R, ZL, ZR, +, -, Home)
  - Directional buttons control D-Pad or analog sticks based on mode

- **Macro Recording**: Record button sequences with precise timing
  - Records D-Pad movements and analog stick inputs
  - Records button presses and releases
  - Timestamps for accurate playback
  - Switch control modes during recording

- **Macro Playback**: Play back recorded macros
  - Loop or one-time playback
  - Accurate timing reproduction
  - Can be stopped at any time

- **Macro Management**: Save and load macros from SD card
  - Persistent storage
  - Named macros for easy identification
  - Browse and select from saved macros

## Installation

**📖 For detailed installation instructions, see [INSTALLATION.md](INSTALLATION.md)**

The installation guide covers:
- Multiple installation methods (uFBT, Mobile App, qFlipper)
- Prerequisites and requirements
- First-time setup
- Troubleshooting common issues

### Quick Start (uFBT)

1. Install uFBT:
   ```bash
   python3 -m pip install --upgrade ufbt
   ```

2. Clone and build:
   ```bash
   git clone https://github.com/ccyyturralde/Flipper-Zero-Joycon.git
   cd Flipper-Zero-Joycon
   ufbt
   ```

3. Install to Flipper Zero:
   ```bash
   ufbt launch
   ```

## Usage

### Manual Controller Mode

1. Connect your Flipper Zero to the Nintendo Switch via USB
2. Launch the Switch Controller app
3. Select "Manual Controller"
4. Control the Switch using:
   - **Directional buttons**: Up/Down/Left/Right (controls D-Pad or sticks)
   - **A Button**: OK button
   - **B Button**: Back button (short press)
   - **Mode Toggle**: Back button (long press) - cycles between D-Pad, Left Stick, Right Stick
   - **Button Menu**: OK button (long press) - access X, Y, L, R, ZL, ZR, +, -, Home

**Control Modes:**
- **D-Pad Mode**: Directional buttons control the D-Pad
- **Left Stick Mode**: Directional buttons control the left analog stick
- **Right Stick Mode**: Directional buttons control the right analog stick

### Recording a Macro

1. Launch the Switch Controller app
2. Select "Record Macro"
3. Enter a name for your macro
4. **Before recording**: Use long press Back to set control mode (D-Pad/Left Stick/Right Stick)
5. Press OK to start recording
6. Perform your button sequence:
   - Use directional buttons for D-Pad or stick movements
   - Press OK for A button
   - You can switch control modes during recording with long press Back
7. Press Back (short press) to stop and save the recording

### Playing a Macro

1. Launch the Switch Controller app
2. Select "Play Macro"
3. Choose a saved macro from the list
4. The macro will play automatically (loops by default)
5. Press Back to stop playback

## Technical Details

### USB HID Implementation

The app implements a custom USB HID device that mimics the Nintendo Switch Pro Controller:
- **Vendor ID**: 0x057E (Nintendo Co., Ltd)
- **Product ID**: 0x2009 (Pro Controller)
- **HID Report**: 12-byte report format
  - 2 bytes: Button state (14 buttons)
  - 1 byte: D-Pad (HAT switch)
  - 8 bytes: Analog sticks (4 axes, 16-bit each)

### Macro File Format

Macros are stored in `/ext/apps_data/switch_controller/` with `.scm` extension:
- Magic number: `0x4D414353` ("MACR")
- Macro name (32 bytes)
- Event count (4 bytes)
- Total duration (4 bytes)
- Event array (variable length)

### Limitations

- Flipper Zero has only 6 physical buttons, so not all Switch buttons are directly accessible (use button menu)
- Analog stick movements are digital (full direction or centered, no partial movements)
- Some advanced controller features (rumble, gyro, NFC) are not implemented

## Button Mapping

| Flipper Button | Switch Function |
|---------------|---------------|
| Up/Down/Left/Right | D-Pad or Analog Stick (depends on mode) |
| OK (short press) | A Button |
| OK (long press) | Button Menu → X, Y, L, R, ZL, ZR, +, -, Home |
| Back (short press) | B Button |
| Back (long press) | Toggle Control Mode (D-Pad/L-Stick/R-Stick) |

### Control Modes

The app has three modes that change what the directional buttons control:

1. **D-Pad Mode** (default): Directional buttons → D-Pad movements
2. **Left Stick Mode**: Directional buttons → Left analog stick (full deflection)
3. **Right Stick Mode**: Directional buttons → Right analog stick (full deflection)

Switch between modes anytime with long press Back button.

## Credits

- Based on research from:
  - [dekuNukem's Nintendo Switch Reverse Engineering](https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering)
  - [Flipper Zero Firmware](https://github.com/flipperdevices/flipperzero-firmware)
  - [Arduino JoyCon Library](https://github.com/squirelo/Arduino-JoyCon-Library-for-Nintendo-Switch)

## License

This project is provided as-is for educational purposes.

## Disclaimer

This software is for educational and research purposes only. Use responsibly and in accordance with Nintendo's terms of service and local laws.
