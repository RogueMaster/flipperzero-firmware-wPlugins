# Password Keyboard

Password Keyboard is a native Flipper Zero Bluetooth application that generates,
stores, and types passwords as a BLE HID keyboard.

## What it does

- Starts in a Bluetooth connection screen and advertises as a keyboard.
- Creates 4–64 character passwords from configurable lowercase, uppercase,
  number, and special-character sets.
- Prompts for a custom name before generating each password.
- Supports normal passwords (type or reveal) and hidden passwords (type only).
- Gives each hidden password its own configurable daily allowance (1–9,
  default 3).
- Counts only successful confirmed typing attempts. The allowance resets on the
  next calendar date according to the Flipper RTC.
- Entries made while creating and testing a password do not consume its daily
  allowance; counting begins after the password is saved.
- Lets a newly generated password be typed once, optionally typed again,
  followed by Tab or Enter, then saved without revealing a hidden password.
- Supports manually entered passwords using the selected normal or hidden type.
- Opens Rename and Delete actions when a saved password is selected by holding
  OK; deletion requires confirmation.
- Includes a Settings screen for the persisted default password name.
- Stores all password payloads in a device-bound authenticated scrambled form.
- Imports regular passwords from a simple file on the Flipper filesystem.

## Build

Install [uFBT](https://pypi.org/project/ufbt/), select the official release SDK,
and build:

```sh
ufbt update --channel=release
ufbt
```

The `.fap` is produced under `dist/`. To build and launch on an attached
Flipper:

```sh
ufbt launch
```

Run the host-side cryptography tests with:

```sh
sh tests/run.sh
```

The SDK API must be no newer than the API on the Flipper. Firmware 1.4.3 uses
API 87.1; the matching uFBT release SDK builds a compatible app. The manifest
links the official `ble_profile` library, following the firmware's own
Bluetooth HID application.

## Bluetooth pairing

1. Open Password Keyboard on the Flipper.
2. Open Bluetooth settings on the target computer or phone.
3. Pair with the advertised `PassKey` keyboard.
4. Once connected, the Flipper moves to the password list.

Pairing keys are isolated in the app data directory. The normal Flipper
Bluetooth profile is restored when the app exits.

## Filesystem import

Create this file on the SD card app-data area:

```text
/ext/apps_data/password_keyboard/import.txt
```

Each non-comment line is `name|password`:

```text
# Password Keyboard import v1
Work email|correct horse battery staple
Router|p@ssw0rd-example
```

Imported entries are regular passwords. After a successful import, the
plaintext import file is removed. The vault remains at:

```text
/ext/apps_data/password_keyboard/vault.bin
```

The Flipper application sandbox exposes that same location internally as
`/data/vault.bin`.

## Security model

The vault never stores plaintext passwords. It derives a key from the Flipper's
hardware UID, uses random per-record nonces, encrypts the bytes with a keyed
BLAKE2s stream, and authenticates each ciphertext before decrypting it. Plaintext
buffers are wiped after typing or leaving the reveal screen.

This is strong practical concealment against casual SD-card inspection, not a
hardware-backed secret boundary. A person who controls the Flipper and can run
modified firmware can recover any password that the device itself can type.
Hidden mode prevents accidental viewing and enforces each password's configured
daily limit in the normal app; it cannot cryptographically defend that limit
against the device owner.

The HID character map assumes a US keyboard layout. Special characters may be
different when the target computer uses another keyboard layout.

Idea by: Evgeniy Raev.
