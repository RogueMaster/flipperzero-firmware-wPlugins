# CK42X PassVault

CK42X PassVault is a Flipper Zero app for storing, generating, editing, and typing passwords from the device. Version 0.4.6 removes the old 20-password vault cap and still includes experimental FIDO2 WebAuthn registration and authentication plus an explicit macOS ANSI keyboard setup flow.

It lets you save and edit as many account entries as Flipper memory and SD space allow, generate passwords from readable presets, and type a selected password over USB HID after explicit confirmation on the Flipper. After macOS identifies the keyboard once, PassVault keeps the HID session active for repeated injections until the app exits.

Generated passwords use the Flipper RNG. Before saving a generated password, the app checks it against the passwords already saved in the vault and changes it if needed so the generated password is unique within the current vault.

## FIDO2 security key

The experimental FIDO2 runtime supports FIDO 2.0, ES256, non-resident credentials, allow lists, and physical user presence. Every registration and authentication ceremony requires an explicit Approve or Deny decision on the Flipper. Exiting FIDO2 mode restores the previous USB configuration.

FIDO credentials use a separate AES-GCM encrypted store and do not change the password vault. Keep another authenticator and account recovery method for every account.

## Security note

PassVault stores the active password vault and FIDO credentials in separate AES-GCM encrypted app-data files and requires a master PIN to unlock. On first setup it migrates legacy plaintext vault.tsv once, then removes it after the encrypted save succeeds.

This is an experimental, unaudited Flipper utility. Flipper Zero has no secure element, and this build is not FIDO certified. Device compromise, weak PINs, shoulder surfing, debug access, or modified firmware can expose protected material. Client PIN, resident credentials, and user verification are unsupported.
