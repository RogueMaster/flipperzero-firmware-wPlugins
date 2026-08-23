# Changelog

## v0.1

Initial release.

- Read, write, and browse an enclave-encrypted vault stored in Sector 1 of an
  NTAG I2C Plus 2K implant (Dangerous Things xSIID).
- AES-256-GCM vault sealed with a device-unique key held in the STM32WB secure
  enclave; nothing is recoverable off-device.
- On-screen vault browser with add / view / edit / remove and an extended
  keyboard for symbols.
- Send a username, password, or note to a host over USB-HID.
- Optional tag password protection (opt-in): device-bound, UID-diversified
  PWD/PACK, selectable read-protect and auth-limit, reversible unprotect.
- `biovault` USB CLI mirroring every on-device command.
