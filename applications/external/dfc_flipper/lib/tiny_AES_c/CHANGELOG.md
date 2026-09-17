<!--
SPDX-FileCopyrightText: kokke
SPDX-FileCopyrightText: Mistial Dev
SPDX-License-Identifier: Unlicense
-->

# Changelog

## Unreleased

### Added

- [Bounded Contribution Policy](CODE_OF_CONDUCT.md) (OpenPhysical v2.0.3),
  [CONTRIBUTING.md](CONTRIBUTING.md), and GitHub issue/PR templates.

### BREAKING

- Status codes are only `AES_OK` / `AES_ERR`. Per-mode `AES_*_SUCCESS` and
  `AES_*_ERROR` macros are removed.
- `AES_keyExpSize` is renamed to `AES_KEY_EXP_SIZE`.
- Classical APIs renamed and now return `int`:
  - `AES_CBC_encrypt_buffer` → `AES_CBC_encrypt`
  - `AES_CBC_decrypt_buffer` → `AES_CBC_decrypt`
  - `AES_CTR_xcrypt_buffer` → `AES_CTR_crypt`
  - `AES_OFB_xcrypt_buffer` → `AES_OFB_crypt`
- CBC rejects lengths that are not multiples of 16 with `AES_ERR`.
- CTR returns `AES_ERR` if a request would wrap the 128-bit counter (buffer and
  IV left unchanged).
- EAX rejects tags shorter than `AES_EAX_MIN_TAG_LEN` (default 8) and zero-length
  tags.
- GCM follows NIST SP 800-38D more strictly:
  - Tag length *t* is fixed at `AES_GCM_init(..., tag_len)` / one-shot entry
    (not re-chosen at finish).
  - `AES_GCM_encrypt_finish` / `AES_GCM_decrypt_finish` take only the tag buffer.
  - Input limits: IV/AAD/P caps; short tags enforce Appendix C per-packet
    |C|+|A| bounds. Invocation quotas and IV uniqueness remain application work
    (no key-lifetime store on MCU).
- No compatibility aliases are provided; update call sites.

### Added

- Opt-in **AES-CMAC** (NIST SP 800-38B): heap-free one-shot `AES_CMAC` /
  `AES_CMAC_verify` with truncated tags (`AES_CMAC_MIN_TAG_LEN`..16; product
  default min 8). Shares the CMAC core with SIV. Full NIST CAVP AES Gen/Ver
  corpora and full Wycheproof `aes_cmac_test.json`; CMAC test builds use
  `AES_CMAC_MIN_TAG_LEN=4` so CAVP short-tag rows hit the public API.
- Opt-in **AES-SIV** (RFC 5297 SIV-AES / AES-SIV-CMAC-256/384/512): heap-free
  one-shot encrypt/decrypt with multi-component associated data. Coverage from
  RFC 5297 Appendix A and vendored Wycheproof `aead_aes_siv_cmac_test.json`.
- `AES_secure_zero` and `AES_ctx_clear`; `AES_ZEROIZE` (default on) wipes stack
  secrets in one-shot CCM/EAX/EAX'/GCM/SIV/CMAC paths.
- `AES_STRICT` optional NULL checks on classical buffer APIs.
- One-shot `AES_GCM_encrypt` / `AES_GCM_decrypt` (auth-before-release decrypt).
- `AES_TINY` compile-time rejection of table4/fast-table GHASH.
- One-shot AEAD buffer contract: exact alias or fully disjoint only;
  partial overlap returns `AES_ERR`.
- README rewritten for MCU use: gating, sizeof table, RTOS notes, examples, and
  residual risks.

### Changed

- CCM/EAX/EAX' one-shots use packed stack workspaces for predictable depth.
- Professional documentation tone; accurate default-mode gating.
- Removed `AES_GCM_SHARED_TABLE` (unsafe when multiple keys share the table).
  Table GHASH is per-context only.
- CTR wrap checks are portable to 16-bit `size_t` (AVR).
- One-shot GCM validates lengths and buffer overlap before any output copy.
- README: CBC requires an **unpredictable** IV, not merely a unique one.
- Sparse matrix coverage for `AES_STRICT=1`, `AES_ZEROIZE=0`, `AES_TINY=1`,
  and multi-key GCM contexts (Make local matrix + Linux GCC CI step).
- Portable `aes_buffers_ok` via `uintptr_t` subtraction; SIV stages `v` so it
  may alias other buffers; expanded SIV API tests and fixed Wycheproof case
  counts (300 per key size).

- Added opt-in ANSI C12.22 EAX' support with Annex I.4 interoperability
  coverage, twelve boundary-focused worked vectors, and an OpenSSL-based
  independent cross-check.
- Added opt-in, heap-free AES-EAX authenticated encryption with the Appendix G
  EAX vectors and all 240 Wycheproof AES-EAX cases, including authentication
  failure, in-place, and API validation tests. Vendored vector attribution and
  Apache-2.0/public-domain licensing details are recorded with the corpus.
- Added opt-in, heap-free, one-shot AES-CCM authenticated encryption with
  NIST SP 800-38C and RFC 3610 vectors, plus unified opt-in CAVP response
  validation for AES block modes, CCM, and GCM through the single `AES_CAVP`
  switch. MCT intermediate records are checked, and the full corpus runs in
  GitHub Actions for pushes and pull requests. Samples and focused API
  boundary tests remain enabled by default.
- Added opt-in, streaming AES-OFB support with NIST SP 800-38A vectors for
  AES-128/192/256, chunked and partial-buffer tests, and compile-time mode
  selection that keeps unused modes out of embedded builds. CTR is now the
  only default mode; CBC, ECB, OFB, and GCM are opt-in.
- Added opt-in, streaming AES-GCM support with arbitrary IV lengths, compact
  NIST CAVP vectors, authenticated tag verification, a block-wise update path,
  and configurable constant-time, wide, table-based, and hardware GHASH
  profiles. The default remains heap-free and constant-time, GCM-disabled
  builds retain no GCM context or runtime overhead, and each context rejects
  mixed encryption/decryption updates. Documented GCM usage, nonce and
  authentication requirements, in-place decryption precautions, profile
  trade-offs, and expanded the test matrix to cover all portable GHASH modes.
- Added configurable secure, runtime-generated, and fast S-box profiles plus
  portable opt-in wide operations for embedded targets.
- Made AES-128 the default only when no AES key-size macro is supplied, so
  AES192 and AES256 can be selected directly by the build.
- Reworked the Make/CMake test matrix to build modes independently, exercise
  each implementation profile at least once, and avoid redundant mode-subset
  combinations.
- Reworked the unit tests around vendored µunit (munit), added cross-platform
  CMake/CTest support, and organized the AES-128/192/256 NIST SP 800-38A
  Appendix F vectors into a shared test-vector header.
- Replaced secret-indexed AES S-box and inverse S-box lookups with fixed-size,
  masked scans so the table-access pattern is independent of the input byte.
