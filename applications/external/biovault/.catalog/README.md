BioVault keeps a password vault **encrypted on an NFC implant**, unlockable
only by the Flipper that owns it.

The encrypted vault lives on the implant (Dangerous Things xSIID, or any NTAG
I2C Plus 2K). The key that decrypts it lives only in the Flipper's secure
enclave and never leaves it. You need **both** the implant and its Flipper to
read anything; either one on its own reveals nothing. Load the vault from the
implant, browse and edit it on-device, and type credentials straight into a
computer over USB. No external tooling, everything in RAM.

## Features

- Read, write, and browse an encrypted credential vault on the implant.
- Add, view, edit, and remove credentials and notes on-device, with an
  extended keyboard for symbols.
- Secrets and notes up to 255 characters per entry, enough for a 24-word seed
  phrase, with the vault compressed before encryption.
- Send a username, password, or note to a computer over USB-HID, as if typed
  on a keyboard.
- Optional implant password protection: the implant itself refuses reads
  without a device-bound, per-implant password, with a configurable
  failed-attempt lockout.
- Optional 6-digit vault PIN, stretched through the secure enclave into key
  material that wraps the vault key. Not a check that can be patched out, and
  brute-forcing it requires this Flipper, one multi-second guess at a time.
- A USB serial CLI (run biovault) that mirrors every on-device command.

## Security

The vault is sealed with AES-256-GCM. The data key is wrapped by a
device-unique key generated inside the STM32WB55 secure enclave and never
leaves the chip. A lost Flipper exposes nothing, since the data is only on the
implant. A scanned implant yields only ciphertext. With a PIN set, even the
Flipper alone gives an attacker nothing to test guesses against.

Full documentation, security model, and source:
[github.com/flamebarke/biovault-flipper](https://github.com/flamebarke/biovault-flipper)
