# Z5Flip

Edit the key base of an **IronLogic Z-5R** access controller directly from Flipper Zero, without the Z-1/Z-2 adapter or a PC.
The app reads and writes the controller's I2C EEPROM (24C64 / 24C32) through the service header.

Intended for owners and installers servicing their own controllers. Physical access to the controller board is required,
exactly as with the vendor's own programming tools.

## Wiring

Z-5R jumper header is 2x4: top row pins 1-4, bottom row 5-8. Do **not** power the controller with 12 V while connected.

- Header 2 (EEPROM SDA) - Flipper pin 15 (C1)
- Header 5 (EEPROM-only power) - Flipper pin 1 (5V)
- Header 6 (EEPROM SCL) - Flipper pin 16 (C0)
- Header 7 or 8 (GND) - Flipper pin 18 (GND)
- Do **not** connect header 1: it powers the controller MCU, which then holds SCL low.

The app turns on 5V (OTG) before every controller operation and uses internal pull-ups. The wiring picture is also in the app under Info / connection.

## Main menu

- **Base** - open the loaded key base.
- **Read: controller / file** - read the whole controller memory (saved automatically as controller_last.bin) or a 4/8 KB .bin image from /ext/apps_data/z5flip/.
- **Erase controller** - clear the key list (offers a backup first). A blank chip is formatted for its size.
- **Info / connection** - wiring diagram, version, capacity and help.

## Key base screen

- **Add key** - choose the type (Normal, Master, Blocking, Inactive), then read iButton (DS1990) or RFID (EM4100, H10301) keys one after another, load .ibtn / .rfid files, or enter 12 hex digits.
- **Set open time** - door open time in seconds, written together with the base.
- **Edit keys list** - filter, mark, change type, delete, or export keys to /ext/ibutton/ or /ext/lfrfid/ for emulation with the stock apps.
- **Save to: controller / file / both** - every page is verified after writing. A backup of the controller is offered before overwriting it.

The .bin files are the same raw image as "Controller to file" in IronLogic BaseZ5R, so the base can be edited on a PC and written back.
All messages are also logged to /ext/apps_data/z5flip/log.txt.

## Limits

- Z-5R with 4 KB (680 keys) and 8 KB (1363 keys) memory. Z-5R model 5000 and Matrix (16/32 KB, Z7 format) are not supported.
- Read and write block the screen for a few seconds.
- The seconds-to-code conversion of the open time is not yet calibrated on hardware.
- Skipping the backup after a read does not check that the same controller is still connected: read or back up a different controller before writing to it.

## Source

Built with uFBT. Host tests for the format and controller logic are in the tests folder (gcc on Linux/WSL).
