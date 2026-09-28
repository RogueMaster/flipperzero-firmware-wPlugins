<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# Elliptic-curve operations

Include `<tiny_crypto/ec.h>` for P-192, P-256 and P-384 public-key derivation,
ECDH and ECDSA verification. Each curve must be enabled in the build.
Public keys use SEC 1 uncompressed encoding: `04 || X || Y`. Coordinates
and private scalars are fixed-width big-endian values, 32 bytes for P-256
and 48 bytes for P-384. P-192 uses 24 bytes and is disabled by default;
enable `TINY_CRYPTO_EC_P192` for protocols that require it.

`TC_EC_public_key` derives a public key from a private scalar. `TC_ECDH`
returns the shared point's X coordinate; pass it through the protocol's
key derivation function before using it as a symmetric key. Both calls
take a `TC_EC_workspace` and leave output unchanged on failure.

## ECDSA verification

`TC_ECDSA_verify_digest` takes the public key, a precomputed digest, a
fixed-width `r || s` signature and a `TC_ECDSA_workspace`. It does not
accept DER-encoded signatures. Hash the message using the algorithm
required by the protocol. A digest longer than the curve order is
truncated to its leftmost bytes; a shorter digest is zero-extended.

The result is `TC_OK` for a valid signature, `TC_MISMATCH` for an invalid
signature or public key, and `TC_ERROR` for invalid arguments or an
unavailable curve. Both high and low values of `s` are accepted.
Verification establishes the signature's validity, not the key's identity
or trustworthiness.

All operations use caller-owned scratch memory. Keep it separate from
input and output buffers, and give concurrent calls separate workspaces.
Scratch is wiped after use; argument rejection leaves it untouched.
Use `sizeof(TC_EC_workspace)` or `sizeof(TC_ECDSA_workspace)` to size it
for the configured curves and limb width.

The C++11 equivalents are in `<tiny_crypto/ec.hpp>`. Use
`tiny_crypto::ecdsa_verify_digest` with pointer-length `bytes` inputs and an
`ecdsa_workspace` reference. It returns the same status codes and does not
allocate or throw exceptions.

## Tests

Configure with `-DTINY_CRYPTO_TEST_OPENSSL=ON` to build the OpenSSL 3
comparison tests. After building, run:

```sh
ctest --test-dir build -R '^test_ecdsa_openssl_' --output-on-failure
```

These munit tests exercise both limb widths and all three curves. OpenSSL is a
test dependency only.
