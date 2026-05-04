# FlipPass

FlipPass is the Flipper Zero implementation of the KeePass password manager; it creates, opens, edits KDBX files, and sends credentials/OTP to another device via USB HID or Bluetooth HID.

## Features

- Create/Browse/Modify KeePass KDBX v4 vaults stored on the SD card.
- Navigate and modify groups and entries of the database.
- Modify the password, encryption type, and KDF derivation effort of an existing KDBX.
- Show username, password, URL, notes, AutoType sequences, OTP, and custom fields on-device.
- Type username, password, AutoType sequences, OTP values, or custom fields over USB HID or Bluetooth HID.
- Generates passwords based on SubGhz noise and user inputs entropy (Trying to be "True Random")
- Create and calculate HMAC-OTP and TIME-OTP configurations as KeePass. (In the editing/creation of an entry, in the OTP field)
- Reuse the default KeePass AutoType sequence when an entry does not define a custom one.
- It locks and eventually closes itself, at configurable intervals.

## Requirements

- A Flipper Zero with an SD card.
- Limited to AES-KDF algorithm

## Usage

1. Launch FlipPass and choose the database from the browser or create one on your preferred directory
2. Enter the master password for the vault.
3. Create/Open a group or entry, then choose whether to view a field or type it to the connected host.
    - To edit a group or entry Hold "OK"
    - To generate a password, leave the "password" field setting blank in an entry (or the "value" field of a protected "Custom Field").
4. For Bluetooth HID, pair or reconnect the host before sending credentials.
5. To select the keyboard layout, hold the chosen typing method (USB ("Right") or BT ("Left")).
    - For stop tipyng, press "Back".
6. To exit quickly, hold "Back".

## Notes

- The application can write via USB HID or via Bluetooth HID profile (preferably paired with the BadUSB app beforehand).
- To reduce typing errors, FlipPass can send characters using the Windows Alt+Num Keypad input method. (The default method is either this one or the last keyboard layout used in BadUSB.)
- Due to RAM requirements and limitations, the application may not function during decompression or saving if the qFlipper application is connected simultaneously.
- Depending on the number and size of database entries, you may need to use an encrypted binary file in flash memory to store the entry model. This will be explicitly requested.
- In File Explorer > Menu > Config: You can configure auto-lock and closes times, as well as attempts to unlock before deleting the active session and pre-authorized use of the encrypted flash session (/ext)
- It is highly recommended to use uncompressed databases
- An effort of 10k AES-KDF iterations is approximately one and a half seconds of processing time on the Flipper. By default, KeePass offers 600k iterations. Although the Flipper has been tested with 2500k iterations, it is the user's responsibility to decide the effort and time invested.
- The screenshots use demo data only.
