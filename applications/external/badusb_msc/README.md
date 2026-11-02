# BadUSB MSC v0.1

**BadUSB + USB Mass Storage for Flipper Zero**

BadUSB MSC combines BadUSB script execution with an 8 MiB FAT16 USB storage drive in a single application.

Transfer files between your Flipper Zero and a computer using native Windows commands, without additional PC software.

**Developer:** ZombieDolphin  
**Version:** 0.1  
**License:** GNU GPLv3

## Features

- **BadUSB:** Execute DuckyScript files (`.txt`).
- **USB Mass Storage:** 8 MiB FAT16 virtual drive named `BADUSBMSC`.
- **File Manager:** Browse and manage supported files directly on the Flipper.
- **Keyboard Layouts:** 55 built-in layouts, automatically installed when missing, including Canadian French Legacy.
- **USB/BLE:** Select the BadUSB HID connection mode.
- **Configuration:** Keyboard selection and USB identity settings.
- **Persistent Storage:** Virtual disk and files are preserved between sessions and updates.

## Installation

1. Download or clone this repository.
2. Open a terminal in the project directory.
3. Build using uFBT:

   `ufbt -j 1`

4. Copy the compiled `badusb_msc.fap` to your Flipper Zero using qFlipper.
5. Launch **BadUSB MSC**.

Use an SDK compatible with your Flipper firmware.

On the first launch, the application automatically creates the virtual disk and displays **"Installing disk..."** with a progress bar.

The disk is created only when missing. Existing data is preserved.

## Usage

**BadUSB Scripts**

Place your scripts in:

`SD Card/badusb/`

Select a script from the BadUSB menu to execute it.

**Keyboard Layouts**

Select your keyboard in:

`BadUSB → Select Script → Config → Keyboard Layout`

Layouts are automatically installed in:

`SD Card/badusb/assets/layouts/`

Choose a layout matching the target computer. The active Windows keyboard layout cannot be detected automatically.

**USB Storage**

When connected, Windows detects a FAT16 drive named `BADUSBMSC`.

Virtual disk location:

`SD Card/apps_data/badusb_msc/drive.img`

Example transfer scripts are available in the `examples/` directory:

- `EXAMPLE_TRANSFER.txt` — Windows to Flipper.
- `EXAMPLE_RETURN.txt` — Flipper to Windows.

These examples use PowerShell and require no additional PC software.

**Always safely eject the USB drive before disconnecting.**

## Compatibility & Limitations

- Designed for Flipper Zero.
- Requires compatible firmware and application API.
- USB storage is available through USB only.
- Keyboard layouts must match the host computer.
- Some international keyboard layouts support only ASCII characters.
- The file manager's disk mirror supports root-level 8.3 filenames; long filenames and nested directories are not fully supported.

## ⚠️ Disclaimer & Legal Notice

**BadUSB MSC is intended for educational purposes, security research, authorized testing, and legitimate personal use only.**

**Authorized Use:** Use this software only on devices and systems you own or have explicit permission to access and test. Unauthorized use is strictly discouraged.

**User Responsibility:** Users are solely responsible for the scripts they execute, the data they access or transfer, and compliance with applicable laws and regulations.

**Risk of Data Loss:** USB storage operations may result in file corruption or data loss. Always safely eject the drive and maintain backups of important files.

**No Warranty:** This software is provided **"AS IS"**, without warranties of any kind, express or implied.

**Limitation of Liability:** To the maximum extent permitted by applicable law, the developer **ZombieDolphin**, contributors, and copyright holders shall not be liable for damages, data loss, security incidents, or other consequences resulting from the use or misuse of this software. Nothing in this notice excludes liability that cannot legally be excluded.

**No Endorsement:** The developer does not encourage or endorse illegal activities, unauthorized access, or malicious use.

**USE AT YOUR OWN RISK.**

This disclaimer supplements the warranty and liability provisions of the GNU General Public License v3.0 without restricting the rights granted by that license.

## License & Credits

Licensed under the **GNU General Public License v3.0 (GPLv3)**.

Developed and maintained by **ZombieDolphin**.

This project incorporates and adapts components from the official Flipper Zero firmware, including BadUSB and USB Mass Storage functionality.

Original upstream copyright and license notices are retained. See `LICENSE` and `CREDITS.md` for details.

**BadUSB MSC is an independent community project and is not affiliated with or officially endorsed by Flipper Devices.**