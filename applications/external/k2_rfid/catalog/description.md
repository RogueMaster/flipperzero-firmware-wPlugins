# CFS RFID

Creality K2, K1, and CFS RFID Spool Reader, Writer, and Emulator for Flipper Zero.

This app allows you to interact with Creality CFS filament spool RFID tags. You can read and decrypt original Creality tags, configure and program blank tags for third-party filament spools, emulate programmed spools directly to the printer, and save or load spools from the SD card.

## Features

- Scan and Decrypt: Reads official Creality CFS tags, decrypting the encrypted filament data in Sector 1.
- Spool Configuration: Choose your printer model (K2, K1, HI), select from all 66 official Creality filament materials, pick from 20 color presets, configure spool weight, and set serial numbers.
- Write Blank Tags: Programs standard MIFARE Classic 1K tags with proper AES encryption, derived sector trailer keys, and printer metadata.
- Tag Emulation: Emulate any configured or scanned spool directly to the printer reader in real time.
- Save and Load: Store spool profiles on your SD card for future use.
- Tag Reset: Wipe or format previously used tags back to factory default transport keys.

## Two Tags Required per Spool

The Creality CFS filament system has RFID reader antennas positioned on one side of each spool slot. To ensure that your spool is detected regardless of which orientation it is inserted into the CFS, you must program two identical tags and attach one to each side (flange) of the spool.

## How to Use

1. To Scan: Select Scan Spool and hold an original Creality spool tag to the back of your Flipper Zero.
2. To Configure: Select Spool Settings to choose your printer model, filament type, color, and weight.
3. To Write: Select Write Tag and hold a blank MIFARE Classic 1K tag to your Flipper Zero until the confirmation screen appears. Repeat for the second tag for the other side of the spool.
4. To Emulate: Select Emulate Spool and hold the back of your Flipper Zero against the CFS reader bay.

## Credits

Ported from K2-RFID by DnG-Crafts (https://github.com/DnG-Crafts/K2-RFID) by mitchsurp.
