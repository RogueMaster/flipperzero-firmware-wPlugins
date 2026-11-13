# Flipper-Zero-TagDuplicate
Flipper Zero app that search for duplicate MIFARE tag saved by checking UID.

**TagDuplicate v1.0** is an NFC utility for Flipper Zero running Momentum firmware, created by **Marcopiiit**.

It reads NFC tag UIDs and checks them against saved MIFARE `.nfc` files. It can also identify duplicate saved files that share the same UID.

## Features

- **Read** — Read the UID of an NFC tag and search for a matching saved MIFARE file.
- **Check Duplicate** — Scan saved NFC files and identify groups of files sharing the same UID.
- **Log** — View previous searches and duplicate checks.
- **About** — Display application information.
- **Progress Bar** — Monitor scanning progress during file comparisons.
- **UID Cache** — Speed up subsequent scans by caching saved file UIDs.
- **Automatic Logging** — Save search results to the Flipper Zero SD card.
- **LED, Vibration and Sound Feedback** — Receive notifications when a tag is found or not found.

## Main Menu

The application provides four menu options:

| Option | Description |
|---|---|
| Read | Read a tag and search for its UID in saved NFC files. |
| Check Duplicate | Find saved NFC files with matching UIDs. |
| Log | Browse and manage previous search records. |
| About | Display the application version and author. |

## How to Use

### 1. Read a Tag

1. Open **TagDuplicate**.
2. Select **Read**.
3. Place the NFC tag against the back of the Flipper Zero.
4. Wait for the UID to be read and compared with saved files.
5. Check the result displayed on the screen.

Possible results:

- **Found** — The UID matches a saved NFC file. The matching filename is displayed.
- **Not Found** — No matching saved file was found.
- **NFC Folder Not Found** — The expected NFC directory is unavailable.

#### Feedback

When a matching file is found:
- The green LED blinks three times.
- The device vibrates and plays a beep with each blink.

When no matching file is found:
- The red LED turns on.
- The device vibrates for approximately one second.

### 2. Check Duplicate

1. Open the main menu.
2. Select **Check Duplicate**.
3. Wait for the application to scan the saved NFC files.
4. Review the results.

The application groups files that contain the same UID.

Example:

```text
UID 04 A1 B2 C3 D4 E5 F6
- card_01.nfc
- card_backup.nfc

UID 12 34 56 78
- tag_copy.nfc
- tag_copy_2.nfc
```

If no duplicate groups are detected, the application displays a message indicating that no duplicates were found, along with the number of eligible files checked.

**Important:** Duplicate detection compares UIDs only. Files sharing a UID are not necessarily identical in their contents or functionality.

### 3. View the Log

Select **Log** from the main menu to view previous operations.

The log records:
- Date and time.
- Read operations and their results.
- Matching filenames.
- Duplicate-check summaries.
- Errors such as a missing NFC directory.

#### Log Controls

| Button | Action |
|---|---|
| Up | Select the previous entry. |
| Down | Select the next entry. |
| OK | Open the selected entry. |
| Left | Delete the selected entry. |
| Back | Return to the main menu. |

The newest entries appear first. The application retains up to 40 entries in its active log, dropping the oldest entries when the limit is reached.

## Supported Files

TagDuplicate searches the following directory:

```text
/ext/nfc/
```

It scans files with the `.nfc` extension and considers files whose `Device type` field contains the text `Mifare`, ignoring letter case.

The default filter is:

```c
#define MIFARE_FILTER "Mifare"
```

This filter can match device types containing names such as MIFARE Classic, Ultralight, DESFire or Plus, provided their saved file metadata contains the expected text.

The application reads the `UID` field from eligible files.

**Note:** The application compares UIDs; it does not compare encryption keys, sector data or the complete contents of NFC dumps.

## Cache System

To improve performance, TagDuplicate caches information about saved NFC files.

Cache location:

```text
/ext/apps_data/tag_duplicate/cache.bin
```

For each cached file, the application stores:
- Relative filename.
- UID and UID length.
- File size.
- Modification timestamp.

Unchanged files do not need to be parsed again. A file is reread when it is new or its size or modification timestamp changes.

The cache is refreshed during scans, and entries for deleted files are removed after a completed scan.

The cache supports up to 400 files.

## Log Storage

The application automatically saves its log to:

```text
/ext/apps_data/tag_duplicate/log.txt
```

The application creates its data directory when needed.

The log is retained on the SD card between application sessions, subject to the application's log limit and storage availability.

## Installation

Install TagDuplicate using the appropriate build and installation workflow for your Flipper Zero firmware.

For a Momentum development environment:

1. Place the application source file `tag_duplicate.c` in your application project directory.
2. Ensure that the project has the required Flipper Zero SDK headers and libraries available.
3. Configure the application manifest (`application.fam`) with the correct entry point and dependencies.
4. Build the application using the development environment compatible with your Momentum firmware version.
5. Copy the compiled `.fap` file to the Flipper Zero SD card, typically under:

```text
/ext/apps/NFC/
```

6. Open the application from the Flipper Zero application menu.

The source file alone is not a compiled application. A compatible manifest and successful build are required.

## Limitations

- The application is designed for NFC tags using the ISO14443-3A polling protocol.
- It reads the UID; it does not dump or modify tag contents.
- Duplicate detection is based exclusively on matching UIDs.
- Only saved `.nfc` files with a matching MIFARE device-type field and a valid UID are considered.
- The cache supports a maximum of 400 entries.
- The application uses file size and modification timestamp to detect changes; files modified without either value changing may not be refreshed.
- Read searches can stop at the first matching file.
- Scanning and results depend on the availability and integrity of the SD card files.

## Compatibility

- **Device:** Flipper Zero
- **Firmware:** Momentum
- **Category:** NFC
- **Version:** 1.0

Compatibility with other firmware versions depends on the availability of the required APIs and successful compilation.

## Author

**Marcopiiit**

## Disclaimer

This application is intended for managing and checking NFC files that you own or are authorized to inspect.

Always comply with applicable laws and respect the privacy and property of others.
