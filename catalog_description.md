# CK42X PassVault

CK42X PassVault is a Flipper Zero app for storing, generating, and typing passwords from the device.

It lets you save a small list of account entries, generate passwords from readable presets, and type a selected password over USB HID after explicit confirmation on the Flipper.

Generated passwords use the Flipper RNG. Before saving a generated password, the app checks it against the passwords already saved in the vault and changes it if needed so the generated password is unique within the current vault.

## Security note

v0.3 stores entries as plaintext TSV in Flipper app data. Anyone with access to the SD card or app data can read the saved passwords. Use it only for passwords you are comfortable storing on the device in plaintext.

Planned hardening includes a master unlock/PIN gate, encrypted vault storage, and edit/delete flows in the UI.
