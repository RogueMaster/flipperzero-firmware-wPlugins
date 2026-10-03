<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# Deterministic random bit generators

Include `<tiny_crypto/drbg.h>` and enable `TINY_CRYPTO_ENABLE_DRBG=ON`. The
library provides the three NIST SP 800-90A Rev. 1 mechanisms:

| Mechanism | Option                         | Primitives                                  | Security strength    |
| --------- | ------------------------------ | ------------------------------------------- | -------------------- |
| Hash_DRBG | `TINY_CRYPTO_DRBG_ENABLE_HASH` | SHA-1, SHA-224, SHA-256, SHA-384 or SHA-512 | 128, 192 or 256 bits |
| HMAC_DRBG | `TINY_CRYPTO_DRBG_ENABLE_HMAC` | HMAC with the same hashes                   | 128, 192 or 256 bits |
| CTR_DRBG  | `TINY_CRYPTO_DRBG_ENABLE_CTR`  | AES-128, AES-192 or AES-256                 | the AES key size     |

The desktop profile enables all three. A mechanism needs its primitives:
HMAC_DRBG needs `TINY_CRYPTO_ENABLE_HMAC`, and CTR_DRBG needs
`TINY_CRYPTO_ENABLE_AES` with `TINY_CRYPTO_AES_ENABLE_DYNAMIC`, which selects the AES
key size at run time. The strength is always the mechanism's maximum: 128
bits for SHA-1, 192 for SHA-224 and 256 for SHA-256, SHA-384 and SHA-512.

A DRBG turns seed material into pseudorandom output. It needs an entropy
source for instantiation and reseeding, and its output is only as strong as
that source. Supply a conditioned source that meets SP 800-90B, or an
equivalent requirement, for the configured strength. Suitable sources include
a hardware TRNG, a secure element or a vetted operating-system interface. A
slow source is sufficient, because the DRBG draws from it only when it
instantiates or reseeds.

## Lifecycle

```c
TC_DRBG_config config = {0};
config.mechanism = TC_DRBG_HMAC;
config.hash = TC_HASH_SHA256;
TC_random_source entropy = {platform_entropy_fill, &platform};
static TC_DRBG drbg;

if (TC_DRBG_instantiate(&drbg, &config, entropy, nonce, personalization) != TC_DRBG_OK)
  return failure;
if (TC_DRBG_generate(&drbg, (TC_buffer){key, sizeof key}, 0, label) != TC_DRBG_OK)
  return failure;
TC_DRBG_uninstantiate(&drbg);
```

`TC_DRBG_instantiate` reads the entropy input and seeds the state. It asks the
source for `entropy_bytes`, which defaults to the strength in bytes. An empty
nonce is drawn from the same source in the same call, at half the strength
(SP 800-90A section 8.6.7). A supplied nonce has at least that length. The
personalization string is optional and separates instances, for example with
a device identifier. Instantiating a context that already holds a DRBG
replaces it.

`TC_DRBG_generate` writes up to `TC_DRBG_MAX_REQUEST_BYTES` (65,536 bytes)
per call. Additional input is optional per call and binds the output to a
purpose or a message. `TC_DRBG_reseed` mixes in fresh entropy and optional
additional input. `TC_DRBG_uninstantiate` wipes the whole context.

CTR_DRBG without a derivation function (`derivation_function = 0`) follows
SP 800-90A section 10.2.1. Its entropy input is exactly the seed length (the
key length plus 16 bytes), it takes no nonce, and personalization and
additional input are at most the seed length. With `derivation_function = 1`,
CTR_DRBG accepts the same inputs as the hash mechanisms.

Every mechanism limits a nonce, personalization string or additional input to
`TC_DRBG_MAX_INPUT_BYTES`, and a nonce plus personalization string to the same
total. This stays within the 2^35-bit limit of SP 800-90A Tables 2 and 3 and
the 32-bit length field of the CTR_DRBG derivation function. A larger input
returns `TC_DRBG_ARGUMENT` before any entropy is drawn, and the DRBG stays
usable.

## Reseeding and prediction resistance

Each generate call counts toward `reseed_interval`, which defaults to 2^48
requests, the SP 800-90A maximum. When the count passes the interval, the
next generate call reseeds from the entropy source first. A smaller interval
bounds how much output one seed produces.

Set `prediction_resistance = 1` at instantiation to allow
prediction-resistant requests. Passing a nonzero `prediction_resistance`
argument to `TC_DRBG_generate` then reseeds before that output, so the
output stays unpredictable even if the earlier state leaked. Requesting it
from a DRBG instantiated without it returns `TC_DRBG_ARGUMENT`.

## Results and failures

| Result                | Meaning                                           | State afterwards                                    |
| --------------------- | ------------------------------------------------- | --------------------------------------------------- |
| `TC_DRBG_OK`          | Success                                           | Usable                                              |
| `TC_DRBG_ARGUMENT`    | Invalid argument, overlap, state or configuration | Unchanged, and a failed instantiate leaves it wiped |
| `TC_DRBG_UNSUPPORTED` | Mechanism or hash compiled out of this build      | Wiped                                               |
| `TC_DRBG_LIMIT`       | Request larger than `TC_DRBG_MAX_REQUEST_BYTES`   | Unchanged                                           |
| `TC_DRBG_ENTROPY`     | The entropy source failed                         | Unchanged, and a failed instantiate leaves it wiped |
| `TC_DRBG_ERROR`       | A hash, HMAC or AES operation failed              | Unusable until uninstantiated                       |

`TC_DRBG_ARGUMENT` and `TC_DRBG_LIMIT` leave the output buffer unchanged.
`TC_DRBG_ENTROPY` and `TC_DRBG_ERROR` from a generate call wipe it. An entropy
failure during a reseed or a prediction-resistant request leaves the
generator usable, so the caller can retry later. Output, additional input,
nonce and personalization must be disjoint from the context, and output must
be disjoint from additional input.

## Storage

The caller owns the `TC_DRBG`. It holds the working state, one entropy buffer
of `TC_DRBG_MAX_ENTROPY_BYTES` (64 by default, at least 48, set in `config.h`)
and scratch space for the largest enabled hash, HMAC or AES key schedule, so
each call uses little stack. Place it in static or long-lived storage. Its
fields are private.

| Configuration             | `sizeof(TC_DRBG)` on a 64-bit host | On AVR (ATmega2560) |
| ------------------------- | ---------------------------------: | ------------------: |
| HMAC_DRBG, SHA-256 only   |                                328 |                 302 |
| Hash_DRBG, SHA-256 only   |                                344 |                 316 |
| CTR_DRBG only             |                                624 |                 598 |
| All mechanisms and hashes |                                656 |                 623 |

## Random sources for other APIs

`TC_DRBG_random_source(&drbg)` returns a `TC_random_source` for RSA key
generation, EC key generation and signing, key challenges and PIV secure
messaging. `TC_DRBG_random` splits large requests into maximum-size calls. It
rejects a NULL or uninstantiated DRBG and output that overlaps the DRBG before
the first call, with the output unchanged. It wipes the whole output when a
later call fails. The DRBG must stay instantiated while the source is
in use.

## C++

Include `<tiny_crypto/drbg.hpp>` and use `tiny_crypto::DRBG`. Its
`instantiate`, `reseed` and `generate` members forward to the C API and return
its `TC_DRBG_result`. `random_source()` returns the `TC_random_source` of
`TC_DRBG_random_source`, and the object must outlive it. The destructor
uninstantiates. The class has copying and moving disabled, because a copy
would repeat the original's output.

## Example

[examples/drbg.c](../examples/drbg.c) seeds an HMAC_DRBG from a platform
entropy source with a device identifier as personalization. It derives
labelled session keys, reseeds on demand and stops the generator on a
primitive failure. `test_drbg_example` builds and runs it.

## Tests

- `test_drbg` covers the lifecycle for each mechanism, argument and overlap
  rejection, request and input limits, entropy failures at instantiate,
  reseed and prediction-resistant generate, automatic reseeding, state
  wiping, overlap rejection without writes and the random-source adapter.
- `test_drbg_cavp` runs 11,520 NIST CAVP DRBGVS trials from
  [tests/vectors/drbg](../tests/vectors/drbg/README.md) across the
  prediction-resistant, reseeding and no-reseed procedures. It carries the
  `extended` label.
- `test_cpp_drbg` checks the C++ wrapper against a CAVP answer.
- The AVR compile checks build every DRBG source, and compile profiles build
  each mechanism alone.

```sh
ctest --test-dir build --output-on-failure -R 'drbg'
```

## Limitations

The implementation follows SP 800-90A Rev. 1 and passes the published CAVP
DRBGVS answers. It has no CAVP or CMVP validation certificate. SHA-512/224,
SHA-512/256 and TDEA are out of scope. Health testing of the entropy source,
and any continuous tests FIPS 140-3 requires around it, are the caller's
responsibility.
