<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# Elliptic-curve operations

Include `<tiny_crypto/ec.h>` for P-192, P-256 and P-384 key generation,
public-key derivation and validation, ECDH, and ECDSA signing and
verification. Each curve must be enabled in the build. Public keys use SEC 1
uncompressed encoding: `04 || X || Y`. Coordinates and private scalars are
fixed-width big-endian values, 32 bytes for P-256 and 48 bytes for P-384.
P-192 uses 24 bytes and is disabled by default. Enable `TINY_CRYPTO_EC_ENABLE_P192`
for protocols that require it.

`TC_EC_coordinate_bytes(curve)` returns that width, or 0 for a curve that is
unknown or disabled in the build. Size buffers from it: a public key is
`1 + 2 * width` bytes, a signature is `2 * width` bytes, and an ECDH secret is
`width` bytes. `TC_EC_MAX_BYTES` is the largest width in the build.

## Calling conventions

Every function returns a `TC_EC_result`:

| Status              | Meaning                                                                                                                                                               |
| ------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `TC_EC_OK`          | The operation completed and its outputs were written.                                                                                                                 |
| `TC_EC_INVALID`     | A key or signature whose length does not match the curve, a private scalar outside `[1, n-1]`, a point that is not on the curve, or a signature that does not verify. |
| `TC_EC_LIMIT`       | An output buffer shorter than required, or the work budget or the random-attempt limit ran out.                                                                       |
| `TC_EC_ARGUMENT`    | A NULL pointer, an empty digest or overlapping storage.                                                                                                               |
| `TC_EC_UNSUPPORTED` | The curve is unknown or disabled in this build.                                                                                                                       |
| `TC_EC_ERROR`       | The random source failed, or a new signature failed its own verification.                                                                                             |

Each function checks its arguments once and reports the first problem in
this order: `TC_EC_ARGUMENT`, `TC_EC_UNSUPPORTED`, `TC_EC_INVALID` for input
lengths, then `TC_EC_LIMIT` for output capacity and work. These checks leave
the outputs, workspace, work budget and random source untouched.

Inputs are `TC_bytes` spans whose length must match the curve exactly.
Outputs are `TC_buffer` spans with at least the required capacity. A larger
buffer is accepted and exactly the required length is written. Outputs
change only on `TC_EC_OK`.

Every call takes a `TC_work_budget`. `TC_EC_operation_work(curve, operation)`
returns the cost of one operation, or of one attempt for the randomized
operations. A scalar multiplication or a modular inversion costs one unit per
curve bit, and point validation and each random request cost one unit. The
operation checks its full cost before it starts and returns `TC_EC_LIMIT` with
the budget unchanged when it is short.

Randomized operations take a `TC_EC_execution` that groups the random
source, the maximum number of random requests and the work budget. The random
source must be cryptographically secure and fill each request completely.

## Keys and ECDH

`TC_EC_generate_key_pair` draws a private scalar, retries out-of-range draws
within `random_attempts`, and writes the scalar and its public key.
`TC_EC_public_key` derives a public key from a private scalar.
`TC_EC_validate_public_key` checks that a public key is a point on the curve
(SEC 1 section 3.2.2.1).

`TC_ECDH` validates the peer key and writes the shared point's X coordinate,
including leading zero bytes. Pass it through the protocol's key derivation
function before using it as a symmetric key.

## ECDSA

`TC_ECDSA_verify_digest` takes the public key, a precomputed digest and a
fixed-width `r || s` signature. Convert DER-encoded signatures to this form
first. Hash the message with the algorithm the protocol requires. A digest
longer than the curve order is truncated to its leftmost bytes. A shorter
digest is zero-extended. Both high and low values of `s` are accepted.
Verification establishes signature validity only. Key identity and trust
come from certificate validation. Its point multiplication branches on
public signature and digest values.

`TC_ECDSA_sign_digest` takes the private scalar, matching public key, digest
hash algorithm and a retry bound. It derives each secret nonce with RFC 6979,
so a repeated or restored random source cannot expose the private key.

`TC_ECDSA_sign_digest_external_random` is the advanced API for protocols that
must supply nonces externally. Each attempt draws an independent secret nonce
from its execution object. Repeating a nonce across different digests exposes
the private key.

With `TC_ECDSA_SIGN_VERIFY` set, the default, both signing functions verify the
new signature against the public key before writing it. A fault during
signing, or a public key from another key pair, then returns `TC_EC_ERROR`
with the output unchanged. `TC_ECDSA_SIGN_VERIFY` is a compile definition with
no CMake option. Set `TC_ECDSA_SIGN_VERIFY=0` only where the verification cost
is unacceptable and faults are handled another way. Key generation,
public-key derivation, ECDH and signing use constant-work multiplication for
secret scalars.

## Storage

All operations use caller-owned scratch memory. Keep it separate from input
and output buffers, and give concurrent calls separate workspaces. Scratch is
wiped after use. Argument rejection leaves it untouched. Use
`sizeof(TC_EC_workspace)` or `sizeof(TC_ECDSA_workspace)` to size it for the
configured curves and limb width.

On AVR the library uses byte limbs. `tests/budgets/avr.json` records an
ATmega2560 ECDSA P-256 profile, with P-384 disabled and
`TC_ECDSA_SIGN_VERIFY` on. It exercises deterministic signing and standalone
verification: at most 18000 bytes of flash, 1500 bytes of static RAM including
a static `TC_ECDSA_workspace` of 1218 bytes, and 1000 bytes of project stack.
The profile is not supported on the ATmega328P because its 2 KiB RAM cannot
hold the measured static data and worst-case project call chain. Check it with
`python3 tools/measure_avr_resources.py --check tests/budgets/avr.json`.
The file's `avr_gcc_version` names the toolchain the budgets were measured
with. The check prints it beside the version in use and accepts either.

The C++11 equivalents are in `<tiny_crypto/ec.hpp>`, including
`ec_coordinate_bytes`. They take `bytes` inputs, fixed-size output arrays and
references to the workspace and budget. `ec_generate_key_pair` and
`ecdsa_sign_digest_external_random` take an execution object, and
`ecdsa_sign_digest` takes its options after the workspace. They return the
same `TC_EC_result` values, use no heap and are `noexcept`.

## Tests

`test_ec_0` and `test_ec_1` cover each limb width. `test_ec_p256`,
`test_ec_p384` and `test_ec_rfc6979` check single-curve builds and the RFC 6979
answers. The extended `test_ec_cavp`, `test_wycheproof_ec` and
`test_wycheproof_ecdsa` run the NIST CAVP and Wycheproof suites.

Configure with `-DTINY_CRYPTO_TEST_OPENSSL=ON` to build the OpenSSL 3
comparison tests. Set `OPENSSL_ROOT_DIR` when the default OpenSSL is older
than 3. After building, run:

```sh
ctest --test-dir build -R '^test_ecdsa_openssl_' --output-on-failure
```

These munit tests exercise both limb widths and all three curves. OpenSSL is a
test dependency only. `test_ecdsa_sign_verify_*` injects a fault between
signing and self-verification, with the check enabled and disabled.
