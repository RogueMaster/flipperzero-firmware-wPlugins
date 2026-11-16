<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# Changelog

## 2.1.0

### Added

- `TINY_CRYPTO_DES_SBOX_MODE` selects the constant-time algebraic DES S-box implementation or
  the faster combined lookup tables. Constant-time is the default for every resource profile.

## 2.0.0

2.0 changes public interfaces across the library. See [migrating to 2.0](docs/migration-2.0.md).

### Added

- SP 800-90A Hash_DRBG, HMAC_DRBG and CTR_DRBG.
- AES-KW and AES-KWP.
- KMAC256, HKDF and SSKDF.
- ISO/IEC 9797-1 MAC algorithms 1 and 3.
- Raw RSA operations and RSA-4096.
- RFC 6979 deterministic ECDSA signing, with a separate external-random interface.
- The card and credential layer: ISO/IEC 7816-4 APDU handling, PIV commands, secure messaging,
  VCI, object readers, the catalog and inventory, key proofs, card inspection, OCSP, CRLs,
  trust-anchor controls, CMS validation and credential policy.
- C++ wrappers for the cryptographic operations, TLV, APDU, PIV commands, secure messaging,
  the inventory and card checks.
- `TINY_CRYPTO_TARGET` application targets: `full` builds every algorithm and format, `piv`
  builds SP 800-73-5 with the SP 800-78-5 algorithms, `twic` adds the TWIC Legacy and NEXGEN
  support, and `desfire` builds the MIFARE DESFire primitives used by dfc-core. See
  [application targets](docs/targets.md).
- `TINY_CRYPTO_ENABLE_TWIC` selects TWIC card support in the PIV modules. The TWIC rules live
  in the `twic_*` sources, so a PIV build compiles no TWIC code.
- `TINY_CRYPTO_TEST_NULL_GUARD` repeats every public C call in the tests with each pointer
  argument set to NULL under the sanitizers.

### Changed

- Byte inputs use `TC_bytes` and outputs `TC_buffer`. Operation statuses share `TC_result`.
- Workspaces, cleanup, overlap and failure behavior follow common conventions. Spans and
  written regions follow one storage rule, which also rejects ranges that wrap the address
  space.
- Short authentication tags use explicit `_short_tag` entry points. HMAC and KMAC tags meet
  the RFC 2104 and SP 800-185 length floors, with short tags down to 32 bits (SP 800-107
  Rev. 1).
- Stateful contexts clear keyed state after a failed initialization or finalization.
- Private RSA operations check the complete work budget before processing secret values.
- RSA-1024, P-192 and the TDEA-CMAC KBKDF PRF are build options that the predefined targets
  select.
- Authenticated revocation from OCSP or a CRL takes precedence over a good result.
- `TC_PIV_CMS_BIOMETRIC_LEGACY` is now `TC_PIV_CMS_BIOMETRIC_FIPS201_1`, and the
  `twic_authenticate` option `--legacy-biometric-signature` is now
  `--fips201-1-biometric-signature`.
- `TC_CHUID_PROFILE_LEGACY_KEY_MAP` is now `TC_CHUID_PROFILE_PIV_SP800_73_4`. The PIV CHUID
  profile applies the SP 800-73-5 fields and UUID versions.
- The APDU channel rejects answers longer than Ne (ISO/IEC 7816-4 section 5.1), and
  ISO/IEC 7816-4 TLV length fields above five bytes are INVALID.
- A failed SELECT or a reselection keeps the selected application and its security state
  (SP 800-73-5 Part 2 section 3.1.1).

### Removed

- `TC_ZEROIZE` and `TC_STRICT`. Argument checks and secret wiping are unconditional.
- The 1.x CMake option names. `retired_options` in `cmake/features.json` maps each to its
  replacement.
- The `piv-acu` and `piv-pd` targets. Use `piv` or `twic`.

## 1.0.1

- Adds embedded PIV/TWIC validation, PKI support, and target benchmarks.

## 1.0.0

- Initial release.
