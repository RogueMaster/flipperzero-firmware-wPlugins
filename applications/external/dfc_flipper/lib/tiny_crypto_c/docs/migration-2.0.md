<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# Migrating to 2.0

Version 2.0 updates public interfaces across algorithms and protocol layers.
Update all calls as one source change. Each operation has one 2.0 entry point.

## Byte ranges and results

Pass immutable byte ranges as `TC_bytes` and writable ranges as `TC_buffer`.
Cipher keys and IVs, in-place block modes, streaming and one-shot
authenticated encryption, MACs, key wrap, hash input, KDFs, protocol parsers and
writers all take spans. Hash digests are written to fixed arrays of the digest
length. `TC_random_fn` callbacks take a pointer and a length. Public operation
statuses now share `TC_result`. Existing names, such as `TC_status`,
`TC_RSA_result` and `TC_TLV_result`, are aliases, and their value names remain
available. Code may use the module names at call sites or a
single `TC_result` handler across modules.

Parsed and validated output records now use the `_report` suffix, including
`TC_X509_path_report`, `TC_X509_validation_report`, `TC_X509_ocsp_report`,
`TC_X509_revocation_report`, and the PIV credential reports. The `_result` and
`_status` suffixes are reserved for operation status aliases and domain state.

Streaming hash and MAC contexts use `init`, `update`, `final` and `ctx_clear`.
Streaming GCM encryption uses `init`, `aad_update`, `encrypt_update`,
`encrypt_finish` and `ctx_clear`. C++ owners use `finish` and clear their state
on destruction. One-shot functions name the operation they perform, such as
`digest`, `encrypt`, `sign` or `verify`.

## Workspaces and execution

Cryptographic one-shots take configuration, inputs, outputs, workspace, then
the work budget or `TC_execution`. RSA and EC randomized operations share
`TC_execution`. ECDSA signing is deterministic and takes
`TC_ECDSA_sign_options` plus a work budget.

## RSA sizes

Enable required modulus sizes independently with
`TINY_CRYPTO_RSA_ENABLE_1024`, `_2048`, `_3072` and `_4096`. RSA-1024 is a
legacy interoperability option and defaults off. PIV and TWIC key policy
rejects it, except TWIC Legacy key proofs that set `allow_rsa1024`.

## Removed switches

`TC_ZEROIZE` and `TC_STRICT` were removed. Public argument checks and
secret-state wiping are unconditional.

The authoritative old-to-new CMake option mapping is the `retired_options`
table in `cmake/features.json`. It lists the option names released in 1.x.
Configuration stops on a retired name and reports its replacement.

## Authentication behavior

An authenticated revocation from either OCSP or a CRL wins over a good result.
PIV/TWIC card checking requires a complete canonical inventory, authenticated
Discovery data, and a card CVC bound to the live secure-messaging session.
GCM decryption remains one-shot so plaintext is released only after tag
verification.
