# Vault file format and key derivation

This document describes `vault.vfv`, the encrypted file the companion writes to the Flipper (`/ext/apps_data/vaultflipper/vault.vfv`), and the small `clock` file next to it. The reference implementations are [`vf_core.c`](../flipper/vaultflipper/vf_core.c) (Flipper, Monocypher) and [`vfformat.py`](../companion/src/vaultflipper/vfformat.py) (companion, libsodium). The test suite checks that both agree.

The vault is split into a small **directory** and many **pages**, each encrypted on its own. Unlocking decrypts only the directory; a page – one folder or part of one, at most 48 logins – is decrypted when it is opened. RAM use on the Flipper therefore stays the same for 10 or 5,000 logins.

## Device key

```
device_secret = AES-256-CBC(enclave unique key slot 11, IV = "VaultFlipper-iv1", "VaultFlipper device secret v1...")
dev_sk        = BLAKE2b-256(key = device_secret, message = "VaultFlipper x25519 v1")
dev_pk        = X25519(dev_sk, basepoint)
```

The enclave key never leaves the STM32WB55 co-processor. The app writes `dev_pk` as hex to `device.pub`; `vaultflipper pair` reads it once. The fingerprint shown on both sides is the first 4 bytes of `BLAKE2b-256(dev_pk)` as `XXXX-XXXX`.

## File key

```
eph_sk, eph_pk = fresh X25519 key pair per sync (companion)
shared         = X25519(eph_sk, dev_pk) = X25519(dev_sk, eph_pk)
pin_key        = Argon2id(password = PIN, salt, memory = blocks KiB, passes, lanes = 1, length = 32)
key            = BLAKE2b-256(key = shared, message = pin_key)
```

The PIN is the ASCII string of the arrow presses, for example `UUDDLRLRUDLR`. An all-zero `shared` value is rejected.

## Layout

All integers are little endian.

### Header (72 bytes)

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | magic `VFLP` |
| 4 | 1 | version, `2` |
| 5 | 3 | reserved, zero |
| 8 | 4 | Argon2id memory in KiB (8–64, multiple of 4; default 32) |
| 12 | 4 | Argon2id passes (1–1000; default 64) |
| 16 | 16 | salt |
| 32 | 32 | `eph_pk` |
| 64 | 4 | directory ciphertext length (≤ 24 KiB) |
| 68 | 4 | reserved, zero |

### Blocks

```
block = nonce (24) | Poly1305 tag (16) | ciphertext
AAD   = header (72 bytes) | u32 block number
```

XChaCha20-Poly1305 (IETF variant) with a random nonce per block. The directory is block 0 and starts right after the header; page *i* is block *i + 1*. Binding the header and the block number into the AAD means parameters cannot be changed and pages cannot be swapped or replayed from another file.

### Records

Both directory and pages use the same record encoding:

```
record: u8 field_count, then per field: u8 tag, u16 length, bytes[length], 0x00
```

| Tag | Field |
| ---: | --- |
| 1 | account name |
| 2 | group (folder, or `Organization/Collection`) |
| 3 | entry name |
| 4 | username |
| 5 | password |
| 6 | TOTP secret, see [2FA codes](#2fa-codes) |
| 7 | first URI |
| 16 | metadata: default keyboard layout |
| 17 | metadata: sync time |
| 32 | first entry name on a page |
| 33 | last entry name on a page |

Unknown tags are skipped.

### Directory (block 0)

```
"VFD2"
record                       metadata (16, 17)
u8  account_count            1–16
    record × account_count   account name (1)
u16 page_count               1–1024
    per page: u8 account, u32 offset, u32 length, u16 entry_count,
              record (2, 32, 33)
```

`account` is an index into the account list, or `255` for a page of the [2FA list](#2fa-codes).

The directory may be at most 24 KiB. Tags 32 and 33 (first and last name on the page) only label pages in the folder list, so the companion stores at most 12 bytes of each.

### Page (blocks 1…n)

```
"VFG2"  u16 entry_count  record × entry_count   (3, 4, 5, 6, 7)
```

A page holds up to 48 logins of one account and one group and at most 8 KiB of plaintext; a single login may use up to 4 KiB.

## 2FA codes

**Secrets.** Tag 6 holds the TOTP secret as `ALGO:DIGITS:PERIOD:SECRET`, for example `SHA1:6:30:JBSWY3DPEHPK3PXP`. `ALGO` is `SHA1`, `SHA256`, `SHA512` or `STEAM` (HMAC-SHA1, 5 characters from `23456789BCDFGHJKMNPQRTVWXY`), `DIGITS` is 1–10, `PERIOD` 1–3600 seconds, `SECRET` is base32 without padding (at most 64 bytes decoded). A bare base32 secret means `SHA1:6:30`. The companion converts Bitwarden's formats – bare secrets, `otpauth://totp/…` URIs and `steam://…` – and leaves out counter-based (HOTP) secrets. Codes follow RFC 6238; the reference is [`vf_totp.c`](../flipper/vaultflipper/vf_totp.c).

**The 2FA list.** After the regular pages come pages with `account = 255`: name (3), username (4) and secret (6) of every login that has a TOTP secret, across all accounts, sorted by name. They carry no password or URI. The app lists them under *2FA codes* without decrypting any other page.

**Clock.** The Flipper keeps local time, set by qFlipper or the mobile app. At every sync the companion compares it with UTC and writes the difference in seconds as text to `/ext/apps_data/vaultflipper/clock`, for example `3599` for UTC+1 with the clock one second behind. The app computes codes for `Flipper time − offset`; *Settings ▸ Time zone* changes the offset in whole hours, keeping the extra seconds. The file is not secret and not encrypted: changing it only produces wrong codes.
