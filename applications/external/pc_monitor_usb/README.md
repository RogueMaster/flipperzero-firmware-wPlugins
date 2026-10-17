# PC Monitor & Soundboard (USB Version)
Flipper Zero application for real-time PC resource monitoring (CPU, RAM, GPU, VRAM, Temperatures) and USB Soundboard pad.

**[A backend running on your PC is required.](https://github.com/DonJulve/Flipper-Zero-PC-Monitor-USB-Backend)**

This is a ported and enhanced version of the original [PC Monitor](https://github.com/TheSainEyereg/flipper-pc-monitor) application to work over USB (Virtual COM Port) with bidirectional control.

## Features
- **Hardware Telemetry**: Displays CPU %, RAM usage, GPU %, VRAM usage, and real-time **CPU & GPU temperatures** (°C).
- **USB Soundboard**: Turns your Flipper Zero into an audio soundboard. Short button presses trigger customized `.mp3` sounds on your PC.
- **Clean USB Stack**: Automatically releases USB CDC back to the Flipper CLI on exit, allowing qFlipper to connect immediately without device reboots.

## Controls
- **Up / Down / Left / Right / OK**: Trigger corresponding soundboard sounds on your PC (`up.mp3`, `down.mp3`, `left.mp3`, `right.mp3`, `ok.mp3`).
- **Back (Short Press)**: Triggers `back.mp3` on your PC.
- **Back (Long Press)**: Exit application safely.

## How to use
1. Install the `.fap` application on your Flipper Zero (via Flipper Lab or `ufbt launch`).
2. Launch the application on your Flipper.
3. Connect your Flipper Zero to your PC via USB.
4. Run the accompanying [Rust backend](https://github.com/DonJulve/Flipper-Zero-PC-Monitor-USB-Backend) on your PC.

### Soundboard Setup on PC
Place your `.mp3` files in a `sounds/` folder in the same directory as the backend executable:

```text
pc-folder/
├── flipper-pc-monitor-backend-usb (or .exe on Windows)
└── sounds/
    ├── up.mp3
    ├── down.mp3
    ├── left.mp3
    ├── right.mp3
    ├── ok.mp3
    └── back.mp3
```

## Credits & Acknowledgments
- **Original Author**: [TheSainEyereg](https://github.com/TheSainEyereg)
- **USB Port, Soundboard & Enhancements**: [DonJulve](https://github.com/DonJulve)
