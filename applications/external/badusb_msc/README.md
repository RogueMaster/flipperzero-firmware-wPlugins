# BadUSB MSC v0.1

**BadUSB scripts + an isolated USB Mass Storage drive for Flipper Zero.**

BadUSB MSC combines a USB HID keyboard and an 8 MiB FAT16 virtual disk in one application. On Windows, the disk appears with the label **`BADUSBMSC`**. No extra PC-side transfer program is required. This is a community project maintained by **ZombieDolphin**, not an official Flipper Devices application.

## Important warning / Disclaimer

**For educational use, research and authorized security testing only.** Run BadUSB scripts **only on computers you own or have explicit permission to test**. Do not use this app to access, modify or transfer data on someone else's system without authorization. You are responsible for complying with applicable laws and policies.

**Risk of data loss:** the virtual disk can be corrupted if disconnected while Windows is writing to it. Safely eject **`BADUSBMSC`** before closing the app or disconnecting USB, and keep backups of important files. Use test files first.

The software is provided **"AS IS"**, without warranty of any kind. To the extent permitted by applicable law, the author and contributors disclaim liability for misuse, data loss or other damage. **Use at your own risk.**

## Features

- **BadUSB** — run DuckyScript `.txt` files stored in `SD Card/badusb/`.
- **Files** — browse and manage supported local files.
- **USB HID + MSC** — run keyboard scripts while a separate 8 MiB FAT16 volume is available to the host.
- **Keyboard layouts** — 55 embedded `.kl` layouts are automatically installed if missing. Existing/custom maps are not overwritten.
- **Config** — choose a keyboard layout, configure USB identity, and switch the HID transport between USB and BLE from the BadUSB script interface. The MSC drive is USB-only.
- **Persistent virtual disk** — `drive.img` is created once when missing, and reused on later launches.

The main menu contains only **BadUSB** and **Files**; the keyboard and USB/BLE options are under **BadUSB → select script → Config**.

## Build and install

This repository contains **source code**, not a pretested universal `.fap`. Build against the SDK corresponding to your Flipper firmware using [uFBT](https://developer.flipper.net/flipperzero/doxygen/app_dev_your_first_app_in_c.html).

1. Clone or download this repository and open a terminal in the folder containing `application.fam`.
2. Run:

   ```powershell
   ufbt -j 1
   ```

3. Once the build succeeds, install `dist/badusb_msc.fap` on your Flipper Zero (for example via qFlipper under `SD Card/apps/USB/`). Firmware/API compatibility must match your device.
4. Launch **BadUSB MSC** from the Flipper's Apps menu.

On the **first launch only**, if the virtual disk is missing (or is an empty zero-byte placeholder), the app displays **"Installing disk..."** and a real 0–100% progress bar while creating an 8 MiB FAT16 `drive.img`. **Do not disconnect the Flipper during disk creation.** Subsequent launches reuse an existing valid disk without showing this installation screen. The app will never overwrite a nonempty invalid disk image automatically; back it up and investigate instead.

The disk lives at `SD Card/apps_data/badusb_msc/drive.img`. It **must not** be uploaded to GitHub or included in releases.

## Keyboard layouts

The app installs missing layouts into `SD Card/badusb/assets/layouts/` when it starts, even when the disk already exists. No manual asset copying is needed; user-created `.kl` files are preserved. Choose the layout that matches the target computer in **BadUSB → Config → Keyboard Layout**. The Flipper **cannot automatically detect** the active Windows keyboard layout.

`fr-CA-Legacy.kl` has been tested on a Windows **Canadian French (Legacy)** layout. Other international variants, including some XKB-derived and Windows Legacy ASCII-only maps, require verification on their respective host keyboards. The 256-byte `.kl` format cannot represent arbitrary Unicode alphabets or complex dead-key sequences; see [layout limitations](docs/WINDOWS_LEGACY_LIMITATIONS.md).

For a quick check, open a text editor on the authorized PC and run [`examples/KEYBOARD_LAYOUT_TEST_LEGACY.txt`](examples/KEYBOARD_LAYOUT_TEST_LEGACY.txt). Confirm that punctuation is typed correctly before running PowerShell transfer scripts.

## Transfer a test file (Windows)

The example scripts use only built-in Windows PowerShell commands and locate the volume by its label, not by a fixed drive letter:

- [`examples/EXAMPLE_TRANSFER.txt`](examples/EXAMPLE_TRANSFER.txt): create `FLIPTEST.TXT` on the Windows Desktop and copy it to `BADUSBMSC`.
- [`examples/EXAMPLE_RETURN.txt`](examples/EXAMPLE_RETURN.txt): copy `BADUSBMSC\FLIPTEST.TXT` back as `FLIPBACK.TXT` on the Desktop.

First make sure the selected keyboard layout matches Windows. After transferring, **eject the `BADUSBMSC` volume safely** before closing the app. The local qFlipper mirror is `SD Card/apps_data/badusb_msc/files/`; only supported root-level **8.3** filenames (for example `FLIPTEST.TXT`) are synchronized. Nested directories, long names and many special characters are not supported by this file mirror.

More details: [transfer guide](docs/SCRIPT_TRANSFER_GUIDE.md) and [test checklist](docs/TEST_CHECKLIST.md).

## Project status and compatibility

Version: **v0.1** (`fap_version="0.1"`). This release is intended for initial community testing. The keyboard layout installer and `fr-CA Legacy` selection were tested on a Flipper, but a **full build and complete regression test of this GitHub packaging change have not been performed here**. Check the [test checklist](docs/TEST_CHECKLIST.md) before distributing a binary. The USB/BLE and custom USB identity features may vary with firmware and host operating system.

## License and credits

Licensed under **GNU GPL v3**: [LICENSE](LICENSE). Maintained by **ZombieDolphin**. This project adapts upstream Flipper Devices BadUSB and Mass Storage components; upstream copyright and attribution are retained in [CREDITS.md](CREDITS.md). Third-party keyboard layout data has separate upstream notices in [`docs/XKB_DATA_LICENSE.txt`](docs/XKB_DATA_LICENSE.txt).

Flipper Zero, Flipper Devices, qFlipper and Unleashed are the names of their respective owners. This project is **not affiliated with or endorsed by** Flipper Devices or any custom firmware team.
