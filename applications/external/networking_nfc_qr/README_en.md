[![en](https://img.shields.io/badge/lang-en-red.svg)](README_en.md)
[![es](https://img.shields.io/badge/lang-es-blue.svg)](README.md)

# Networking NFC + QR for Flipper Zero

This application for Flipper Zero allows emulating an NFC card and displaying a QR code simultaneously on the screen. It is ideal for networking situations, allowing you to share your contact or information in two different ways at the same time.
**This branch is intended to be used with the RogueMaster firmware.**

## Features

*   **NFC Emulation**: Loads and emulates standard `.nfc` files.
*   **QR Visualization**: Generates and displays QR codes from `.qrcode` files.
*   **Dual Interface**: Shows the QR on the screen while the NFC chip is active being emulated.
*   **Optimization**: Automatically adjusts the QR size to maximize visibility on the Flipper screen.
*   **Credits**: Screen with information about the author.

## Requirements

*   **Flipper Zero** (obviously).
*   **Firmware**: You need the firmware source code to compile the application.
    *   You can use the [Official Firmware](https://github.com/flipperdevices/flipperzero-firmware).
    *   It is also compatible with **Custom Firmwares (CFW)** like Unleashed, RogueMaster or Xtreme.

## Compilation and Installation

I have included an automated script to facilitate compilation.

1.  Clone this repository.
2.  Ensure you have downloaded the firmware repository (official or CFW) in a nearby folder (by default it looks in `../flipperzero-firmware`).
3.  Run the compilation script:

```bash
./compile.sh
```

4.  The script will ask for your firmware path. If it is in the default path, simply press `Enter`.
5.  The script will compile the application and save the resulting `.fap` file in the `build/` folder within the project.
6.  Copy the `build/networking_nfc_qr_app.fap` file to your Flipper Zero (using qFlipper or the SD card) in the `/ext/apps/NFC/` folder (or wherever you prefer).

## Usage

1.  Open the **Networking NFC + QR** application on your Flipper Zero.
2.  **Select QR Code**: Select the file with the QR content (must be a plain text file or `.qrcode` with the message).
3.  **Select NFC File**: Select the `.nfc` file you want to emulate.
4.  **Start Emulation**: The magic begins. The Flipper will start emulating the NFC card and will show the QR code on the screen.
5.  Press **Back** to stop emulation and return to the menu.

## Credits

This project has been made possible thanks to:

*   **DonJulve**: Main development and adaptation.
*   **[Flipper Zero Firmware](https://github.com/flipperdevices/flipperzero-firmware)**: For the incredible platform and documentation.
*   **[bmatcuk/flipperzero-qrcode](https://github.com/bmatcuk/flipperzero-qrcode)**: For their excellent library and codebase for QR code generation on the Flipper Zero.