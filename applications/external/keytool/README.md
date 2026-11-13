# Flipper-Zero-KeyRecoveryTool
Flipper Zero apps that helps recover and backup MIFARE keys from existing dump to user dictionary.

**KeyTool** is a Flipper Zero application for Momentum firmware that scans MIFARE NFC dump files and checks whether their keys are present in the NFC dictionaries detected on the device (main and user) and in the cache section you can import in the user dictionary the keys missing in both.

Created by **Marcopiiit**.

Quick guide 

1 use Start to scan a dump and the app will show which keys are found or not found in which database and will save a copy of the scan in the LOG section.
If some keys are not found in any of the database, a copy of those keys will be done in CACHE section.

2 To check the NOT FOUND keys use the CACHE section. In this section you can Import to user database the keys found in the scan that are not present in any database. The app will not Import the keys if duplicates are present.

SMART LOGIC:

IF SOME KEYS ARE NOT FOUND IN ANY OF THE DB THE APP WILL WARN ON TOP OF THE LOG SO YOU DO NOT HAVE TO READ THE ENTIRE LOG.

## Features

- **Start** — Select and scan an `.nfc` dump file.
- **Dictionary Detection** — Automatically detects supported NFC dictionary files.
- **Key Verification** — Checks each extracted key against every detected dictionary.
- **Detailed Results** — Shows which dictionaries contain each key and identifies keys that are missing from all scanned dictionaries.
- **Log** — Automatically saves scan results with the date and time for later review.
- **Cache** — Keeps track of keys that were not found in any detected dictionary, grouped by dump.
- **About** — Displays the application version and author.

## Main Menu

| Option | Description |
|---|---|
| Start | Select a dump and run a dictionary scan. |
| Log | Browse previously saved scan reports. |
| Cache | Review missing keys grouped by dump and add it in the user dictionary (the app will check for duplicate). |
| About | Display application information. |

## How to Use

1. Launch KeyTool on your Flipper Zero.
2. Select **Start** from the main menu.
3. Choose an `.nfc` dump file from the configured dump directory.
4. Wait for the application to parse the dump and scan the detected dictionaries.
5. Review the results displayed on the screen.
6. Open **Log** to review saved scan reports later.
7. Open **Cache** to review keys that were not found in any scanned dictionary and add it in the user dictionary (app will check for duplicates also).

## Scan Results

Each scan report includes the dump path, the number of extracted keys, and the number of detected dictionaries.

Keys are organized into two sections:

### NOT FOUND

Lists keys that were not found in any of the detected dictionaries.

### FOUND

Lists keys that were found in at least one dictionary, including the names of the dictionaries containing each key.

The final summary reports the number of keys found compared with the total number of extracted keys.

**Important:** A key reported as `NOT FOUND` is missing from the dictionaries scanned by KeyTool. This does not necessarily mean the key is invalid or unavailable elsewhere.

## Supported Dump Files

KeyTool uses the `.nfc` file extension in its file browser and relies on its MIFARE dump parser to extract keys.

The actual supported dump format and key extraction rules depend on the implementation of the accompanying `keytool_mifare` module.

## Dictionary Detection

KeyTool automatically searches for dictionaries in the following locations:

- `nfc/assets/mf_classic_dict`
  - `.nfc` files
  - `_user.nfc` files
- `nfc/dict/`
  - `.dict` files
  - `.txt` files
  - `.nfc` files
- `/ext/dict/`
  - `.dict` files
  - `.txt` files
  - `.nfc` files

The exact detection behavior depends on the accompanying `keytool_dict` module.

If no dictionaries are detected, KeyTool displays an error message listing the expected locations.

## Logs

Each scan is automatically saved with the dump filename and the current date and time.

The Log menu allows you to browse previous scan reports, open their details, and delete saved entries.

## Cache

The Cache menu groups keys that were not found in any detected dictionary by their source dump.

According to the application interface, holding the **OK** button on a cache entry imports the associated missing keys. The exact import destination and behavior depend on the implementation of the accompanying cache module.

## Requirements

- Flipper Zero
- Momentum firmware
- Compatible `.nfc` dump files
- Supported NFC dictionary files

## Version

**Version:** 1.0  
**Author:** Marcopiiit
