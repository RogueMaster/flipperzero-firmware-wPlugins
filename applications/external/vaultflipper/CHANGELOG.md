# Changelog

All notable changes to this project are documented here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [0.1.0] - 2026-11-07

### Added

- Flipper app: unlock with an arrow-key PIN, browse accounts and folders, type username, password or the full login over USB, or show an entry.
- Large vaults: the vault is stored in encrypted pages, so thousands of logins fit and unlocking stays at about one second.
- Search by name, username or URL on the Flipper.
- 2FA codes: every login with a TOTP secret appears under *2FA codes* with its current code and a countdown; a big code view types the code on OK. SHA-1, SHA-256, SHA-512 and Steam Guard are supported. The companion measures the Flipper's clock at every sync, and the time zone can be adjusted on the Flipper.
- Folder lists and search results show the username of every login, so entries with the same name are easy to tell apart.
- Clear design with icons: home with your accounts, folders, and an entry card where OK types the login, Left the username and Right the password.
- Settings in one place: keyboard layout, Mac keyboard setup, typing test, time zone, language, help and about.
- The LED blinks green when typing is done; if a character does not exist in the keyboard layout, the Flipper says how many were left out.
- While unlocked the Flipper stays connected as "VaultFlipper Keyboard", like a security key, so typing starts instantly.
- One-click Mac keyboard setup: the Flipper answers the macOS Keyboard Setup Assistant for you.
- Lock by holding Back, from the menu, or automatically after 2 minutes. After 10 wrong PINs the vault is deleted.
- Encryption bound to your Flipper's crypto enclave and your PIN.
- Keyboard layout picker with all BadUSB layouts and a bundled German Mac layout, plus a typing test.
- English and German interface.
- Companion CLI: guided setup with a clear, colored terminal interface, sign-in with master password, single sign-on (SSO) or API key, several Bitwarden and Vaultwarden accounts, shared organizations without duplicates, folder, collection and favorite filters, pairing and sync over USB, status check, `--no-totp` to leave out 2FA secrets.
- The keyboard layout of your computer is detected automatically, and the PIN can be entered with the arrow keys.
- User guide in English and German.
- If one account cannot be read, the others are still synced after you confirm.

[Unreleased]: https://github.com/its-sven/vaultflipper/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/its-sven/vaultflipper/releases/tag/v0.1.0
