# Security

## Reporting a vulnerability

Please do not open a public issue. Use [GitHub's private vulnerability reporting](https://github.com/its-sven/vaultflipper/security/advisories/new) instead. You will get an answer within a week.

## Threat model

VaultFlipper aims to keep your logins safe when the **Flipper or its SD card is lost or stolen** and when the **vault file is intercepted** while it is copied.

| Situation | Protection |
| --- | --- |
| Someone copies the SD card or the `.vfv` file | Useless without the Flipper's enclave key *and* the PIN. |
| Someone has the Flipper and tries PINs in the app | Slows down from the 3rd try, wipes the vault after 10. A try counts before it is checked, so a reset cannot skip it. With access to the SD card the vault file can be backed up and restored, so the wipe only slows guessing down; the real protection is a long PIN. |
| Someone has the Flipper and flashes custom firmware | Can try PINs on the device at about one per second; the counter does not help. A PIN with 12+ presses (16 million combinations) makes this take months. |
| The computer you type into is compromised | Not protected. Typed text can be read like any keyboard input. |
| Someone watches you enter the PIN | Not protected. |
| Memory of the running app is read with a debugger | Not protected while unlocked; plaintext is wiped on lock and exit. |

## Design notes

- The companion never asks for your master password; `bw` does. Session keys stay in memory and `bw lock` runs after every sync.
- The vault on the Flipper is read-only. Nothing is synced back.
- Every page of the vault is encrypted on its own and bound to its position in the file; only the open page is decrypted in RAM.
- **2FA secrets.** Logins with a TOTP secret show their codes on the Flipper. Keeping password and second factor on one device means whoever unlocks it has both – the same trade-off as storing TOTP in Bitwarden itself. To keep the second factor elsewhere, add the account with `--no-totp`; the secrets are then left out of the vault.
- Cryptography: X25519, BLAKE2b, Argon2id and XChaCha20-Poly1305 from [Monocypher](https://monocypher.org) (Flipper) and libsodium (companion). See [docs/FORMAT.md](docs/FORMAT.md).
