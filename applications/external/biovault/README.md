# BioVault

BioVault keeps a password vault **encrypted on an NFC implant**,
unlockable only by the Flipper that owns it.

The encrypted vault lives on the implant. The key that decrypts it lives only in
the Flipper's secure enclave and never leaves it. You need **both** the implant
and its Flipper to read anything; possession of either one on its own reveals
nothing. Read the implant using the Flipper, load the vault, and browse, edit,
or type your credentials straight into a computer over USB. No external
tooling, no `proxmark`, everything in RAM.

## How it works

BioVault splits your secrets across two things you hold:

- **The implant** stores the vault as one AES-256-GCM encrypted blob in Sector 1.
  On its own it is just ciphertext, and tampering with it is detected.
- **The Flipper** holds the decryption key, wrapped by a device-unique key that
  is generated inside the STM32WB55 secure enclave and can never be read back
  out. On its own it has nothing to decrypt.

When you Load, the Flipper reads the blob from the implant, decrypts it in RAM,
and shows the vault. Edits stay in RAM until you Save them back to the implant.
Nothing is ever written to the Flipper's storage except the wrapped key.

## Hardware

- **Dangerous Things xSIID** implant, or any **NTAG I2C Plus 2K** implant
  (NXP NT3H2211).
- The vault occupies **Sector 1** only. Sector 0 (UID, NDEF records) is left
  untouched, so the implant still works as a normal NFC tag alongside the
  vault.

## Features

- Read, write, and browse an encrypted credential vault on the implant.
- Add, view, edit, and remove credential and note entries on-device, with an
  extended keyboard for symbols.
- Secrets and notes up to **255 characters** per entry, enough for a 24-word
  seed phrase, with the vault compressed before encryption to make the most of
  the implant's 1 KB.
- **Send** a username, password, or note to a computer over USB-HID, with an
  optional trailing Return.
- **Optional implant password protection**, so the implant itself refuses reads
  without a device-bound password (see *Implant protection*).
- **Optional 6-digit vault PIN** wrapping the vault key with PIN-derived key
  material (see *Vault PIN*).
- A **`biovault` USB serial CLI** that mirrors every on-device command, so you
  can manage the vault and add or copy passwords from a terminal.

## Usage

The app opens on **Load from Implant** (after the PIN screen, if a vault PIN is
set); hold the implant to the Flipper, then it drops into the main menu:

| Menu item | What it does |
| --- | --- |
| **Vault** | Browse entries; open one to View, Send (USB-HID), Edit, or Remove it. |
| **Add Entry** | Add a credential (label + username + password) or a note (label + text). |
| **Load / Read / Save / Wipe Implant** | Sync the vault to and from the implant. Load first, so Save never overwrites the implant with an unsynced vault. |
| **Settings** | USB auto-return, implant password protection, vault PIN. |
| **About / Diagnostics** | Usage notes and crypto self-test results. |

### CLI

Connect over USB serial, run `biovault` to open a subshell, and use:

```
Vault:    list, get <label>, add <label>, edit <label>, remove <label>
Device:   read, load, save, wipe, reveal
Protect:  protect, unprotect        (confirmed on the Flipper)
Config:   settings [<key> <value>], pin <set|remove|enter>
```

`reveal` prints the implant's password to the terminal (handy for a Proxmark),
but only after the provisioned implant authenticates.

## Security model

The design goal is that **your data stays safe even if you lose the Flipper**,
because the vault is never stored on the Flipper, only on the implant.

| Scenario | What the attacker has | Outcome |
| --- | --- | --- |
| Lost or stolen Flipper (even unlocked) | The wrapped key, but no data | Nothing exposed. The enclave key is gone, so the vault becomes unrecoverable. A recoverability loss, not a confidentiality loss. |
| Implant read over NFC | The ciphertext blob | Useless without your Flipper's enclave key, which can't be cloned to another device. With implant password protection enabled they can't even read the blob. |
| `keystore.bin` copied off the SD card | The wrapped vault key | Doesn't matter. The wrap can only be undone inside this Flipper's enclave (and needs the PIN too, if set), so the file is useless on any other hardware. |
| Your Flipper *and* NFC range to your implant | Both factors | The only real exposure, and with a vault PIN set they still need that too. |

The Flipper is not a hardened secure element, and it does not need to be here:
it never holds the plaintext vault at rest, only transiently in RAM while the
implant is coupled. Keeping the data off the Flipper is what makes a lost
device a non-event for confidentiality.

### Vault PIN (optional)

Settings → **Vault PIN** (or `pin set` in the CLI) wraps the vault key under a
6-digit PIN. This is **key material, not a check**: there is no PIN
verification anywhere in the code to patch out, and no verifier stored on the
Flipper. A stolen Flipper alone gives an attacker *nothing to even test PIN
guesses against*. If they also obtain the implant's ciphertext, every guess
must still run a multi-second key-stretching chain through this Flipper's
secure enclave (PBKDF2 plus ~16k enclave AES iterations, about 3 seconds per
guess), which cannot be offloaded to faster hardware: sweeping all 10^6 PINs
costs about a month of continuous on-device grinding. Guessing through a
read-protected implant instead runs into its auth limit, which locks the
implant when hit.

The trade-offs of having no verifier:

- **A wrong PIN cannot be detected at entry.** It just derives the wrong key;
  loads then fail authentication ("wrong PIN?").
- **With read-protection enabled, a wrong-PIN session presents a wrong implant
  password**, which burns one of the implant's failed-auth attempts per load if
  an auth limit is set. Keep the auth limit high, or off, when using a PIN.
- **A forgotten PIN loses the vault.** There is no reset that doesn't destroy
  the security. Removing the PIN is only allowed once the session is proven
  safe: after a load that decrypts the vault (proving the PIN), or after a load
  of a blank implant or a wipe (no ciphertext left to orphan).

## Backups and firmware updates

Two things must survive for the vault to keep working: the wrapped-key file on
the SD card (`apps_data/biovault/keystore.bin`) and the enclave key, which is
generated on-device by the hardware TRNG and lives in the STM32WB55's secure
storage. There is no recovery path for the enclave key.

| Event | Keystore file (SD) | Enclave key | Vault |
| --- | --- | --- | --- |
| Official firmware update | survives | survives | fine |
| Custom firmware, or switching firmware | survives | survives | fine |
| Radio stack (CPU2) update | survives | survives | fine |
| SD card formatted, corrupted, or replaced | **lost** | survives | restore `keystore.bin` from a backup |
| Coprocessor secure storage wiped, or hardware failure | survives (useless) | **lost** | gone, by design |

Backing up `keystore.bin` to a computer is safe and recommended: as the threat
table above notes, the file is useless off-device. A backup protects against
SD card loss only.

## Implant protection (chip-side)

Beyond the encryption, you can optionally enable the NTAG I2C Plus's **own
hardware protection**, so the implant itself refuses access to Sector 1 without
a password. It is opt-in from **Settings → Protect implant** and fully
reversible (**Unprotect implant**).

The NT3H2211 offers several protection mechanisms. BioVault uses them like
this:

| Mechanism | Register | How BioVault uses it |
| --- | --- | --- |
| Password authentication | `PWD_AUTH` / `PACK` | 32-bit password and 16-bit acknowledge, both derived from the enclave key and the implant UID: device-bound, unique per implant, never stored. The `PACK` the implant returns is verified on every unlock, so a cloned or emulated implant that merely spoofs the UID still can't authenticate. |
| Sector 1 gate | `2K_PROT` | Gates all of Sector 1 (the vault) behind the password. The core of the feature. |
| Read vs write protection | `NFC_PROT` | The **Read protect** setting chooses whether the password guards writes only (ciphertext stays readable but can't be altered or cloned) or reads and writes (ciphertext can't even be read off the implant). |
| Failed-attempt lockout | `AUTHLIM` | The **Auth limit** setting caps failed unlocks; after `2^n` failures the protected area locks **permanently**. Powerful but destructive: a low value can brick the implant. Off by default, and the app warns before you set it. |
| Protection start pointer | `AUTH0` | Left at the factory value (`0xE2`), which keeps the implant's own configuration pages (password and access bytes) behind the password while leaving Sector 0 user memory / NDEF open. |

BioVault deliberately does **not** touch the implant's one-way locks. It never
sets **`REG_LOCK`** (which would permanently freeze the configuration
registers), the static / dynamic **lock bits**, or **`NFC_DIS_SEC1`** (which
would cut Sector 1 off from NFC entirely). Avoiding those is what keeps
provisioning reversible: Unprotect restores the implant to its factory-open
state.

The xSIID ships pre-configured with `AUTH0 = 0xE2` and the well-known Dangerous
Things password (`DNGR`) guarding only its configuration pages, with Sector 1
open, which is why the vault works before provisioning. BioVault authenticates
with that factory password the first time it protects an implant, then replaces
it with the device-bound one.

## On-implant format

```
Sector 1 blob    : [magic 'BV':2][ver:1][nonce:12][ct_len:2 LE][ciphertext][tag:16]
Flipper keystore : [magic 'BVK1':4][wrap_iv:16][wrapped_dek:32]
        with PIN : [magic 'BVK2':4][salt:16][sw_iters:4][hw_iters:4][wrap_iv:16][wrapped:32]
```

(The trailing `tag:16` is the AES-GCM authentication tag, not the NFC tag.)

The blob plaintext is heatshrink-compressed before sealing. In the keystore,
`wrapped` is the data key, XORed with the PIN-derived key (when a PIN is set)
before the enclave wrap.

A keystore that exists but doesn't parse (a torn write or SD corruption) is
refused rather than re-keyed, so a glitch can't orphan the on-implant vault.

## Building

Built with [uFBT](https://github.com/flipperdevices/flipperzero-ufbt):

```sh
ufbt            # build -> dist/biovault.fap
ufbt launch     # build, upload, and run on a connected Flipper
```

## License

MIT, see [LICENSE](LICENSE).
