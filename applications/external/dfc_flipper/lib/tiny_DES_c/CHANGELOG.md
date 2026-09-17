<!--
SPDX-FileCopyrightText: Mistial Dev
SPDX-License-Identifier: Unlicense
-->

# Changelog

## Unreleased

### BREAKING

- Default compile surface is **CTR + Triple-DES only**. ECB, CBC, OFB, CFB1/8/64,
  and CMAC are **off** unless enabled with `DES_ENABLE_*` / CMake
  `TINY_DES_ENABLE_*`.
- Mode selection uses **only** `DES_ENABLE_*` macros. Bare `CBC` / `ECB` /
  `CTR` / `OFB` / `CFB*` / `TDES` defines are **removed** (avoids clashes with
  `aes.h`). Pass `-DDES_ENABLE_CBC=1`, not `-DCBC=1`.
- Classical APIs renamed and now return `int` (`DES_OK` / `DES_ERR`):
  - `DES_CBC_encrypt_buffer` → `DES_CBC_encrypt` (same for decrypt / DES3)
  - `DES_CTR_xcrypt_buffer` → `DES_CTR_crypt`
  - `DES_OFB_xcrypt_buffer` → `DES_OFB_crypt`
  - `DES_CFB*_encrypt_buffer` → `DES_CFB*_encrypt` (and decrypt / DES3)
- CBC rejects lengths that are not multiples of 8 with `DES_ERR`.
- CTR returns `DES_ERR` if a request would wrap the 64-bit counter (buffer and
  IV left unchanged).
- `DES3_init_ctx` / `DES3_init_ctx_iv` return `int` and reject keylen ∉ {16,24}.
- CMAC API replaced:
  - Removed `DES_cmac` and non-standard `DES_cmac_with_iv`
  - Added gated `DES_CMAC` / `DES_CMAC_verify` with `tag_len` and
    `DES_CMAC_MIN_TAG_LEN` (default 8)
- No compatibility aliases are provided; update call sites.

### Added

- `DES_OK` / `DES_ERR`, `DES_secure_zero`, `DES_ctx_clear`, `DES3_ctx_clear`
- `DES_ZEROIZE` (default on) and `DES_STRICT` (optional NULL checks)
- OpenSSL-cross-checked CMAC KATs; negative API tests; sparse Make configs
- Partial final CFB-64 segment support without buffer over-read
- PlatformIO / Arduino library metadata; CTR benchmark target
- README rewritten for MCU defaults, footprint, security notes, residual risks

### Changed

- Makefile product target is `des.o`; tests use a full-feature object and sparse
  configuration matrix
- C++ `des.hpp` mode-gates all methods and wipes context on destroy
