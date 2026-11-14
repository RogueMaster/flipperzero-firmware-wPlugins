# Security

[← Documentation index](README.md)

How SSH Runner handles secrets, what it protects against and what it does not, and how to
report a vulnerability.

## Contents

- [Reporting a Vulnerability](#reporting-a-vulnerability)
- [What Is Secret, and Where It Lives](#what-is-secret-and-where-it-lives)
- [Private Keys on the SD Card](#private-keys-on-the-sd-card)
- [At Rest on the ESP32: One Image, Opt-In Protection](#at-rest-on-the-esp32-one-image-opt-in-protection)
- [Wiping a Board](#wiping-a-board)
- [Host Keys](#host-keys)
- [Logging](#logging)
- [Scripts](#scripts)

## Reporting a Vulnerability

Please report security problems privately through GitHub's private vulnerability reporting:
the repository's **Security** tab > **Report a vulnerability**. Do not open a public issue for
a vulnerability.

Include what you can of:

- the affected part (Flipper app, ESP32 firmware or `sshrunner`) and its version;
- the Flipper firmware (Unleashed or official, and its version) and the board;
- the steps to reproduce, and what an attacker gains;
- a proof of concept, logs or screenshots, with your own secrets removed.

You will get an acknowledgment, and the fix is released with a note in the
[changelog](../CHANGELOG.md). Only the latest release receives security fixes.

## What Is Secret, and Where It Lives

| Secret | Where | Never |
|---|---|---|
| SSH private keys | Flipper SD card, `/ext/apps_data/ssh_runner/keys/<name>` (encrypted or not, as the user put it there). Crosses the UART **once per connect**; held in ESP32 RAM for one authentication, then wiped | logged; stored on the ESP32 |
| Remembered key passphrases | ESP32 NVS, keyed by key fingerprint | written to SD; sent back to the Flipper |
| Remembered passwords (opt-in per host) | ESP32 NVS, keyed by host | written to SD; the Flipper only sees a "stored" flag |
| Typed passwords and passphrases | Flipper RAM while typing, one frame | logged, stored on SD; buffers are zeroed after use on both sides |
| WiFi credentials | ESP32 NVS (the WiFi driver runs with RAM storage) | stored on the Flipper |
| Pinned host keys (not secret, integrity-relevant) | ESP32 NVS, per host | — |

## Private Keys on the SD Card

- Keys are files, like `~/.ssh`: copy `id_ed25519`, `id_rsa`… into `keys/` with qFlipper, or
  generate one in the app (the ESP32 makes an Ed25519 or ECDSA P-256 key, and the Flipper saves
  it). Supported: Ed25519, ECDSA P-256/384/521, RSA ≥ 2048 with `rsa-sha2-256/512` only (never
  SHA-1 `ssh-rsa`). DSA and public-only files are rejected.
- The chosen key file crosses the short internal Flipper–ESP32 link once per connect, in
  chunks. The ESP32 parses it in internal RAM, uses it for this one authentication, and zeroes
  the buffer, the parsed key and any passphrase right after. It is never logged and never
  written to the ESP32's flash.
- Per host exactly one key is offered (IdentitiesOnly). A wrong host learns nothing about the key
  from a public-key authentication attempt beyond the public key itself.
- **Unencrypted key files** are as exposed as an unprotected `~/.ssh/id_*`: anyone who reads the
  SD card has the key. Use a passphrase for anything that matters (Keys > Generate Key offers
  one).
- **Passphrase "decrypt once".** Encrypted key files stay encrypted on the SD card. The first use
  asks for the passphrase; on success the ESP32 remembers it in NVS keyed by the key fingerprint
  (like macOS `UseKeychain`). Keys > *key* > **Forget Passphrase** removes it; deleting the key
  does too. Consequences:
  - an encrypted key on a lost SD card is useless without this ESP32;
  - the board alone holds only passphrases, not keys;
  - the Flipper with its SD card *and* the board together can use every key whose passphrase is
    remembered;
  - with plaintext NVS (the default, see below) a remembered passphrase is readable from a
    flash dump of the board. It is protected at rest only after **Protect Secrets**.
- **Unlocked keys (like ssh-agent).** After the first unlock the ESP32 keeps an encrypted key
  parsed in RAM so later connects skip the ~11 s bcrypt key derivation. It stays there while the
  board is powered, until **Forget Passphrase**, renaming or deleting the key, the console
  command `keys lock`, or a restart (each wipes it: `ssh_key_free()` zeroizes). The trade-off is
  ssh-agent's: whoever can drive the board while it runs (the Flipper with the app, or the USB
  console) can use the unlocked key without its passphrase; nothing of it is written to flash. A
  key unlocked with **Once** is not kept.
- Rotation: generate a new key, deploy its `.pub`, remove the old line from `authorized_keys`,
  delete the old key on the Flipper.

## At Rest on the ESP32: One Image, Opt-In Protection

- **One firmware image for everyone.** It boots with **plaintext NVS** and never burns an eFuse by
  itself (`CONFIG_NVS_ENCRYPTION`, flash encryption and secure boot are all off).
  `tools/flash.py` refuses an image built with any of them.
- **Plaintext means readable.** With physical access and a USB cable, anyone can dump the flash
  and read the WiFi password, remembered passwords, remembered key passphrases and pinned host
  keys, on every board that has not enabled **Protect Secrets**.
- **Protect Secrets** (Settings > Security on the Flipper): a warning dialog (default Cancel),
  then the user types `confirm` (case-insensitive, trimmed). The ESP32 checks the word again and
  refuses on any mismatch; no other message or console command can burn eFuses. It then burns a
  random HMAC key into one free eFuse key block (purpose `HMAC_UP`, read- and write-protected),
  derives the NVS encryption keys from it, copies the secrets into an encrypted NVS partition,
  and erases the plaintext partition. This is **permanent**; USB flashing and updates keep
  working.
  - How (`keystore_sec.c`): the copy goes into the `fes_sec` partition and is compared before a
    marker is written; the ESP32 then restarts and erases the plaintext partition at boot,
    before WiFi opens NVS. Any interrupted step resumes at the next boot; a power loss before the
    marker leaves the plaintext copy intact.
  - The only call that burns is `sec_burn()`, reached only after the word check (twice: in the
    link decoder and in `keystore_sec_enable()`). The image never sets `CONFIG_NVS_ENCRYPTION`,
    so no ESP-IDF path generates (burns) keys at boot. Enabling twice burns nothing.
  - Not yet verified on real silicon: the key derivation by the HMAC peripheral. Everything else
    is tested on the board with virtual eFuses, which derive the same keys in software.
  - Protects against: offline flash dumps.
  - Does not protect against: malicious firmware flashed onto the board over USB, which can use
    the HMAC peripheral to derive the same keys.
- **Not available: locking the board.** Flash encryption with Secure Boot v2 would also stop
  malicious firmware, but with stock ESP-IDF the bootloader enables both on its first boot,
  with no point at which the user could consent. SSH Runner does not offer it.

## Wiping a Board

- `wifi forget` on the ESP32's USB console erases the stored WiFi credentials.
- Deleting a host on the Flipper erases its remembered password and pinned host keys;
  **Forget Passphrase** or deleting a key erases its remembered passphrase.
- **Full wipe:** `esptool.py --chip esp32s3 -p <port> erase_flash` erases every secret on the
  board. Run it **before the board leaves your hands.** After **Protect Secrets** the eFuse key
  stays (it is permanent), but the encrypted data is gone and the board starts with an empty,
  still-encrypted store.
- The SD card is yours: delete `keys/` (or the whole `/ext/apps_data/ssh_runner/`) to remove the
  private keys from the Flipper.

## Host Keys

- Pinned per host (its name, or `host:port` for Quick Connect), several keys per host.
- Designed for several devices reached at the same IP (robots or appliances with a fixed
  default IP), where a changed key is often expected and a hard reject would make the tool
  unusable.
- `prompt` (default): an unknown key asks to trust or cancel; a key not in the trusted set warns
  and offers **Connect Once**, **Trust & Add**, **Replace All** or **Cancel**. `strict`: asks once,
  then refuses any other key. `off`: accepts silently and stores nothing. The policy code
  (`components/keystore/hostkey_policy.c`) is unit-tested, including "no password before the
  host-key decision".
- Decisions other than Cancel are logged in NVS (the `audit` console command; the count is shown
  per host).

## Logging

- libssh logging is off in released firmware (a build option, verbosity 0).
- Forwarding warnings to the Flipper (`LOG_EVT`) is a debug-only build option.
- No code path logs a password, a passphrase or key material; the console's `pw` command wipes
  its argument after use.

## Scripts

Scripts are plain files on the SD card and run as the SSH user. The app always shows the
name, size and interpreter and asks before running one.

## See Also

- [Quick Start: Wiping the Board](QUICKSTART.md#6-wiping-the-board)
- [UART Protocol](PROTOCOL.md): the secret-protection messages
