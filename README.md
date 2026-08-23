# BioVault (Flipper Zero)

A Flipper Zero FAP that reads and writes an **enclave-encrypted vault** on an
NTAG I2C Plus 2K NFC implant — specifically the
[xSIID](https://dangerousthings.com/product/xsiid/) (NXP NT3H2211).

This is a self-contained port of the original Proxmark3 workflow
(`biovault.py` + `hf_i2c_plus_2k_utils.lua`): no laptop, no `pm3`, no temp files
on disk. Hold the Flipper to the implant, unlock, read the vault.

## Status

**Milestone 1: NFC read path — verified on hardware.**
`activate` (anti-collision + SELECT) → `GET_VERSION` → two-part `SECTOR SELECT 1`
→ read Sector 1 user memory → display hex. This proved the riskiest unknown —
driving the NTAG I2C Plus 2K sector-select handshake from the Flipper's
ISO14443-3A poller. Confirmed against a real xSIID implant (UID `0478A5D2CD5280`):
`GET_VERSION` matches `0004040502021503`, sector-select packet 1 returns the
4-bit ACK `0x0A`, packet 2 is a passive-ACK (poller timeout = success), and
Sector 1 reads back distinct from Sector 0 (page 0 zeroed, data from page 4 —
the legacy `biovault.py` layout).

Built and linked against the SDK in `~/tools/flipper` (ufbt, official firmware
**1.4.3**, target f7, **API 87.1**).

Two non-obvious things this milestone nailed down for the ISO14443-3A poller:
- Started with `nfc_poller_start_ex` on the base `iso14443_3a` protocol, the
  callback receives raw `NfcEvent`s; the card-ready event is
  `NfcEventTypePollerReady`, not `Iso14443_3aPollerEventTypeReady`.
- `PollerReady` only means a card answered the initial request — you must call
  `iso14443_3a_poller_activate()` to SELECT it into ACTIVE state, or every
  application command (`READ`, `GET_VERSION`) times out.

### Roadmap

- [x] M1 — raw Sector 1 read + hex display (verified on hardware)
- [x] M2 — enclave key management (device-unique KEK in slot 11) + AES-GCM decrypt of the on-tag blob (verified)
- [x] M3 — write path (compress → AEAD → `WRITE`/`SECTOR SELECT` to Sector 1); save + load round-trip verified on hardware
- [ ] M4 — provisioning: `PWD_AUTH` + set `2K_PROT` / `AUTHLIM` (dev-tag-guarded, irreversible)
- [x] M5 — on-screen vault browser (scrollable detail view + extended keyboard); CSV parse dropped — the vault uses a length-prefixed binary format, not CSV
- [x] CLI — `biovault list/get/add/edit/remove` + interactive subshell over the vault; `read` prints a hex+ASCII Sector 1 dump to the terminal
- [x] USB-HID send — type an entry's username / password / data into a host as keystrokes (per-entry **Send** action), optional auto-Enter after credentials
- [x] Settings — persisted on-device preferences (`bv_settings`), starting with the auto-Enter toggle
- [x] Reliability hardening — bounded read/write retries (no infinite silent loop), corrupt-keystore re-key guard, header-page-last save ordering, subshell-safe teardown

Everyday use is feature-complete; **M4 (provisioning)** is the only open milestone,
and the riskiest — it writes irreversible password/lock config to the tag.

## Design

### Why the Flipper

The Flipper's **ST25R3916** NFC frontend speaks ISO14443-3A, and the
**STM32WB55** MCU provides a hardware AES engine, a TRNG, and a secure key
store ("enclave"). That's the whole stack the vault needs, on one device.

### Key management — enclave only, no recovery (deliberate)

The vault key never exists as a recoverable secret off the device:

- A **device-unique key** lives in the STM32WB55 secure enclave (slot 11,
  `FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT`). It is generated on-device by the
  TRNG on first use (`furi_hal_crypto_enclave_ensure_key`), and Flipper/NXP
  never see it. Code can *use* it via the AES engine but cannot *read* it back.
- This enclave key is used as a **key-encryption key (KEK)**. The actual data
  key (DEK) that drives AES-GCM is wrapped by the KEK and stored alongside the
  ciphertext; it exists in RAM only transiently during an unlock.

**Consequence, accepted by design:** if this Flipper is lost, bricked, wiped, or
has its coprocessor firmware (FUS) reflashed, the enclave key is gone and the
implant ciphertext is **permanently unrecoverable**. There is no escrow and no
backup. That is the intended property — the vault is bound to this one device.

### Why enclave *and* a passphrase/PIN (later milestone)

The enclave protects the key **at rest and against extraction/cloning** — a
flash/SD dump yields nothing, and a wrapped blob won't decrypt on another
Flipper. It does **not** stop someone holding the unlocked device from simply
running an app that decrypts and displays. So the enclave is *complementary to*,
not a *substitute for*, a user secret. M2 mixes a short PIN into the KEK-unwrap
step so a stolen Flipper alone is not enough.

Flipper's own `furi_hal_crypto.h` is blunt: *"Flipper was never designed to be
secure… it can be easily dumped with a debugger or modified code."* The enclave
raises the bar; the AES-GCM + auth-tag layer is what carries real confidentiality.

### Authenticated encryption

The original used `openssl aes-256-cbc` (unauthenticated). This port uses
**AES-GCM** via `furi_hal_crypto_gcm_encrypt_and_tag` /
`furi_hal_crypto_gcm_decrypt_and_verify` (hardware, authenticated). The on-tag
blob is length-prefixed so there is no fragile carve on runs of null bytes:

```
on tag  : [magic 'BV':2][ver:1][nonce:12][ct_len:2 LE][ciphertext:ct_len][tag:16]
keystore: [magic 'BVK1':4][wrap_iv:16][wrapped_dek:32]   (Flipper-local, not on tag)
```

The wrapped DEK is **not** written to the implant — it lives in the Flipper's
own app-data keystore (`/ext/apps_data/biovault/keystore.bin`), so a rotation or
future PIN change never requires a tag write. A keystore that exists but doesn't
parse (torn write, SD corruption) is treated as an error and refused, never
silently re-keyed — re-keying would orphan the on-tag ciphertext forever.

### Interrupted-save safety

Sector 1 is written header-page-**last**: page 0 (magic + nonce + length) is
blanked first, the ciphertext pages go down, and the real header is written only
once everything else is on the tag. A save torn mid-write (implant pulled away)
therefore reads back on the next Load as an empty/foreign tag — never as a valid
header framing stale ciphertext that would fail GCM auth with nothing to fall
back to. The continuous poll-retry loop also usually just re-completes the write
while the implant is still coupled.

### USB-HID send

Any entry can be typed into a host over USB as if from a keyboard: **Vault → an
entry → Send (USB)**, then pick Username / Password (or Data for a note). The
Flipper temporarily switches its USB device from CDC (the serial CLI) to HID,
waits for the host to re-enumerate and its HID driver to settle, types the field
via the ASCII→keycode map, then restores the previous USB mode. The typed field
is snapshotted under the vault lock and wiped from RAM as soon as the send
completes; it runs on a short worker thread so the UI stays responsive. With the
**Auto-Enter** setting on, a Return is appended after a username/password (notes
are left alone so a data dump isn't auto-submitted).

### Settings

`bv_settings` persists on-device preferences to
`/ext/apps_data/biovault/settings.bin` (magic-prefixed, defaults-on-missing, so
new fields are backward-compatible). Currently one toggle — **Auto-Enter** — with
room for the M4/PIN preferences to slot in as new `VariableItemList` rows.

### The NFC handshake gotcha

`SECTOR SELECT` is two frames. Packet 1 (`C2 FF`) returns a 4-bit ACK.
Packet 2 (`<sector> 00 00 00`) returns a **passive ACK** — no response at all —
so a poller **timeout there means success**. `bv_select_sector()` handles this;
the exact frame-wait-time may need tuning against a real tag (see the NOTE at
the top of `biovault.c`).

## Build & run

```sh
source ~/tools/flipper/env.sh
cd biovault-flipper
ufbt              # build -> dist/biovault.fap
ufbt launch       # build, upload, and run on a connected Flipper
```

The app opens on **Load from Implant** (so Save never overwrites a tag with an
un-synced vault), then drops into a menu: **Vault** (browse / view / send / edit
/ remove entries), **Add Entry**, **Load** / **Read** / **Save** / **Wipe
Implant**, **Settings**, and **Diagnostics** (crypto self-test results).

## Credit

Ports the Proxmark3 BioVault tooling by Shain Lakin.
See the datasheet: NXP NT3H2111/NT3H2211.
