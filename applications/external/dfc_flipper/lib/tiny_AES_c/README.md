<!--
SPDX-FileCopyrightText: kokke
SPDX-FileCopyrightText: Mistial Dev
SPDX-License-Identifier: Unlicense
-->

[![License: Unlicense](https://img.shields.io/badge/license-Unlicense-blue.svg)](https://unlicense.org)
[![CI](https://github.com/mistial-dev/tiny-AES-c/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/mistial-dev/tiny-AES-c/actions/workflows/ci.yml)
[![C99](https://img.shields.io/badge/C-99-00599C?logo=c&logoColor=white)](https://en.cppreference.com/w/c/language)
[![Heap-free](https://img.shields.io/badge/heap-free-informational)](#overview)
[![Bounded Contribution Policy](https://img.shields.io/badge/Contribution%20Policy-Bounded-blue)](https://github.com/OpenPhysical/BoundedContributionPolicy)

# Tiny AES in C

Heap-free AES for constrained targets (AVR-class MCUs and similar). This
repository is a fork of [kokke/tiny-AES-c](https://github.com/kokke/tiny-AES-c).
Fork-specific changes are listed in [CHANGELOG.md](CHANGELOG.md).

## Overview

Portable C99 implementation of AES-128/192/256 with compile-time mode
selection. **The default build enables CTR only.** All other modes are opt-in
so unused algorithms do not contribute code or context fields:

| Mode | Default | Role |
|------|---------|------|
| **CTR** | on | Confidentiality stream mode |
| ECB | off | Single-block; insecure for most uses |
| CBC | off | Block chaining; no padding provided |
| OFB | off | Stream mode; confidentiality only |
| CCM | off | One-shot AEAD (packet mode) |
| EAX | off | One-shot AEAD |
| EAX′ | off | ANSI C12.22 authenticated encryption |
| GCM | off | Streaming AEAD plus one-shot helpers |
| SIV | off | Deterministic / nonce-misuse-resistant AEAD (RFC 5297) |
| CMAC | off | Message authentication (NIST SP 800-38B) |

Key size is fixed at compile time: define `AES192` or `AES256`, otherwise
AES-128 is selected.

Design priorities: small binary size, low RAM, no heap, predictable stack, and
simple call sites suitable for bare-metal and RTOS firmware. This is not a
high-throughput server library.

## Status codes

Fallible APIs return `AES_OK` (0) or `AES_ERR` (−1).

## Configuration

### Modes (Make / CMake / `-D`)

```text
make AES_ENABLE_CBC=1 AES_ENABLE_GCM=1
cmake -S . -B build -DTINY_AES_ENABLE_CBC=ON -DTINY_AES_ENABLE_GCM=ON
```

Direct inclusion: define `AES_ENABLE_CBC`, `AES_ENABLE_ECB`, `AES_ENABLE_CTR`, `AES_ENABLE_OFB`, `AES_ENABLE_GCM`, `AES_ENABLE_CCM`, `AES_ENABLE_EAX`, `AES_ENABLE_EAX_PRIME`, `AES_ENABLE_SIV`, and/or `AES_ENABLE_CMAC` to `1` before including `aes.h`.

### Security and size profiles

| Macro | Default | Purpose |
|-------|---------|---------|
| `AES_SBOX_MODE` | constant-time scan | `secure` / `runtime` / `fast` (Make names) |
| `AES_WIDE_OPS` | 0 | Portable wider block helpers when 1 |
| `AES_ZEROIZE` | **1** | Wipe stack secrets in one-shot paths |
| `AES_STRICT` | 0 | NULL checks on classical buffer APIs |
| `AES_TINY` | 0 | `#error` if GHASH is table4/fast-table |
| `AES_EAX_MIN_TAG_LEN` | 8 | Minimum EAX tag length |
| `AES_CMAC_MIN_TAG_LEN` | 8 | Min CMAC tag bytes (product); CAVP tests use 4 for full corpus |
| `AES_GCM_GHASH_MODE` | auto | bitwise / wide / table4 / fast-table / hardware |

Make examples:

```text
make AES_SBOX_MODE=secure AES_WIDE_OPS=off AES_ZEROIZE=1
make AES_ENABLE_GCM=1 AES_TINY=1 AES_GCM_GHASH_MODE=bitwise
make AES_ENABLE_GCM=1 AES_GCM_GHASH_MODE=table4
```

## Memory footprint

Sizes measured with AES-128 on a 64-bit host (`sizeof`). On 8/16-bit targets,
structure layout may differ slightly; measure with your toolchain.

| Configuration | `sizeof(AES_ctx)` | `sizeof(AES_GCM_ctx)` |
|---------------|-------------------|------------------------|
| CTR only | 192 | — |
| No IV modes (key schedule only) | 176 | — |
| GCM, auto/bitwise GHASH | 176 | **312** |
| GCM, table4 (per-context table) | 176 | **~8504** |

`AES_TINY=1` rejects table4/fast-table at compile time. Prefer bitwise/auto GHASH
on small devices; table modes always store ~8 KiB **per context**.

One-shot CCM/EAX/EAX′ keep a packed workspace on the stack (one `AES_ctx` plus
a few 16-byte blocks). There are no VLAs and no recursive paths.

ROM S-box: 256 bytes (plus inverse S-box when ECB/CBC/CAVP need decrypt).
Runtime S-box mode uses 256 bytes of RAM instead of ROM for the forward table.

## Public API (summary)

```c
void AES_secure_zero(void *p, size_t n);
void AES_ctx_clear(struct AES_ctx *ctx);

void AES_init_ctx(struct AES_ctx *ctx, const uint8_t *key);
void AES_init_ctx_iv(struct AES_ctx *ctx, const uint8_t *key, const uint8_t *iv);
void AES_ctx_set_iv(struct AES_ctx *ctx, const uint8_t *iv);

void AES_ECB_encrypt(const struct AES_ctx *ctx, uint8_t *buf); /* 16 bytes */
void AES_ECB_decrypt(const struct AES_ctx *ctx, uint8_t *buf);

int  AES_CBC_encrypt(struct AES_ctx *ctx, uint8_t *buf, size_t len);
int  AES_CBC_decrypt(struct AES_ctx *ctx, uint8_t *buf, size_t len);

int  AES_CTR_crypt(struct AES_ctx *ctx, uint8_t *buf, size_t len);
int  AES_OFB_crypt(struct AES_ctx *ctx, uint8_t *buf, size_t len);

int  AES_GCM_init(struct AES_GCM_ctx *ctx, const uint8_t *key,
                  const uint8_t *iv, size_t iv_len, size_t tag_len);
int  AES_GCM_aad_update(...);
int  AES_GCM_encrypt_update(...);
int  AES_GCM_decrypt_update(...);
int  AES_GCM_encrypt_finish(struct AES_GCM_ctx *ctx, uint8_t *tag);
int  AES_GCM_decrypt_finish(struct AES_GCM_ctx *ctx, const uint8_t *tag);
int  AES_GCM_encrypt(...);  /* one-shot; tag_len fixed for that key use */
int  AES_GCM_decrypt(...);  /* one-shot, auth-before-release */
void AES_GCM_clear(struct AES_GCM_ctx *ctx);

int  AES_CCM_encrypt(...);
int  AES_CCM_decrypt(...);
int  AES_EAX_encrypt(...);
int  AES_EAX_decrypt(...);
int  AES_EAX_PRIME_encrypt(...);
int  AES_EAX_PRIME_decrypt(...);

/* SIV=1: key length is 2×AES_KEYLEN; V is a 16-byte synthetic IV. */
int  AES_SIV_encrypt(const uint8_t *key,
                     const uint8_t *const *ad, const size_t *ad_lens,
                     size_t ad_count,
                     const uint8_t *pt, size_t pt_len,
                     uint8_t v[16], uint8_t *ct);
int  AES_SIV_decrypt(...);

/* CMAC=1: SP 800-38B; tag_len is AES_CMAC_MIN_TAG_LEN..16 (default min 8). */
int  AES_CMAC(const uint8_t *key, const uint8_t *msg, size_t msg_len,
              uint8_t *tag, size_t tag_len);
int  AES_CMAC_verify(const uint8_t *key, const uint8_t *msg, size_t msg_len,
                     const uint8_t *tag, size_t tag_len);
```

C++ projects should include `aes.hpp`.

## Examples

### CTR (default mode)

```c
struct AES_ctx ctx;
uint8_t key[16] = { /* ... */ };
uint8_t iv[16]  = { /* unique per message under this key */ };
uint8_t buf[64];

AES_init_ctx_iv(&ctx, key, iv);
if (AES_CTR_crypt(&ctx, buf, sizeof buf) != AES_OK) {
    /* counter would wrap; choose a new IV */
}
AES_ctx_clear(&ctx);
AES_secure_zero(key, sizeof key);
```

### CBC

```c
/* length must be a multiple of 16; padding is the caller's responsibility */
if (AES_CBC_encrypt(&ctx, buf, len) != AES_OK) {
    /* misaligned length or STRICT NULL failure */
}
```

### OFB

```c
AES_init_ctx_iv(&ctx, key, iv);
AES_OFB_crypt(&ctx, chunk1, n1); /* stream position preserved */
AES_OFB_crypt(&ctx, chunk2, n2);
```

### ECB (avoid unless you must)

```c
AES_init_ctx(&ctx, key);
AES_ECB_encrypt(&ctx, block16);
```

### CCM (one-shot AEAD)

```c
uint8_t ct[64], tag[16];
if (AES_CCM_encrypt(key, nonce, nonce_len, aad, aad_len,
                    pt, pt_len, ct, tag, sizeof tag) != AES_OK)
    return -1;
/* On decrypt failure the library zeros the plaintext buffer. */
```

### EAX (one-shot AEAD)

```c
/* tag_len must be >= AES_EAX_MIN_TAG_LEN (default 8) and <= 16 */
if (AES_EAX_decrypt(key, nonce, nlen, aad, alen, ct, clen, tag, tlen, pt)
    != AES_OK) {
    /* authentication failed; pt unchanged */
}
```

### SIV (RFC 5297, one-shot)

```c
/* key is AES_SIV_KEYLEN (2×AES_KEYLEN) bytes: K1||K2 for CMAC and CTR. */
const uint8_t *ad[2] = { aad, nonce }; /* empty aad still counts as a component */
size_t ad_lens[2] = { aad_len, nonce_len };
uint8_t v[AES_SIV_V_LEN];
uint8_t ct[pt_len];

/* v may alias pt when ct is distinct; v must be disjoint from ct. */
if (AES_SIV_encrypt(key, ad, ad_lens, 2, pt, pt_len, v, ct) != AES_OK)
    return -1;
/* Auth failure wipes the plaintext buffer (no large temp on MCU). */
```

### CMAC (SP 800-38B, one-shot)

```c
uint8_t tag[16];
/* Full tag, or truncate to >= AES_CMAC_MIN_TAG_LEN (default 8) leading octets. */
if (AES_CMAC(key, msg, msg_len, tag, sizeof tag) != AES_OK)
    return -1;
if (AES_CMAC_verify(key, msg, msg_len, tag, 8) != AES_OK)
    return -1; /* constant-time compare of truncated tag */
```

### GCM one-shot (preferred for whole messages)

```c
uint8_t ct[64], tag[16];
if (AES_GCM_encrypt(key, iv, iv_len, aad, aad_len, pt, pt_len,
                    ct, tag, sizeof tag) != AES_OK)
    return -1;

if (AES_GCM_decrypt(key, iv, iv_len, aad, aad_len, ct, pt_len,
                    tag, sizeof tag, pt) != AES_OK)
    return -1; /* pt not released on failure (in-place is zeroed) */
```

### GCM streaming

```c
struct AES_GCM_ctx gctx;
/* tag_len is fixed for this key/context (SP 800-38D §5.2.1.2). */
AES_GCM_init(&gctx, key, iv, iv_len, 16);
AES_GCM_aad_update(&gctx, aad, aad_len);
AES_GCM_encrypt_update(&gctx, buf, len);
AES_GCM_encrypt_finish(&gctx, tag); /* tag must hold gctx.tag_len bytes */
AES_GCM_clear(&gctx);
```

**Streaming decrypt** writes candidate plaintext before the tag is checked.
Do not act on that buffer until `AES_GCM_decrypt_finish` returns `AES_OK`.
Prefer one-shot decrypt when the full ciphertext is available.

On tiny MCUs prefer: 16-byte tags, 12-byte IVs, one-shot GCM, and
`AES_TINY=1` with bitwise/auto GHASH (never table4 without shared table).

## RTOS and concurrency

- The library has **no internal locks**.
- Do not use the same `AES_ctx` or `AES_GCM_ctx` concurrently from multiple
  tasks or ISRs without external serialization.
- Prefer one context per owner (task or connection).
- `AES_init_sbox()` (runtime S-box profile) must run once before first use,
  under single-threaded init.
- AES is usually too heavy for short ISRs; measure stack and latency on target.
- Wipe keys with `AES_ctx_clear` / `AES_GCM_clear` / `AES_secure_zero` before
  returning buffers to pools.

## Security notes

- **IV / nonce requirements** (caller responsibility):
  - **CTR / OFB / CCM / EAX / GCM / SIV:** never reuse a nonce/IV with the same
    key (SIV is misuse-resistant but still degrades on reuse).
  - **CBC encryption:** the IV must be **unpredictable** (typically random) at
    the time the plaintext is chosen; uniqueness alone is not enough.
- **One-shot AEAD buffers:** exact in-place alias and fully disjoint buffers
  are OK; **partial overlap returns `AES_ERR`**.
- **No padding** is provided. CBC/ECB lengths must be multiples of 16 bytes.
- CBC rejects non-aligned lengths with `AES_ERR`. CTR rejects requests that
  would wrap the 128-bit counter (buffer and IV left unchanged).
- EAX tags must be at least 8 bytes by default (library policy, not NIST GCM).
- **GCM (SP 800-38D):** tag length *t* is fixed at `AES_GCM_init` / one-shot
  entry and must be 4, 8, or 12–16 bytes. Input length caps match the standard
  (`AES_GCM_MAX_*`). Short tags (4/8) enforce Appendix C **per-packet**
  |C|+|A| bounds (most permissive table row). **Key lifetime / max decryption
  invocations** for short tags and IV uniqueness are **not** tracked in RAM —
  the firmware must rotate keys and generate IVs correctly.
- EAX′ uses the fixed four-byte C12.22 tag by specification.
- Default S-box access uses fixed-size masked scans to reduce cache-timing
  leakage. Constant-time behaviour still depends on the compiler and CPU.
- `AES_SBOX_MODE_FAST` and GHASH `fast-table` are **not** constant-time.
- ECB mode is insecure for almost all multi-block use cases.

## Residual risks (not fixed by this library)

| Issue | Reason |
|-------|--------|
| True constant-time on all platforms | Compiler and microarchitecture dependent |
| Nonce uniqueness | Application protocol |
| Padding schemes | Out of scope |
| Streaming GCM plaintext before tag | Inherent to incremental decrypt API |
| Power / EM / residual cache leakage | Beyond portable C S-box scans |
| Replay protection, KDF, RNG | Out of scope |
| Undersized caller buffers | Caller must size outputs correctly |
| Wipe vs aggressive LTO | Best-effort `volatile` stores |

## Build and test

```text
make                    # aes.o with default CTR
make size
make test               # matrix of modes and profiles
make AES_CAVP=1 test    # full vendored CAVP corpora
make benchmark AES_ENABLE_GCM=1
```

CMake:

```text
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Unit tests use vendored µunit. Vectors under `test_vectors/` include NIST SP
800-38A samples; CCM/GCM CAVP (optional full run); EAX Appendix G / Wycheproof;
C12.22 EAX′ notes; SIV RFC 5297 / Wycheproof; and full AES-CMAC CAVP Gen/Ver
plus Wycheproof.

## Contributing

This project uses the [Bounded Contribution Policy](CODE_OF_CONDUCT.md) to keep
discussions and contributions focused on technical goals. See
[CONTRIBUTING.md](CONTRIBUTING.md) for build/test expectations and PR guidance.
Sister DES/3DES library: [tiny-DES-c](https://github.com/mistial-dev/tiny-DES-c).

## License

AES implementation: public domain (Unlicense). Vendored µunit: MIT.
Upstream contributors are credited in the original project history.
Contribution policy:
[OpenPhysical BoundedContributionPolicy](https://github.com/OpenPhysical/BoundedContributionPolicy).

