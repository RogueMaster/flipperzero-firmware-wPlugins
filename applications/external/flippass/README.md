# FlipPass

FlipPass lets Flipper Zero open a KeePass vault and send credentials to another device through USB HID or Bluetooth HID.

## Features

- Browse KeePass KDBX 4 vaults stored on the SD card.
- Navigate groups and entries without modifying the database.
- Show username, password, URL, notes, AutoType sequences, and custom fields on-device.
- Type username, password, AutoType sequences, or custom fields over USB HID or Bluetooth HID.
- Reuse the default KeePass AutoType sequence when an entry does not define a custom one.

## Requirements

- A Flipper Zero with an SD card.
- A KeePass database in KDBX 4 format.
- AES256 or ChaCha20 database encryption.
- Raw or GZip-compressed payloads.

## Usage

0. Download and use [KeePass](https://keepass.info/download.html) to create a password vault. (With KDF AES)
1. Copy your .kdbx file to /ext/apps_data/flippass/. (or your preferred directory)
2. Launch FlipPass and choose the database from the browser.
3. Enter the master password for the selected vault.
4. Open a group or entry, then choose whether to view a field or type it to the connected host.
5. For Bluetooth HID, pair or reconnect the host before sending credentials.
6. To select the keyboard layout, hold the chosen typing method (USB or BT).
7. To exit quickly, hold Back.

## Notes

- FlipPass is a read-only browser. It does not edit or save KeePass entries.
- The application can write via USB HID or via Bluetooth HID profile (preferably paired with the BadUSB app beforehand).
- To reduce typing errors, FlipPass can send characters using the Windows Alt+Num Keypad input method. (This is the default method.)
- Due to RAM requirements and limitations, the app may close during decompression or when the qFlipper app connects simultaneously.
- The screenshots use demo data only.
