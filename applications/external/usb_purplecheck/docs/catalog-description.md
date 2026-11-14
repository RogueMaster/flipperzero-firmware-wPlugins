# USB PurpleCheck

Standalone USB control assessment utility by RogerF5. All controls, observations and results stay on the Flipper. No PC companion or scripts are required.

## Features

- Isolated 1 MiB FAT12 storage in read/write or device-protected read-only mode.
- USB keyboard session marker, sent only after an additional manual OK. No shell commands or Enter key.
- Harmless 4 KB SEND.TXT and RETURN.TXT test files. Automatic byte-for-byte return verification and CRC32 on the Flipper.
- Startup banner, persistent settings and English, Spanish or Ukrainian interface. English is the default.
- Configurable 30, 60 or 120 second trial limit, early cancellation, SD JSONL sessions and last-result history.

## Use

Close qFlipper, open the app and press OK. Select a profile and start; an eight-second preparation countdown precedes USB takeover. For storage transfer validation, copy SEND.TXT to the PC and return it as RETURN.TXT, then safely eject before stopping. For keyboard validation, focus a text editor and press OK again on the Flipper. Right opens Settings; Left opens help from the menu.

## Result limits

No response is inconclusive. HID submission and block traffic are observations, not automatic policy verdicts. Read-only rejection is enforced locally by the Flipper. A matching RETURN file proves the synthetic pattern is present on the test volume, not which application copied it or whether it was saved on the PC. The host initiates file operations. Personal files are neither requested nor automatically collected.

Source and full instructions: [USB PurpleCheck](https://github.com/RogerF5-Security/USB-PurpleCheck). GPL-3.0; includes USB transport derived from Flipper Devices' Mass Storage application.
