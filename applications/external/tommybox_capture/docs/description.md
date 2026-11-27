# TommyBox Capture

A native Flipper Zero NFC application for rapidly scanning, unlocking, verifying, and saving **TommyBox (NXP ICODE SLIX-L)** NFC figures without ever saving a truncated or corrupted capture file.

## Why This App Exists

TommyBox NFC figures use NXP ICODE SLIX-L (ISO15693-3) tags locked in Privacy Mode with the TommyBox privacy password. Because figures have thick plastic bases and weaker RF coupling, scanning them with the stock NFC workflow often leads to silent partial block reads (zero-filled trailing blocks), default placeholder privacy passwords on timeout, or repetitive menu navigation across large collections.

**TommyBox Capture** runs a continuous, hands-free SLIX-L unlock loop that strictly verifies all 8 data blocks in memory and reads back every saved file from the SD card to confirm integrity before displaying a clear **SAVED! 8 / 8** banner.

## Features

- **Continuous Hands-Free Batch Scanning**: Hold a figure against the back of the Flipper Zero until you get the green LED flash, double beep, and **SAVED! 8 / 8** confirmation, then immediately move to the next figure.
- **Automatic SLIX-L Privacy Unlock**: Automatically supplies the TommyBox privacy password during unlock requests.
- **Strict 8/8 Block Verification**: Verifies the NXP ICODE SLIX-L UID prefix, confirms all 8 blocks (32 bytes) of data content are complete and non-zero, and reads back the saved file from /ext/nfc on the SD card to confirm on-disk integrity.
- **Partial Read Recovery**: Displays live block progress if a figure is moved away too early, vibrates to alert you, and immediately resets the poller so you can hold steady and finish.
- **UID Deduplication and Safe Incomplete File Handling**: Checks existing files in /ext/nfc by UID so valid captures are never duplicated. By default, existing incomplete files are left completely untouched (Keep). You can optionally configure Settings to rename older incomplete captures with an _incomplete suffix (Rename) or remove them after a verified replacement is saved (Delete).
- **Repeat Scan Verification**: Re-scanning the same figure performs a live RF check and increments the on-screen scan counter without creating duplicate files.
- **One-Button On-Device Rename**: Press Right (Rename) right after a capture to rename the file on the spot, or keep the automatic UID-based name and keep scanning.
- **Built-In SD Card Auditor**: Inspects all SLIX-L files in /ext/nfc and reports valid versus incomplete captures along with the specific failure reason for any broken files.

## Usage

- Open **Scan TommyBox** from the main menu and hold each figure flat against the back of the Flipper Zero until **SAVED! 8 / 8** appears.
- Press **Right (Rename)** after a capture if you want to rename the saved file immediately, or hold the next figure to keep batch scanning.
- Open **Check SD Card** from the main menu to audit existing SLIX-L files in /ext/nfc and identify any incomplete captures that should be re-scanned.
- Open **Settings** to choose how older incomplete files with a matching UID are handled: **Keep** (default, does not touch existing files), **Rename** (appends _incomplete), or **Delete** (removes the broken copy only after the new verified capture is saved).

## Source Code

Source code and full documentation are available on [GitHub](https://github.com/mitchsurp/Tonies-Capture-Flipper).
