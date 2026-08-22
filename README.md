# BioVault (Flipper Zero)

A Flipper Zero FAP that reads and writes an **enclave-encrypted vault** on an
NTAG I2C Plus 2K NFC implant — specifically the
[xSIID](https://dangerousthings.com/product/xsiid/) (NXP NT3H2211).

This is a self-contained port of the original Proxmark3 workflow
(`biovault.py` + `hf_i2c_plus_2k_utils.lua`): no laptop, no `pm3`, no temp files
on disk. Hold the Flipper to the implant, unlock, read the vault.

## Status

**Milestone 1 (in this commit): NFC read path.**
`GET_VERSION` → `SECTOR SELECT 1` → read Sector 1 user memory → display hex.
This proves the riskiest unknown — driving the NTAG I2C Plus 2K sector-select
handshake from the Flipper's ISO14443-3A poller — before any crypto is layered on.

Built and linked against the SDK in `~/tools/flipper` (ufbt, official firmware
**1.4.3**, target f7, **API 87.1**).

### Roadmap

- [x] M1 — raw Sector 1 read + hex display (this commit)
- [ ] M2 — enclave key management (device-unique KEK in slot 11) + AES-GCM decrypt of the on-tag blob
- [ ] M3 — write path (compress → AEAD → `WRITE`/`SECTOR SELECT` to Sector 1)
- [ ] M4 — provisioning: `PWD_AUTH` + set `2K_PROT` / `AUTHLIM` (dev-tag-guarded, irreversible)
- [ ] M5 — CSV parse + on-screen vault browser

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
[magic 'BV':2][ver:1][wrapped-DEK][nonce:12][ct_len:2][ciphertext+tag]
```

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

Milestone 1 UI: **OK** reads Sector 1, **Up/Down** scroll the hex, **Back** exits.

## Credit

Ports the Proxmark3 BioVault tooling by Shain Lakin.
See the datasheet: NXP NT3H2111/NT3H2211.
