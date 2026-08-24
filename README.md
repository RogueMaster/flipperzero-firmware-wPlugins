# BioVault

BioVault keeps a password vault **encrypted on an NFC implant**,
unlockable only by the Flipper that owns it.

The encrypted vault lives on the implant. The key that decrypts it lives only in
the Flipper's secure enclave and never leaves it. You need **both** the implant
and its Flipper to read anything with possession of either one on its own revealing nothing. 
Read the implant using the Flipper, load the vault, and browse, edit, or type your
credentials straight into a computer over USB. No external tooling, no `proxmark`,
everything in RAM.

## How it works

BioVault splits your secrets across two things you hold:

- **The implant** stores the vault as one AES-256-GCM–encrypted blob in Sector 1
  of the tag. On its own it is just ciphertext.
- **The Flipper** holds the decryption key, wrapped by a device-unique key that
  is generated inside the STM32WB55 secure enclave and can never be read back
  out. On its own it has nothing to decrypt.

When you Load, the Flipper reads the blob from the implant, decrypts it in RAM,
and shows the vault. Edits stay in RAM until you Save them back to the implant.
Nothing is ever written to the Flipper's storage except the wrapped key.

## Hardware

- **Dangerous Things xSIID** implant, or any **NTAG I2C Plus 2K** tag
  (NXP NT3H2211).
- The vault occupies **Sector 1** only. Sector 0 (UID, NDEF records) is left
  untouched, so the implant still works as a normal NFC tag alongside the vault.

## Features

- Read, write, and browse an encrypted credential vault on the implant.
- Add, view, edit, and remove credential and note entries on-device, with an
  extended keyboard for symbols.
- **Send** a username, password, or note to a computer over USB-HID — the
  Flipper types it as if it were a keyboard — with an optional trailing Return.
- **Optional tag password protection** (opt-in): device-bound, per-tag
  protection of Sector 1 so the raw ciphertext can't even be read off the tag by
  another reader. Selectable read-protection and a failed-attempt limit. Fully
  reversible.
- **Optional vault PIN / passphrase**: the PIN is stretched into key material
  that wraps the vault key, so it can't be bypassed by patching the app — an
  attacker with your Flipper must brute-force it through this device's secure
  enclave, one slow guess at a time.
- A **`biovault` USB serial CLI** that mirrors every on-device command, so you
  can manage the vault and copy passwords from a terminal.

## Usage

The app opens on **Load from Implant**; hold the implant to the Flipper, then it
drops into the main menu:

- **Vault** — browse entries; open one to **View**, **Send** (USB-HID),
  **Edit**, or **Remove** it.
- **Add Entry** — add a credential (label + username + password) or a note
  (label + text).
- **Load / Read / Save / Wipe Implant** — sync the vault to and from the tag.
  Load first, so Save never overwrites the tag with an unsynced vault.
- **Settings** — USB auto-return, opt-in tag password protection, and an
  optional vault PIN.
- **About** and **Diagnostics** (crypto self-test results).

Edits are held in RAM until you **Save** them back to the implant.

### CLI

Connect over USB serial, run `biovault` to open a subshell, and use:

```
Vault:    list, get <label>, add <label>, edit <label>, remove <label>
Device:   read, load, save, wipe, reveal
Protect:  protect, unprotect        (confirmed on the Flipper)
Config:   settings [<key> <value>], pin <set|remove|enter>
```

`reveal` prints the tag's password to the terminal (handy for `pm3`), but only
after the provisioned implant authenticates — the same two-factor gate as the
on-device reveal.

## Security model

The design goal is that **your data stays safe even if you lose the Flipper**,
because the vault is never stored on the Flipper — only on the implant.

- **Lost or stolen Flipper (even unlocked):** the finder has the *key* but not
  your *data*. There is no vault on the Flipper to decrypt; the ciphertext is on
  the implant, which you still have. Nothing is exposed. The cost is to you: the
  enclave key is gone, so your vault becomes **unrecoverable**. This is a
  recoverability loss, not a confidentiality loss.
- **Someone reads your implant over NFC:** they get an AES-256-GCM blob that is
  useless without your Flipper's enclave key — and the key can't be cloned to
  another device. With tag password protection enabled, they can't even read the
  raw blob.
- **The only real exposure** is an adversary who holds *both* your Flipper *and*
  NFC range to your implant at the same time and unlocks it — which is inherent
  to any scheme where both factors are present together (that combination is
  exactly what you use to read your own vault).

Other properties:

- **No escrow, no recovery, by design.** The enclave key is device-bound and
  generated on-device by the hardware TRNG. If the Flipper is lost, wiped, or
  has its coprocessor firmware reflashed, the key is gone and the vault with it.
  There is no backup.
- **Authenticated encryption.** The vault is sealed with AES-256-GCM
  (hardware-accelerated), so tampering with the on-tag blob is detected rather
  than silently decrypted.
- **Tag protection is device-bound and per-tag.** When enabled, the tag password
  is derived from the enclave key and the tag's UID, is never stored, and is
  shown only after the provisioned implant authenticates. A configurable
  failed-attempt limit can permanently lock the tag — the app warns before you
  set it.

### Vault PIN (optional)

Settings → **Vault PIN** (or `pin set` in the CLI) wraps the vault key under a
PIN or passphrase. This is **key material, not a check**: there is no PIN
verification anywhere in the code to patch out, and no verifier stored on the
Flipper — a stolen Flipper alone gives an attacker *nothing to even test PIN
guesses against*. If they also obtain the implant's ciphertext, every guess
must still run a multi-second key-stretching chain through this Flipper's
secure enclave (PBKDF2 plus ~16k enclave AES iterations, ~3 s per guess), which
cannot be offloaded to faster hardware: a 6-digit PIN costs weeks on-device; a
passphrase is out of reach entirely.

The trade-offs of having no verifier, by design:

- **A wrong PIN cannot be detected at entry.** It just derives the wrong key —
  loads then fail authentication ("wrong PIN?"). The PIN is entered twice at
  each unlock to guard against typos.
- **With read-protection enabled, a wrong-PIN session presents a wrong tag
  password**, which burns one of the tag's failed-auth attempts per load. Keep
  the auth limit high (or off) when using a PIN.
- **A forgotten PIN loses the vault.** There is no reset that doesn't destroy
  the security. Removing or changing the PIN is only allowed after a
  GCM-verified load proves the current session's PIN, so a typo'd session can
  never rewrap the key.

The Flipper is not a hardened secure element, and it does not need to be here:
it never holds the plaintext vault at rest (only transiently in RAM, while the
implant is coupled, during an unlock). The enclave and authenticated encryption
raise the bar; keeping the data off the Flipper is what makes a lost device a
non-event for confidentiality.

## Tag protection (chip-side)

By default the vault is protected by encryption alone — the on-tag blob is
AES-256-GCM ciphertext, useless without your Flipper. On top of that you can
optionally enable the NTAG I2C Plus's **own hardware protection**, so the tag
itself refuses access to Sector 1 without the password. It is opt-in from
**Settings → Protect implant** and fully reversible (**Unprotect implant**).

The NT3H2211 offers several protection mechanisms. BioVault uses them like this:

- **Password authentication (`PWD_AUTH`).** A 32-bit password with a 16-bit
  acknowledge (`PACK`) gates the protected memory. BioVault derives both from the
  enclave key and the tag UID — device-bound, unique per tag, never stored — and
  verifies the `PACK` the tag returns on every unlock, so a cloned or emulated
  tag that merely spoofs the UID still can't authenticate.
- **Sector 1 protection (`2K_PROT`).** Set to gate all of Sector 1 (the vault)
  behind the password. This is the core of the feature.
- **Read vs write protection (`NFC_PROT`).** The **Read protect** setting chooses
  whether the password guards **writes only** (the ciphertext stays readable but
  can't be altered or cloned) or **reads and writes** (the ciphertext can't even
  be read off the tag).
- **Failed-attempt lockout (`AUTHLIM`).** The **Auth limit** setting caps failed
  unlocks; after `2^n` failures the protected area locks **permanently**. It is
  powerful but destructive — a low value can brick the tag — so the app warns
  before you set it, and it defaults to off.
- **Protection start pointer (`AUTH0`).** Left at the tag's factory value
  (`0xE2`), which keeps the tag's own configuration pages (password and access
  bytes) behind the password, while leaving Sector 0 user memory / NDEF open.

BioVault deliberately does **not** touch the tag's one-way locks. It never sets
**`REG_LOCK`** (which would permanently freeze the configuration registers), the
static / dynamic **lock bits**, or **`NFC_DIS_SEC1`** (which would cut Sector 1
off from NFC entirely). Avoiding those is what keeps provisioning reversible —
Unprotect always restores the tag to its factory-open state.

The xSIID ships pre-configured with `AUTH0 = 0xE2` and the well-known Dangerous
Things password (`DNGR`) guarding only its configuration pages, with Sector 1
open — which is why the vault works before provisioning. BioVault authenticates
with that factory password the first time it protects a tag, then replaces it
with the device-bound one.

## On-tag format

```
Sector 1 blob : [magic 'BV':2][ver:1][nonce:12][ct_len:2 LE][ciphertext][tag:16]
Flipper key   : [magic 'BVK1':4][wrap_iv:16][wrapped_dek:32]   (on the Flipper, not the tag)
```

The wrapped data key lives in the Flipper's app-data storage, never on the
implant. A keystore that exists but doesn't parse (a torn write or SD
corruption) is refused rather than silently re-keyed, so a glitch can't orphan
the on-tag vault.

## Building

Built with [uFBT](https://github.com/flipperdevices/flipperzero-ufbt):

```sh
ufbt            # build -> dist/biovault.fap
ufbt launch     # build, upload, and run on a connected Flipper
```

## License

MIT — see [LICENSE](LICENSE).
