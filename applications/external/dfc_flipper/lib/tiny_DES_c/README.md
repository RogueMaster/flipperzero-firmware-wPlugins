<!--
SPDX-FileCopyrightText: Mistial Dev
SPDX-License-Identifier: Unlicense
-->

[![License: Unlicense](https://img.shields.io/badge/license-Unlicense-blue.svg)](https://unlicense.org)
[![CI](https://github.com/mistial-dev/tiny-DES-c/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/mistial-dev/tiny-DES-c/actions/workflows/ci.yml)
[![C99](https://img.shields.io/badge/C-99-00599C?logo=c&logoColor=white)](https://en.cppreference.com/w/c/language)
[![Heap-free](https://img.shields.io/badge/heap-free-informational)](#overview)
[![Bounded Contribution Policy](https://img.shields.io/badge/Contribution%20Policy-Bounded-blue)](https://github.com/OpenPhysical/BoundedContributionPolicy)

# Tiny DES / Triple-DES in C

Heap-free DES and Triple-DES (3DES / TDES) for constrained targets. This project
is a **sibling** of
[mistial-dev/tiny-AES-c](https://github.com/mistial-dev/tiny-AES-c)—same
MCU-oriented priorities (small binary, low RAM, no heap, compile-time mode
selection, fallible status codes, opt-in features) and the same general layout
and tooling. It is **inspired by**
[kokke/tiny-AES-c](https://github.com/kokke/tiny-AES-c), the original tiny,
portable block-cipher engine style that both libraries follow.

**DES is cryptographically broken for confidentiality. 3DES is legacy.** Prefer
AES ([tiny-AES-c](https://github.com/mistial-dev/tiny-AES-c)) for new designs.
This library is for interoperability and legacy protocols that still require
DES/TDEA.

## Overview

Portable C99 implementation of single DES and 2-key / 3-key Triple-DES with
compile-time mode selection. **The default build enables CTR and Triple-DES
only.** Other modes and CMAC are opt-in so unused algorithms do not contribute
code or context fields:

| Mode / feature | Default | Role |
|----------------|---------|------|
| **CTR** | on | Stream confidentiality (default) |
| **TDES** | on | 2-key / 3-key Triple-DES contexts and APIs |
| ECB | off | Single-block; insecure for multi-block use |
| CBC | off | Block chaining; no padding provided |
| OFB | off | Stream mode; confidentiality only |
| CFB1 / CFB8 / CFB64 | off | Cipher feedback (CAVP / legacy) |
| CMAC | off | NIST SP 800-38B message authentication |

Configuration uses **only** `DES_ENABLE_*` macros (no bare `CBC`/`CTR` names),
so `des.h` can co-exist with `aes.h` in one translation unit.

Design priorities: small binary size, low RAM, no heap, predictable stack, and
simple call sites suitable for bare-metal and RTOS firmware.

## Status codes

Fallible APIs return `DES_OK` (0) or `DES_ERR` (−1).

## Configuration

### Modes (Make / CMake / `-D`)

```text
make DES_ENABLE_CBC=1 DES_ENABLE_CMAC=1
cmake -S . -B build -DTINY_DES_ENABLE_CBC=ON -DTINY_DES_ENABLE_CMAC=ON
```

Direct inclusion: define `DES_ENABLE_ECB`, `DES_ENABLE_CBC`, `DES_ENABLE_CTR`,
`DES_ENABLE_OFB`, `DES_ENABLE_CFB1`, `DES_ENABLE_CFB8`, `DES_ENABLE_CFB64`,
`DES_ENABLE_TDES`, and/or `DES_ENABLE_CMAC` to `1` before including `des.h`
(or leave defaults).

### Security and size profiles

| Macro | Default | Purpose |
|-------|---------|---------|
| `DES_ZEROIZE` | **1** | Wipe stack secrets in CMAC |
| `DES_STRICT` | 0 | NULL checks on classical buffer APIs |
| `DES_CMAC_MIN_TAG_LEN` | 8 | Min CMAC tag bytes when CMAC is enabled |

## Memory footprint

Sizes measured with default CTR+TDES on a 64-bit host (`sizeof` / `size`). On
8/16-bit targets, structure layout may differ slightly; measure with your
toolchain.

| Configuration | `sizeof(DES_ctx)` | `sizeof(DES3_ctx)` | `des.o` TEXT (approx.) |
|---------------|-------------------:|--------------------:|------------------------:|
| Default CTR + TDES | 136 | 392 | ~4.9 KiB |
| All modes + CMAC | 136 | 392 | ~13 KiB |

ROM SP tables: about 2 KiB (`8 × 64 × uint32_t`).

## Public API (summary)

```c
void DES_secure_zero(void *p, size_t n);
void DES_ctx_clear(struct DES_ctx *ctx);
void DES3_ctx_clear(struct DES3_ctx *ctx); /* DES_ENABLE_TDES */

void DES_init_ctx(struct DES_ctx *ctx, const uint8_t *key);
void DES_init_ctx_iv(struct DES_ctx *ctx, const uint8_t *key, const uint8_t *iv);
void DES_ctx_set_iv(struct DES_ctx *ctx, const uint8_t *iv);

void DES_ECB_encrypt(const struct DES_ctx *ctx, uint8_t *buf); /* 8 bytes */
void DES_ECB_decrypt(const struct DES_ctx *ctx, uint8_t *buf);

int  DES_CBC_encrypt(struct DES_ctx *ctx, uint8_t *buf, size_t len);
int  DES_CBC_decrypt(struct DES_ctx *ctx, uint8_t *buf, size_t len);
int  DES_CTR_crypt(struct DES_ctx *ctx, uint8_t *buf, size_t len);
int  DES_OFB_crypt(struct DES_ctx *ctx, uint8_t *buf, size_t len);
int  DES_CFB64_encrypt(...); int DES_CFB64_decrypt(...);
int  DES_CFB8_encrypt(...);  int DES_CFB8_decrypt(...);
int  DES_CFB1_encrypt(...);  int DES_CFB1_decrypt(...); /* bit_length */

int  DES3_init_ctx(struct DES3_ctx *ctx, const uint8_t *key, size_t keylen);
int  DES3_init_ctx_iv(...);
/* DES3_* modes mirror DES_* names */

/* DES_ENABLE_CMAC */
int  DES_CMAC(const uint8_t *key, size_t keylen, const uint8_t *msg, size_t msg_len,
              uint8_t *tag, size_t tag_len);
int  DES_CMAC_verify(...);
```

C++ projects may include `des.hpp` for RAII wrappers (`tiny_des::DES`,
`tiny_des::DES3`). Freestanding or STL-free firmware should include `des.h`
only.

## Examples

### CTR with 3DES (default product shape)

```c
struct DES3_ctx ctx;
uint8_t key[24] = { /* unique 3-key material */ };
uint8_t iv[8]   = { /* unique per message under this key */ };
uint8_t buf[64];

if (DES3_init_ctx_iv(&ctx, key, sizeof key, iv) != DES_OK)
    return -1;
if (DES3_CTR_crypt(&ctx, buf, sizeof buf) != DES_OK) {
    /* counter would wrap; choose a new IV */
}
DES3_ctx_clear(&ctx);
DES_secure_zero(key, sizeof key);
```

### CBC

```c
/* length must be a multiple of 8; padding is the caller's responsibility */
if (DES_CBC_encrypt(&ctx, buf, len) != DES_OK) {
    /* misaligned length or STRICT NULL failure */
}
```

### CMAC

```c
uint8_t tag[8];
if (DES_CMAC(key, keylen, msg, msg_len, tag, sizeof tag) != DES_OK)
    return -1;
if (DES_CMAC_verify(key, keylen, msg, msg_len, tag, 8) != DES_OK)
    return -1;
```

## RTOS and concurrency

- The library has **no internal locks**.
- Do not use the same context concurrently from multiple tasks or ISRs without
  external serialization.
- Prefer one context per owner (task or connection).
- Wipe keys with `DES_ctx_clear` / `DES3_ctx_clear` / `DES_secure_zero` before
  returning buffers to pools.

## Security notes

- **DES** effective key is 56 bits; practical attacks exist. Do not use DES for
  new confidentiality requirements.
- **3DES** is deprecated for many standards; use only when mandated.
- **IV / nonce requirements** (caller responsibility):
  - **CTR / OFB / CFB:** never reuse a nonce/IV with the same key.
  - **CBC encryption:** the IV must be **unpredictable** (typically random).
- **No padding** is provided. CBC lengths must be multiples of 8 bytes.
- CBC rejects non-aligned lengths with `DES_ERR`. CTR rejects requests that
  would wrap the 64-bit counter (buffer and IV left unchanged).
- CFB-64 allows a shorter final segment (SP 800-38A).
- Weak keys and parity bits are **not** rejected; callers must apply policy.
- Tables and SP lookups are **not** constant-time side-channel hardened.
- CMAC tags shorter than 8 bytes require risk analysis (`DES_CMAC_MIN_TAG_LEN`).

## Residual risks (not fixed by this library)

| Issue | Reason |
|-------|--------|
| DES/3DES cryptographic weakness | Algorithm limits |
| True constant-time on all platforms | Compiler and microarchitecture dependent |
| Nonce uniqueness | Application protocol |
| Padding schemes | Out of scope |
| Power / EM / residual cache leakage | Beyond portable C tables |
| Replay protection, KDF, RNG | Out of scope |
| Wipe vs aggressive LTO | Best-effort `volatile` stores |

## Build and test

```text
make                    # des.o with default CTR+TDES
make size
make test               # full-mode unit tests + sparse configs
make DES_CAVP=1 test    # full vendored TDES CAVP corpora
make benchmark
```

CMake:

```text
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Unit tests use vendored µunit. Vectors under `test_vectors/` include NIST CAVP
TDES response files (opt-in), edge-case JSON (Wycheproof-style), and OpenSSL-
cross-checked CMAC KATs.

To include in another CMake project:

```cmake
add_subdirectory(path/to/tiny-DES-c)
target_link_libraries(your_target PRIVATE tiny-des-c)
```

## Contributing

This project uses the [Bounded Contribution Policy](CODE_OF_CONDUCT.md) to keep
discussions and contributions focused on technical goals. See
[CONTRIBUTING.md](CONTRIBUTING.md) for build/test expectations and PR guidance.

## Credits

- **Sibling project:** [mistial-dev/tiny-AES-c](https://github.com/mistial-dev/tiny-AES-c)
  (API shape, defaults philosophy, test/CAVP style, MCU docs).
- **Inspiration:** [kokke/tiny-AES-c](https://github.com/kokke/tiny-AES-c)—the
  original small, zero-allocation, single-file AES implementation model.
- Unit testing framework: [µunit (munit)](https://nemequ.github.io/munit/).
- Contribution policy:
  [OpenPhysical BoundedContributionPolicy](https://github.com/OpenPhysical/BoundedContributionPolicy).

## License

This software is released into the **Public Domain** under the terms of
[The Unlicense](LICENSE). Vendored µunit: MIT.
