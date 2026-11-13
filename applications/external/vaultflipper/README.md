<div align="center">

<img src="docs/logo.svg" width="112" alt="VaultFlipper logo">

# VaultFlipper

**Your Bitwarden / Vaultwarden logins on a Flipper Zero – offline, encrypted, typed over USB.**

[![Release](https://img.shields.io/github/v/release/its-sven/vaultflipper?sort=semver)](https://github.com/its-sven/vaultflipper/releases)
[![CI](https://github.com/its-sven/vaultflipper/actions/workflows/ci.yml/badge.svg)](https://github.com/its-sven/vaultflipper/actions/workflows/ci.yml)
[![Flipper Zero](https://img.shields.io/badge/Flipper_Zero-official_firmware-FF8200)](https://flipperzero.one)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue)](LICENSE)

<img src="docs/screenshots.png" alt="VaultFlipper: home, folder with usernames, entry card, 2FA codes, 2FA code, settings" width="760">

**[User guide](docs/GUIDE.md)** · **[Anleitung (Deutsch)](docs/GUIDE.de.md)** · [Security](SECURITY.md) · [File format](docs/FORMAT.md)

</div>

> [!WARNING]
> VaultFlipper is young and has not had an independent security review. Read [how it protects your data](#security) before you trust it with important accounts.

## Features

- **Works like a security key** – unlock with your PIN and the Flipper becomes a USB keyboard that types username, password or the whole login, on computers and phones.
- **All your vaults** – bitwarden.com, bitwarden.eu and self-hosted Vaultwarden side by side, with master password, single sign-on (SSO) or API key, including shared organizations.
- **Large vaults** – thousands of logins in folders, each shown with its username, and search by name, username or URL. Only the folder you open is decrypted, so unlocking takes about a second either way.
- **2FA codes** – every login with a TOTP secret in one list with its current code; OK types it. SHA-1/256/512 and Steam Guard.
- **Bound to your device** – the vault file only opens on your Flipper and with your PIN.
- **A PIN on the D-pad** – 8 to 32 arrow presses; slows down after wrong tries and wipes after 10.
- **Every keyboard layout** – detected from your computer, switchable on the Flipper, with a typing test and one-click Mac keyboard setup.
- **Guided setup** – `vaultflipper setup` walks you through accounts, pairing, layout and the first sync.
- **English and German**, on the Flipper and in the terminal.

## How it works

```
 Bitwarden / Vaultwarden ──bw CLI──▶  vaultflipper (computer) ──USB──▶  VaultFlipper app (Flipper Zero)
   one or more accounts               filter · encrypt for             PIN ▸ account ▸ folder ▸ entry
                                      this device + your PIN           ▸ types as a USB keyboard
```

The companion reads your logins with the official [Bitwarden CLI](https://bitwarden.com/help/cli/) – your master password goes straight into `bw`, never into VaultFlipper. It encrypts everything for your Flipper and your PIN and copies it over USB. Sync is one-way; nothing is written back to your vault.

## Quick start

You need a Flipper Zero with the official firmware (1.5+), Python 3.11+ with [uv](https://docs.astral.sh/uv/) and the Bitwarden CLI.

1. **App** – copy `vaultflipper.fap` from a [release](https://github.com/its-sven/vaultflipper/releases) to `apps/Tools/` with qFlipper (or `make launch`), and open it once.
2. **Companion**

   ```bash
   uv tool install "git+https://github.com/its-sven/vaultflipper#subdirectory=companion"
   ```

3. **Setup** – close qFlipper and the app, then:

   ```bash
   vaultflipper setup
   ```

4. **Use it** – open VaultFlipper, press your PIN, pick a login and press **OK**.

The [user guide](docs/GUIDE.md) explains every step, the Mac keyboard setup, phones and troubleshooting.

## On the Flipper

| | |
| --- | --- |
| **Home** | your accounts, *2FA codes*, *Search*, *Settings*, *Lock* |
| **Entry card** | **OK** login + Enter · **◀** username · **▶** password · **▲** show password · **▼** more |
| **2FA code** | **OK** types the code · **◀ ▶** previous / next |
| **Settings** | keyboard layout, Mac keyboard setup, typing test, time zone, language, help, about |
| **Lock** | hold **Back** anywhere, or wait 2 minutes |

## Companion commands

| Command | What it does |
| --- | --- |
| `setup` | guided setup: accounts, sign-in, pairing, layout, first sync |
| `sync` | fetch all accounts, encrypt, send to the Flipper (`--dry-run`, `--out FILE`) |
| `status` | check `bw`, accounts and the paired Flipper |
| `account add/list/remove` | manage accounts; filters `--folder`, `--collection`, `--favorites-only`, `--no-orgs`, `--no-totp` |
| `login NAME [--sso \| --apikey]` | sign in with `bw`; the method is remembered |
| `pair` | read the Flipper's device key over USB |
| `typetest` | check the keyboard layout |

## Security

| | |
| --- | --- |
| **Device binding** | An X25519 key derived from the Flipper's crypto enclave. The companion encrypts for that key, so the file is useless on any other device – also while it is copied. |
| **PIN** | Argon2id (~1 s on the Flipper) combined with the device secret; every page sealed with XChaCha20-Poly1305 and bound to its position. |
| **Wrong PINs** | slowdown from the 3rd try, vault deleted after 10. |
| **In memory** | only the open folder is decrypted, and wiped on lock and exit. |

**2FA codes** put the second factor on the same device as the password; use `--no-totp` to keep them apart.

**Know the limits.** The Flipper has no secure boot: whoever holds your Flipper can flash their own firmware and guess PINs on the device (about one per second). Choose **12 or more** presses. Anything you type over USB can be read by that computer. Details in [SECURITY.md](SECURITY.md); please report vulnerabilities privately.

## Development

```bash
make test      # companion tests + the Flipper crypto core built natively
make fap       # build the Flipper app with ufbt
make launch    # build, install and start on a connected Flipper
```

| Folder | Content |
| --- | --- |
| `flipper/vaultflipper/` | Flipper app (C): `vf_core` format and crypto, `vf_totp` 2FA codes, `vf_ui` views, `vf_hid` keyboard, `vf_device` enclave |
| `companion/` | companion CLI (Python) |
| `tests/native/` | builds the app's crypto core natively and checks it against the companion |
| `tools/` | icons, keyboard layouts, remote control and screenshots over USB |
| `docs/` | user guides, file format |

See [CONTRIBUTING.md](CONTRIBUTING.md).

## Credits

- [Monocypher](https://monocypher.org) by Loup Vaillant (BSD-2-Clause or CC0) – crypto on the Flipper.
- [PyNaCl](https://pynacl.readthedocs.io) / libsodium and [Rich](https://rich.readthedocs.io) – crypto and terminal UI in the companion.
- Inspired by the Flipper community password apps, especially [Flipper Authenticator](https://github.com/akopachov/flipper-zero_authenticator).

VaultFlipper is not affiliated with Bitwarden, Inc. or Flipper Devices Inc.

## License

[MIT](LICENSE) © 2026 Sven Würth
