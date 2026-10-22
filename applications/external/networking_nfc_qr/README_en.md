[![en](https://img.shields.io/badge/lang-en-red.svg)](README_en.md)
[![es](https://img.shields.io/badge/lang-es-blue.svg)](README.md)

# Networking NFC + QR for Flipper Zero

This application for Flipper Zero allows emulating an NFC card and displaying a QR code simultaneously on the screen. It is ideal for networking situations, allowing you to share your contact or information in two different ways at the same time.

## Features

*   **Quick Dual Creation (QR + NFC)**: Generates both a `.qrcode` file and an `.nfc` file (NTAG215 with NDEF URI) at once by entering the name and URL directly on the Flipper.
*   **NFC Emulation**: Loads and emulates standard `.nfc` files.
*   **QR Visualization**: Generates and displays QR codes from `.qrcode` files.
*   **Dual Interface**: Shows the QR on the screen while the NFC chip is active being emulated.
*   **Optimization**: Automatically adjusts the QR size to maximize visibility on the Flipper screen.
*   **Credits**: Screen with information about the author.

## Requirements

*   **Flipper Zero** (obviously).
*   **Firmware**: You can compile using **uFBT** (recommended and fastest) or using the official firmware source code or Custom Firmwares (CFW) like Unleashed, RogueMaster or Xtreme.

## Compilation and Installation

### Option 1: Using uFBT (Recommended)
If you have `ufbt` installed:
```bash
ufbt
```
The compiled `.fap` will be generated in the `dist/` directory.

### Option 2: Using local firmware with script
1.  Ensure you have downloaded the firmware repository in `../flipperzero-firmware`.
2.  Run the compilation script:

```bash
./compile.sh
```

The script will compile the application and save the resulting `.fap` in `build/networking_nfc_qr_app.fap`.

Copy the `.fap` file to your Flipper Zero (using qFlipper or SD card) in the `/ext/apps/NFC/` folder.

## Usage

1.  Open the **Networking NFC + QR** application on your Flipper Zero.
2.  **Create QR + NFC**: Enter the desired file name and URL. The app will automatically generate the `.qrcode` file in `/ext/apps_data/qrcodes/` and the `.nfc` file in `/ext/nfc/`, selecting them ready for immediate emulation.
3.  **Select QR Code**: Manually select an existing `.qrcode` file if you prefer another.
4.  **Select NFC File**: Manually select an existing `.nfc` file if you prefer another.
5.  **Start Emulation**: The magic begins. The Flipper will start emulating the NFC card and will show the QR code on the screen.
6.  Press **Back** to stop emulation and return to the menu.

## Credits

This project has been made possible thanks to:

*   **DonJulve**: Main development and adaptation.
*   **[Flipper Zero Firmware](https://github.com/flipperdevices/flipperzero-firmware)**: For the incredible platform and documentation.
*   **[bmatcuk/flipperzero-qrcode](https://github.com/bmatcuk/flipperzero-qrcode)**: For their excellent library and codebase for QR code generation on the Flipper Zero.
