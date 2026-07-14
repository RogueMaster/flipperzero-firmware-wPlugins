# Xiaomi air purifier filter DRM — protocol notes

This document describes how Xiaomi air purifiers track "filter life" on the filter's
NFC tag, and exactly what this app does to reset it. Everything here was verified
against real filter tag dumps (see [Credits](#credits-and-sources)).

## The tag

Each filter carries an **NTAG213** (NXP), an ISO 14443-3A / NfcA tag with 45 pages of
4 bytes each (180 bytes total). The appliance reads and writes it over NFC to enforce
a usage limit.

## Memory map

| Page(s) | Field | Notes |
| --- | --- | --- |
| 0–1 | UID | 7-byte UID (`page0[0..2]` + `page1[0..3]`); `page0[3]` is BCC0. Always readable. |
| 2 | Lock bytes / internal | |
| 3 | Capability container | |
| 4 | RFID factory id | ASCII; first half of the product code (e.g. `…AP`). |
| 5 | RFID product id | ASCII; second half of the product code (e.g. `…11`). |
| 6 | RFID timestamp | Manufacturing time. |
| 7 | RFID serial number | Unique per tag. |
| **8** | **Usage counter** | **little-endian `uint32`. Writing `00 00 00 00` = 100% life.** |
| 9–39 | Unused | Zero. |
| 40 | Dynamic lock + RFUI | `00 00 00 BD`. |
| 41 | `MIRROR / AUTH0` config | `04 00 00 04` → **AUTH0 = 0x04**: protection starts at page 4. |
| 42 | `ACCESS` config | `C0 05 00 00` → **PROT = 1**: password required for **read and write**. |
| 43 | `PWD` | The 4-byte NTAG password (see below). |
| 44 | `PACK` + RFUI | PACK = `00 00`. |

Because `AUTH0 = 4` and `PROT = 1`, pages 4 and up — including the counter at page 8
— cannot even be **read** without authenticating first. The UID (pages 0–1) is always
readable, which is all the app needs to derive the password.

### The usage counter

Page 8 holds a little-endian `uint32` that counts **up** with filter use; a fresh
filter reads `0` (100%). Two dumps of the *same* filter at different wear levels
confirm it is the only page that changes:

| Filter state | Page 8 bytes | Decoded (LE) |
| --- | --- | --- |
| 99% life left | `DA 67 01 00` | 92 122 |
| 98% life left | `A4 29 02 00` | 141 732 |
| 100% (fresh) | `00 00 00 00` | 0 |

More life left ⇒ lower counter, exactly as expected for an "elapsed usage" value.

## Password algorithm

The 4-byte NTAG password is a deterministic function of the tag's 7-byte UID:

```python
import hashlib

def getpwd(uid_hex):
    uid = bytearray.fromhex(uid_hex)
    h = bytearray(hashlib.sha1(uid).digest())   # 20 bytes
    return "".join("%02X" % h[(h[0] + off) % 20] for off in (0, 5, 13, 17))
```

That is: hash the raw UID with SHA-1, then pick four bytes from the digest, indexed by
the value of the first digest byte plus the fixed offsets `0, 5, 13, 17` (mod 20).

The C implementation in [`src/core/xiaomi_filter.c`](../src/core/xiaomi_filter.c) with
the bundled SHA-1 in [`src/core/sha1.c`](../src/core/sha1.c) reproduces this exactly.

### Golden vectors (verified against real hardware)

The password is stored on the tag itself at page 43, so a tag dump lets us check the
algorithm against ground truth. All of these are asserted in the host tests:

| UID | Password | Source |
| --- | --- | --- |
| `04A03CAA1E7080` | `CD91AFCC` | vendor assert + tag dump |
| `04112233445566` | `EC9805C8` | vendor assert |
| `0404D972BA6C81` | `103AF4A2` | tag dump (page 43) |
| `047FD6E2EA6C80` | `7D295EE7` | tag dump (page 43) |
| `04FA3CAA1E7080` | `95BF2720` | tag dump (page 43) |

PACK (page 44) is `00 00` on every observed tag.

## Reset transaction

Captured from a real reset (NFC sniff), and what this app performs:

```
REQA / anticollision / SELECT        -> obtain UID  (e.g. 04 04 D9 72 BA 6C 81)
1B 10 3A F4 A2                        PWD_AUTH with the derived password
   <- 00 00                           PACK (auth OK)
30 08                                 READ page 8 (current counter)   [optional]
A2 08 00 00 00 00                     WRITE page 8 = zeros            (the reset)
30 08                                 READ page 8 -> 00 00 00 00      (verify)
```

- `0x1B` = `PWD_AUTH`, followed by the 4 password bytes; the tag answers with the
  2-byte PACK.
- `0x30` = `READ` (returns 4 pages starting at the given page).
- `0xA2` = `WRITE` (one page).

The app never issues the write unless `PWD_AUTH` succeeds, so a non-Xiaomi tag (whose
UID-derived password won't match) is left untouched.

## Product code

The factory-id (page 4) and product-id (page 5) pages each contribute two printable
ASCII bytes, forming a short product code shown in the app:

| Filter family | Page 4 | Page 5 | Code |
| --- | --- | --- | --- |
| A | `00 00 41 50` | `00 00 31 31` | `AP11` |
| B | `00 00 4A 44` | `00 00 41 30` | `JDA0` |

This is informational only; the reset does not depend on it.

## Credits and sources

- flamingo-tech, *Xiaomi air purifier reverse engineering*:
  <https://github.com/Flamingo-tech/xiaomi-air-purifier-reverse-engineering>
  (tag dumps, memory map, password algorithm).
- unethical.info, *Breaking Free from DRM: The Story of Hacking My Air Purifier*:
  <https://unethical.info/2024/01/24/hacking-my-air-purifier/>.
