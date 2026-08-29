# Changelog

## v0.1

Initial release.

- Read, write, and browse an enclave-encrypted vault stored in Sector 1 of an
  NTAG I2C Plus 2K implant (Dangerous Things xSIID).
- Secrets and notes up to 255 characters — enough for a 24-word BIP39 seed
  phrase.
- Vault plaintext is heatshrink-compressed before encryption to stretch the
  tag's fixed 1024-byte Sector 1.
- Optional 6-digit vault PIN: stretched (PBKDF2 + enclave-iterated AES) into
  key material that wraps the vault key. No verifier by design — a stolen
  Flipper alone offers nothing to brute-force against.
- AES-256-GCM vault sealed with a device-unique key held in the STM32WB secure
  enclave; nothing is recoverable off-device.
- On-screen vault browser with add / view / edit / remove and an extended
  keyboard for symbols.
- Send a username, password, or note to a host over USB-HID.
- Optional tag password protection (opt-in): device-bound, UID-diversified
  PWD/PACK, selectable read-protect and auth-limit, reversible unprotect.
- A biovault USB CLI mirroring every on-device command.
- Key failures are explained on screen instead of only in the serial log: a
  missing device key, a PIN that has not been entered, an unreadable keystore,
  and a vault written by a different Flipper each get their own message and a
  short "what to do", on the device and over the CLI.
