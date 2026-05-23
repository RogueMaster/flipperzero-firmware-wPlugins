# CK42X PassVault

CK42X PassVault is a Flipper Zero app for storing, generating, and typing passwords from the device.

It lets you save a small list of account entries, generate passwords from readable presets, and type a selected password over USB HID after explicit confirmation on the Flipper.

Generated passwords use the Flipper RNG. Before saving a generated password, the app checks it against the passwords already saved in the vault and changes it if needed so the generated password is unique within the current vault.

## Security note

v0.4 stores the active vault as AES-GCM encrypted app data and requires a master PIN to unlock. On first setup it migrates legacy plaintext `vault.tsv` once, then removes it after the encrypted save succeeds.

This is a small Flipper utility, not a hardened audited password manager: device compromise, weak PINs, shoulder surfing, or modified firmware can still expose vault contents.
