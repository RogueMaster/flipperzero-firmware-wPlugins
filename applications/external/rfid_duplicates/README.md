# Flipper-Zero-RFIDduplicates
Flipper Zero apps that identifies and check RFID duplicates.

**Version:** 1.0  
**Author:** Marcopiiit  
**Platform:** Flipper Zero  
**Firmware:** Momentum

## Overview

RFIDduplicates is a Flipper Zero application designed to read 125 kHz RFID tags and identify duplicate RFID keys saved on the device.

The application scans the `.rfid` files stored in `/ext/lfrfid` and compares their protocol identifiers and key data to find matching entries.

## Features

### Read
- Reads 125 kHz RFID tags using automatic protocol detection.
- Checks the scanned tag against saved RFID keys.
- Displays the name of matching saved key files.
- Shows the detected protocol and tag data.
- Allows you to scan another tag using the **Again** button.

### Check duplicates
- Scans saved `.rfid` files in `/ext/lfrfid`.
- Compares keys using their protocol, data size, and key data.
- Groups duplicate entries and displays their saved filenames.
- Reports the total number of tags checked and duplicate groups found.
- Supports scanning up to 256 saved RFID entries per scan.

### About
Displays the application version and author information.

## Main Menu

- **Read** — Scan an RFID tag and check whether it is saved on the Flipper Zero.
- **Check duplicates** — Search saved RFID files for duplicate keys.
- **About** — Display application information.

## How Duplicate Detection Works

Two saved RFID entries are considered duplicates when all three conditions match:

1. They use the same RFID protocol.
2. Their key data has the same length.
3. Their key data is identical byte by byte.

Files with different filenames can therefore be identified as duplicates if they contain the same protocol and key data.

## Storage Location

The application reads saved RFID files from:

`/ext/lfrfid`

Only files with the `.rfid` extension are considered. Files that cannot be loaded or contain unsupported data are skipped.

The application does not scan every possible RFID tag or search external dictionaries. It checks the RFID files already saved in the specified directory.

## Installation

1. Compile the application using a compatible Flipper Zero firmware development environment.
2. Copy the compiled application to your Flipper Zero.
3. Launch it from the appropriate application menu.
4. Ensure your saved RFID files are located in `/ext/lfrfid`.
5. Select **Read** or **Check duplicates** from the main menu.

## Limitations

- Designed for 125 kHz LF RFID tags, not 13.56 MHz NFC cards.
- Only saved `.rfid` files in `/ext/lfrfid` are checked.
- A maximum of 256 entries can be loaded per scan.
- If more than 256 eligible files are present, the scan stops at the limit.
- Duplicate detection compares protocol and key data, not filenames.
- Reading success depends on tag compatibility and supported RFID protocols.

## Credits

Created by **Marcopiiit**.

---

*RFIDduplicates v1.0 — Flipper Zero / Momentum*
