# CK42X PassVault

CK42X PassVault is a small Flipper Zero field utility for storing a short list of local account entries, generating readable passwords, and typing a selected password over USB HID only after confirmation.

## Features

- Add account, username, and password entries on the Flipper
- Generate passwords with memorable, strict, long, and no-symbol presets
- Select a saved entry and review it on-device
- Explicit confirmation before HID password typing
- CK42X.com branding and crowned bee app icon

## Security note

This version stores entries as plaintext TSV in Flipper app data. Treat it as a field utility and prototype, not as a hardened password manager for high-value secrets. Master unlock and encrypted storage are planned hardening steps.

Website: https://ck42x.com
