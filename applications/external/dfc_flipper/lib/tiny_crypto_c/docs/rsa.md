<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# RSA

Enable `TINY_CRYPTO_ENABLE_RSA=ON` and include `<tiny_crypto/rsa.h>`.
`TC_RSA_verify_v15_digest` verifies a precomputed SHA-1, SHA-224, SHA-256,
SHA-384 or SHA-512 digest using PKCS#1 v1.5. The caller decides which sizes and hashes its
protocol accepts and applies its trust policy separately.

Pass modulus and exponent as unsigned big-endian bytes, without ASN.1 INTEGER
sign padding. Signature length must match the modulus length. The digest length
must match the selected hash. Compute the message digest before calling the
verifier and validate certificate chains separately.

`TC_DER_rsa_public` in `<tiny_crypto/der.h>` reads a PKCS #1 `RSAPublicKey`
and returns borrowed modulus and exponent magnitudes with sign padding removed.
It checks integer encoding and positivity. Apply key-strength policy and use
the RSA API to check arithmetic constraints. X.509 key parsing uses this reader.

The supported moduli are 1024, 2048, 3072 and 4096 bits. RSA enables 2048, 3072 and 4096 by
default. RSA-1024 is a legacy size and needs `TINY_CRYPTO_RSA_ENABLE_1024=ON`. Each size has a
`TINY_CRYPTO_RSA_ENABLE_<bits>` option. A disabled size returns `TC_RSA_UNSUPPORTED`, and the
size and work helpers return zero for it. `TC_RSA_modulus_supported(bits)` reports whether a
size is enabled, and `TC_RSA_MAX_MODULUS_BYTES` (512) sizes caller storage that holds one
modulus-length value, such as a signature or an encoded message.

Allocate `TC_RSA_word` storage using the limb count returned by
`TC_RSA_workspace_words(TC_RSA_OPERATION_VERIFY, bits)`, or the matching
`TC_RSA_*_WORKSPACE_WORDS(bits)` macro for a static array. The function
returns zero for unsupported sizes. Keep the workspace separate from input
bytes and metadata. Verification wipes used scratch. Failures before
arithmetic leave scratch unused.

Pass the hash in `TC_RSA_v15_options`. `TC_work_budget.remaining` bounds the
count of modular operations and encoding comparisons and is reduced by work
performed, including work before an invalid signature is detected. The
verifier checks this amount before any arithmetic:

```c
TC_RSA_public_work(&key) + TC_RSA_encode_v15_work(&options, modulus_bytes)
```

See [work budgets](#work-budgets). Check the result explicitly: `TC_RSA_OK`,
`TC_RSA_INVALID`, `TC_RSA_LIMIT`, `TC_RSA_ARGUMENT`, or `TC_RSA_UNSUPPORTED`.
Only `TC_RSA_OK` accepts the signature.

## Status rule

Every RSA function checks its arguments once and reports the first problem in
this order:

1. `TC_RSA_ARGUMENT`: a NULL pointer, overlapping or misaligned storage, or a
   digest whose length differs from its known hash.
1. `TC_RSA_UNSUPPORTED` or `TC_RSA_INVALID` for the key: an unsupported modulus
   size, or a malformed modulus, exponent, private component or CRT value.
1. `TC_RSA_UNSUPPORTED` or `TC_RSA_INVALID` for the scheme: a disabled or
   unknown hash, or parameters such as a salt or label too long for the modulus.
1. `TC_RSA_INVALID` for received data: a signature, ciphertext or raw input of
   the wrong length.
1. `TC_RSA_LIMIT`: a caller output buffer shorter than the modulus or the
   documented size, then too little workspace, blinding attempts or work.

The arithmetic finds a representative at or above the modulus and returns
`TC_RSA_INVALID` after the limit checks pass. Output buffers larger than
required are accepted, and exactly the documented length is written.
`TC_RSA_ERROR` reports an RNG failure, a hash failure, or a private-key result
that fails its check with the public exponent. Argument errors and limits found
before arithmetic leave outputs, workspace and work unchanged. A blinding limit
reached after rejected factors consumes the work of each attempt, wipes the
workspace and leaves the output unchanged.

For repeated verification with one key, initialize a caller-owned
`TC_RSA_prepared_public_key` with `TC_RSA_prepare_public_key`. Preparation uses
a cache of one modulus width of limbs, a temporary workspace of two modulus
widths, and a budget of `16 * modulus_bytes + 1`. It wipes temporary storage
on success. Keep the borrowed modulus, exponent, and cache storage alive and
unchanged until `TC_RSA_prepared_public_key_clear`. Clear wipes only the setup. The caller
still owns the cache, which contains public data derived from the modulus.
Use `TC_RSA_verify_v15_prepared` or `TC_RSA_verify_pss_prepared` with a separate
verification workspace. `TC_RSA_prepared_public_work(&setup)` replaces
`TC_RSA_public_work` in the verification budget because the cached `R^2`
skips the setup work. The one-shot functions remain useful when the key is
used only once.

`TINY_CRYPTO_RSA_SMALL=ON` selects byte limbs, and the micro resource profile enables it by
default. Native builds otherwise use 32-bit limbs. AVR always uses byte limbs. RSA
verification needs neither EC nor a hash implementation when the caller supplies the digest.

## Encoding for card and hardware signing

For card or hardware signing, `TC_RSA_encode_v15_digest` produces the complete
EMSA-PKCS1-v1_5 representative from a precomputed digest. Supply 128, 256,
384, or 512 output bytes for an enabled RSA-1024, RSA-2048, RSA-3072, or RSA-4096 size and a
work budget covering that length. Keep output and the work budget separate from the options,
the digest and each other. Failures preserve the output.
The function shares the software signer's encoding implementation and requires
no hash context or RSA workspace. See
[card-key authentication](credential-reader.md#card-key-authentication)
for an example that submits this representative to a card.

`TC_RSA_encode_pss_digest` builds an EMSA-PSS representative with explicit
message and MGF hashes and caller-supplied salt. Enable both hashes and obtain
the salt from a cryptographic random source. The salt length must match the
options. Output capacity is 128, 256, 384, or 512 bytes for an enabled size, with `emBits`
one less than the modulus width in bits. Keep output and the work budget separate from all
inputs. Preflight errors preserve both. A failure during encoding wipes the
output and consumes work. The card transport submits the resulting bytes to
the private-key operation.

`TC_RSA_encode_v15_work` and `TC_RSA_encode_pss_work` return the exact work a
successful encoding consumes for given options and modulus size, or zero when
the options or size are unsupported. A smaller budget returns `TC_RSA_LIMIT`
with output and work unchanged. Use them to preflight a budget before
drawing digest or salt bytes from a random source. `TC_key_challenge_prepare`
does this so that a short budget fails before any RNG use.

## Work budgets

`TC_work_budget.remaining` is a `uint32_t` count of public work units:
modular operations, encoded and masked bytes, hash invocations and RNG
requests. The count is independent of elapsed time. The work functions return the exact cost
of one successful call, or zero when the operation would reject the arguments
or the cost exceeds `UINT32_MAX`:

| Operation                  | Work                                                                                             |
| -------------------------- | ------------------------------------------------------------------------------------------------ |
| `TC_RSA_raw_public`        | `TC_RSA_public_work(&key)`                                                                       |
| `TC_RSA_verify_v15_digest` | `TC_RSA_public_work(&key) + TC_RSA_encode_v15_work(&options, L)`                                 |
| `TC_RSA_verify_pss_digest` | `TC_RSA_public_work(&key) + TC_RSA_encode_pss_work(&options, L)`                                 |
| `TC_RSA_verify_*_prepared` | `TC_RSA_prepared_public_work(&setup)` in place of `TC_RSA_public_work`                           |
| `TC_RSA_encrypt_oaep`      | `1 + TC_RSA_oaep_work(&options, L) + TC_RSA_public_work(&key)`                                   |
| `TC_RSA_raw_private`       | `TC_RSA_private_work(&key, A)` for a key without CRT values                                      |
| `TC_RSA_sign_v15_digest`   | `TC_RSA_private_work(&key, A) + TC_RSA_encode_v15_work(&options, L)`                             |
| `TC_RSA_sign_pss_digest`   | `TC_RSA_private_work(&key, A) + TC_RSA_encode_pss_work(&options, L)`, plus 1 for a nonempty salt |
| `TC_RSA_decrypt_oaep`      | `TC_RSA_private_work(&key, A) + TC_RSA_oaep_work(&options, L)`                                   |

`L` is the modulus length in bytes and `A` is the number of blinding attempts.
`TC_RSA_private_work` reads only `key->public_key` and whether `key->crt` is
set, and selects the CRT cost when it is. Each rejected blinding factor costs
one more attempt, so budget with the same `A` as `execution.random_attempts`.
Operations preflight the cost of all `A` allowed attempts before any arithmetic
or RNG request. A smaller budget returns `TC_RSA_LIMIT` and leaves outputs,
work and the RNG untouched. Add costs with overflow checks. The
[signing](../examples/rsa_sign.c) and [encryption](../examples/rsa_encrypt.c)
examples show the pattern.

For reference, the public operation costs `16*L + 16*E + 4` for exponent
length `E`, and a prepared key skips the `16*L` term. The private operation
costs `32*L + 32*E + 8` at full width or `48*L + 32*E + 12` with CRT values,
and each attempt adds `16*L + 1`. Key preparation costs `16*L + 1`, CRT
validation `32*L + 1`, CRT derivation `48*L + 3`, and private-key validation
`TC_RSA_VALIDATE_WORK(bits, attempts)`.

## Raw operations

`TC_RSA_raw_public` and `TC_RSA_raw_private` apply RSAEP/RSAVP1 and RSADP/RSASP1
([RFC 8017, sections 5.1 and 5.2](https://www.rfc-editor.org/rfc/rfc8017.html#section-5))
to one caller-formatted representative. They add no padding, so the caller
selects and checks its protocol's encoding. The input has the modulus length
and must be less than the modulus, otherwise the result is `TC_RSA_INVALID`.
The output needs at least the modulus length. A shorter buffer returns
`TC_RSA_LIMIT`, and exactly the modulus length is written on `TC_RSA_OK`.
Allocate `TC_RSA_RAW_PUBLIC_WORKSPACE_WORDS(bits)` or
`TC_RSA_RAW_PRIVATE_WORKSPACE_WORDS(bits)` limbs.

The private operation takes the private exponent as a magnitude of 1 to
`L` bytes. It blinds the input with an RNG factor, bounded by
`execution.random_attempts`, and checks the result with the public exponent
before publishing it. A failed check returns `TC_RSA_ERROR`.

## C++11

`tiny_crypto::rsa_encode_v15_digest` and `tiny_crypto::rsa_encode_pss_digest`
accept `tiny_crypto::buffer` or a C array
whose size determines the representative length. Both overloads use the C
encoder and return `TC_RSA_result`.

Include `<tiny_crypto/rsa.hpp>`. `tiny_crypto::rsa_verify_v15_digest` calls the
same C verifier and returns `TC_RSA_result`. `rsa_workspace_for(words)` builds
a borrowed workspace view from a `TC_RSA_word` array, inferring its capacity.
Neither helper allocates memory or throws exceptions. Keep the array alive and
exclusive to the operation. Copies of a view share the same scratch.
The `rsa_prepare_public_key`, `rsa_verify_v15_prepared`,
`rsa_verify_pss_prepared`, and `rsa_prepared_public_key_clear` wrappers expose
the same caller-owned cache and borrowed-key lifetime as the C API.

`tiny_crypto::rsa_private_key` holds the same borrowed components as the C type.
`rsa_validate_private_key` takes references to the key, workspace, and
`rsa_execution`, and an exponent policy that defaults to
`TC_RSA_EXPONENT_FIPS`. The execution object groups the random source,
rejection limit, and remaining work.
The C++ key-generation wrappers expose the same caller-owned state and buffers:
`rsa_keygen_init`, `rsa_keygen_step`, and `rsa_keygen_clear`. They allocate no
memory and throw no exceptions.

`rsa_raw_public` and `rsa_raw_private` wrap the raw operations. Each takes the
output as `tiny_crypto::buffer` or as a C array whose size sets the capacity.
`rsa_workspace_words(operation, bits)` sizes a workspace, and
`rsa_modulus_supported` and `rsa_exponent_in_fips_range` return `bool`. The
work helpers `rsa_public_work`, `rsa_prepared_public_work`,
`rsa_private_work`, `rsa_encode_v15_work`, `rsa_encode_pss_work` and
`rsa_oaep_work` return the same units as their C functions.

```cpp
TC_RSA_word words[TC_RSA_RAW_PUBLIC_WORKSPACE_WORDS(2048)];
const tiny_crypto::rsa_workspace workspace = tiny_crypto::rsa_workspace_for(words);
uint8_t representative[256];
TC_work_budget work = {tiny_crypto::rsa_public_work(key)};
if (tiny_crypto::rsa_raw_public(key, signature, workspace, representative, work) != TC_RSA_OK)
  return false; /* representative is unchanged */
```

`TC_RSA_verify_pss_digest` verifies a precomputed digest using explicit message
and MGF hashes and salt length. Both hash implementations must be enabled.
It uses the same caller-owned limb workspace as v1.5 verification, plus a local
hash context and 64-byte digest buffer. Its work covers PSS hashing and mask
generation:

```c
TC_RSA_public_work(&key) + TC_RSA_encode_pss_work(&options, L)
```

The public RSA API provides v1.5 and PSS signing and signature verification,
OAEP encryption/decryption, and private-key component validation. The
[native X.509 provider](x509-crypto.md) uses RSA signature verification for
certificates and CMS. See [testing](testing.md) for the OpenSSL cross-checks.

## Key generation

`TC_RSA_keygen_init` and `TC_RSA_keygen_step` generate two-prime RSA-1024,
RSA-2048, RSA-3072, or RSA-4096 keys with public exponent 65537. The operation uses no
heap storage. Supply `TC_RSA_KEYGEN_WORKSPACE_WORDS(bits)` aligned limbs, or
query `TC_RSA_workspace_words(TC_RSA_OPERATION_KEYGEN, bits)`, plus
caller-owned output buffers.
The modulus and private exponent need `bits/8` bytes, each prime needs
`bits/16` bytes, and the exponent needs three bytes. A shorter output buffer
or workspace returns `TC_RSA_LIMIT`, and a NULL buffer or overlapping storage
returns `TC_RSA_ARGUMENT`. Both leave the state, outputs and workspace
unchanged.

Initialize `TC_RSA_keygen_state` to zero before its first use. State, scratch,
output metadata and output buffers must be mutually disjoint and remain alive
across steps. `TC_RSA_keygen_init` takes cumulative candidate and RNG-request
limits. Each `TC_RSA_keygen_step` also takes a work allowance. It returns
`TC_RSA_IN_PROGRESS` before exceeding that allowance. Call it again with the
same state and callbacks. `TC_RSA_KEYGEN_STEP_WORK(bits)` is enough for any one
pending unit to make progress. The units are a candidate, a Miller-Rabin
setup, a Miller-Rabin round and the final derivation of `n` and `d`. A
cancellation callback is checked between units. Cancellation returns
`TC_RSA_CANCELLED` and wipes retained secrets.

The generator follows FIPS 186-5 appendix A.1.1:

- It sets the top two bits of each prime, which exceeds the
  `sqrt(2) * 2^(nlen/2 - 1)` lower bound.
- It rejects small-prime factors and any prime with `p - 1` divisible by 65537.
- It requires `|p - q| > 2^(nlen/2 - 100)`.
- It performs 65 independent Miller-Rabin rounds on each accepted candidate.
- It sets `d = e^-1 mod LCM(p - 1, q - 1)` and generates new primes in the
  rare case `d <= 2^(nlen/2)`.

Residue checks on candidates, the prime-distance comparison, the GCD, the LCM
and the derivation of `d` run in time that depends only on the key size.
Candidate search itself has variable running time, which reveals only how
many candidates were rejected. Supply a cryptographic RNG whose callback
returns `TC_OK` only after filling the complete request.

Output is published only after both primes and `n`, `d` have been derived.
RNG errors and terminal limits preserve every output buffer and wipe retained
candidates. Call `TC_RSA_keygen_clear` after success or when abandoning an
in-progress operation. Generated components use fixed-width unsigned big-endian
encodings and can be placed directly in `TC_RSA_private_key` views. Validate or
test the generated key before provisioning it to persistent storage.

## Private-key validation

Include `<tiny_crypto/key.h>` for PKCS #8 RSA import. Enable
`TINY_CRYPTO_ENABLE_DER=ON`. The import reader also works with X.509 disabled.
`TC_KEY_rsa_private_read` returns a `TC_KEY_rsa_private_key` containing borrowed
components, attributes and algorithm parameters. An optional public key must
match the private key's modulus and exponent. An output that overlaps the
encoding returns `TC_TLV_ARGUMENT`.

Before signing, pass the selected `TC_signature_algorithm` to
`TC_KEY_rsa_private_signature_check`. A `TC_KEY_RSA_PSS` key permits PSS only.
Encoded restrictions also constrain the digest, mask digest and minimum salt
length. A `TC_KEY_RSA` key permits both RSA signature schemes. Apply application
policy and check compiled algorithm support separately. Keep the imported view
and its source bytes unchanged throughout validation and use.

`TC_DER_private_key_info` reads DER PKCS #8 containers, including the optional
public key in [RFC 5958](https://www.rfc-editor.org/rfc/rfc5958.html#section-2).
Version 0 carries the private key. Version 1 also carries a public key.
It returns borrowed algorithm, key and optional attribute spans. Check the algorithm
OID and its parameters before passing the key span to an algorithm-specific
reader. Validate public/private key consistency before use. Attribute contents
require their own schema checks. Keep the complete
container buffer protected and stable while using these views.

`TC_DER_rsa_private` in `<tiny_crypto/der.h>` reads a DER-encoded PKCS #1
two-prime private key. It returns borrowed magnitudes for all eight components,
including the CRT exponents and coefficient. Keep the encoded buffer stable and
the output object separate from it. Parsing checks structure and integer encoding.
Mathematical validation is a separate step. Multi-prime version 1 returns
`TC_TLV_UNSUPPORTED`. The format is defined in
[RFC 8017, Appendix A.1.2](https://www.rfc-editor.org/rfc/rfc8017.html#appendix-A.1.2).

The compiled [key-loading example](../examples/rsa_read.c) has explicit PKCS #1
and PKCS #8 entry points. Both validate the key's factors and CRT fields, then
sign using the same caller-owned workspace. The PKCS #8 entry point checks whether
the key permits v1.5 signatures.
`example_sign_rsa_pkcs8_pss_sha256` follows the same workflow with PSS,
SHA-256/MGF1-SHA-256 and a 32-byte salt. It checks those choices against the
imported key's restrictions before validation or RNG use.
Its key components borrow the DER buffer. For repeated signing, retain the parsed
key and validate it once while its bytes remain unchanged. The example sets
`TC_RSA_private_key.crt` to the validated CRT values, so signing uses the CRT kernel.

`TC_RSA_validate_private_key` checks a borrowed `TC_RSA_private_key`. Its public
modulus and exponent use the encodings described above. `d`, `p`, and `q` are
nonempty unsigned magnitudes of at most the modulus length. Leading zeros are
accepted. Keep all five components stable while
validation runs. Allocate `TC_RSA_VALIDATE_WORKSPACE_WORDS(bits)` limbs, or use
`TC_RSA_workspace_words(TC_RSA_OPERATION_VALIDATE, bits)` for a runtime size.

Validation checks `n = p*q` with distinct odd factors of half the modulus
width, then the FIPS 186-5 appendix A.1.1 criteria:

- `sqrt(2) * 2^(nlen/2 - 1) <= p, q`, checked exactly as `p^2 >= 2^(nlen - 1)`.
- `|p - q| > 2^(nlen/2 - 100)`.
- `2^(nlen/2) < d < LCM(p - 1, q - 1)` and `e*d = 1 mod LCM(p - 1, q - 1)`.
- The public exponent range selected by `TC_RSA_exponent_policy`.

`TC_RSA_EXPONENT_FIPS` requires an odd `2^16 < e < 2^256`, which
`TC_RSA_exponent_in_fips_range` tests. `TC_RSA_EXPONENT_ANY_ODD` accepts any
odd `3 <= e < n` for keys outside FIPS 186-5, such as test vectors with
`e = 3`. Every other criterion applies under both policies. The GCD, LCM and
comparisons on secret values run in time that depends only on the key size.
Each factor then receives 65 Miller-Rabin rounds at the factor width. Three is
handled exactly. Supply an independent cryptographically secure random source
and a request limit of at least `TC_RSA_VALIDATION_ROUNDS` per factor.
Rejection sampling can require extra requests. `TC_RSA_LIMIT` reports exhausted
requests, work, or storage. `TC_RSA_ERROR` reports RNG failure.

Validation returns `TC_RSA_INVALID` at the first failed public check or
structural check. These are a public exponent outside the selected policy, a
factor of another width than half the modulus, a private exponent that is zero,
even or at least `n`, an even, equal or unit factor, and `n != p*q`. The
remaining FIPS 186-5 criteria are accumulated as masks without branching.
Miller-Rabin stops at the first factor found composite. The time of an early
return can reveal which check rejected the key. Each of those keys is invalid.

For a fixed composite candidate, the Miller-Rabin bound is `4^-rounds`.
The 65-round policy gives a conservative combined bound of `2^-129` across two
factors, assuming independent uniform bases. This uses the validation bound
discussed in [FIPS 186-5 Appendix C.1][fips1865]. Key-strength policy, provenance,
and authorization require application checks.

For a `bits`-bit key and at most `A` requests per factor,
`TC_RSA_VALIDATE_WORK(bits, A)` is the work budget validation requires. It is
checked in full before any arithmetic or RNG request, so a smaller budget
returns `TC_RSA_LIMIT` with the budget, workspace and RNG unchanged. Evaluate
it with `uint32_t` operands, including on 16-bit targets. Workspace must be
aligned and separate from key bytes and metadata. The RNG context must also be separate
from those ranges. Validation wipes used workspace before returning.

The compiled [validation example](../examples/rsa_validate.c) shows workspace
setup and budget calculation. Its [header](../examples/rsa_validate.h) exposes
`example_validate_rsa_key`. Allocate a `TC_RSA_word` array with
`TC_RSA_VALIDATE_WORKSPACE_WORDS(bits)` entries, keep it outside a small task
stack, and pass its pointer and element count with the key and RNG callback.
The example allows four requests per required round. Handle `TC_RSA_LIMIT`
as an incomplete validation and accept the components only on `TC_RSA_OK`.

The private-operation tests compile this example and exercise its successful
validation path using OpenSSL-generated keys.

## Signing a digest

For imported CRT components, first validate the private key, then call
`TC_RSA_validate_crt` with `TC_RSA_crt` containing `dp`, `dq` and `q_inverse`.
The check compares the reduced exponents and verifies the coefficient and its
range. Allocate `TC_RSA_CRT_WORKSPACE_WORDS(bits)` limbs or query
`TC_RSA_workspace_words(TC_RSA_OPERATION_CRT, bits)`. Existing validation workspace can be reused.
Keep it separate from key bytes and metadata. Used scratch is wiped.
The work is `32*modulus_bytes+1`.
The C++ wrapper is `tiny_crypto::rsa_validate_crt`.

Generated or imported keys that contain only `n`, `e`, `d`, `p`, and `q` can
derive the remaining values with `TC_RSA_derive_crt` or
`tiny_crypto::rsa_derive_crt`. The three caller-owned outputs each need half the
modulus length. They are published together only after all computations succeed.
A shorter output returns `TC_RSA_LIMIT`. The CRT validation workspace can be
reused, and the work is `48*modulus_bytes+3`.

`TC_RSA_sign_v15_digest` signs a precomputed SHA digest using PKCS#1 v1.5.
Validate the private components before signing and keep them unchanged while
in use. Allocate `TC_RSA_SIGN_WORKSPACE_WORDS(bits)` limbs or query
`TC_RSA_workspace_words(TC_RSA_OPERATION_SIGN, bits)`. The signature buffer needs at
least the modulus length and must be separate from the key, digest, metadata and
workspace. A shorter buffer returns `TC_RSA_LIMIT`, and exactly the modulus
length is written.

Supply a `TC_RSA_execution` containing a cryptographically secure random source,
a blinding-attempt limit, and a work budget. The operation checks its result
using the public exponent before publishing signature bytes, and a failed
check returns `TC_RSA_ERROR`. Failures preserve the signature buffer.
Used workspace is wiped. For `A` attempts the work is
`TC_RSA_private_work(&key, A) + TC_RSA_encode_v15_work(&options, L)`.
Use overflow-checked arithmetic for application-selected limits.

The compiled [v1.5 signing example](../examples/rsa_sign.c) builds a workspace
view over caller-owned storage and budgets four blinding attempts. Its
[header](../examples/rsa_sign.h) declares `example_sign_rsa_v15_digest`.
Allocate `TC_RSA_SIGN_WORKSPACE_WORDS(bits)` limbs and keep the signature buffer
separate. Validate the key when loading it, then sign with the unchanged
components. Handle each result before sending or storing the signature.

The C++ wrapper is `tiny_crypto::rsa_sign_v15_digest`. Both APIs accept a digest
directly, so the corresponding hash implementation can be disabled.

After CRT validation, assign its borrowed view to `TC_RSA_private_key.crt`.
`TC_RSA_sign_v15_digest` and its C++ wrapper then select the accelerated private
kernel without changing PKCS #1 encoding. The CRT kernel blinds modulo `n`, performs
the two half-width exponentiations, recombines, unblinds, and verifies the result
with the public exponent before publishing it. `TC_RSA_private_work` selects
the CRT cost when `crt` is set.

`TC_RSA_sign_pss_digest` and `tiny_crypto::rsa_sign_pss_digest` take a
`TC_RSA_pss_options` value containing the message hash, MGF hash, and salt
length. Both hash implementations
must be enabled. They use the same signing workspace. Salt occupies temporary
scratch while the encoded message is built, then that storage is reused for
blinded exponentiation. A nonempty salt adds one RNG request.
`execution.random_attempts` bounds blinding requests.
The optional `TC_RSA_private_key.crt` view selects the same CRT acceleration.
The work is the sum below, plus 1 for a nonempty salt:

```c
TC_RSA_private_work(&key, A) + TC_RSA_encode_pss_work(&options, L)
```

The salt length is at most `L - H - 2` for message-hash length `H`, and a
longer salt returns `TC_RSA_INVALID`.

## OAEP encryption

`TC_RSA_encrypt_oaep` takes a public key, `TC_RSA_oaep_options`, and plaintext.
The options contain the message hash, MGF hash, and borrowed label. Both hashes
must be enabled. The message can contain up to
`modulus_bytes - 2*hash_digest_bytes - 2` bytes. Use `{NULL, 0}` for an empty
message or label.

Allocate `TC_RSA_ENCRYPT_WORKSPACE_WORDS(bits)` limbs, or query
`TC_RSA_workspace_words(TC_RSA_OPERATION_ENCRYPT, bits)`. `TC_RSA_execution` supplies the RNG and
shared work budget. Its `random_attempts` field is unused because OAEP encryption
requests one seed.
Ciphertext storage needs at least the modulus length, and a shorter buffer
returns `TC_RSA_LIMIT`. Exactly the modulus length is written, only on
`TC_RSA_OK`. Keep it and scratch separate from the key, plaintext, label and
metadata. Keep RNG state separate from those buffers too. Used scratch is wiped,
including when the RNG fails or the work budget runs out.

The C++ wrapper is `tiny_crypto::rsa_encrypt_oaep`. Encryption follows
[RFC 8017, section 7.1.1](https://www.rfc-editor.org/rfc/rfc8017.html#section-7.1.1).

The compiled [SHA-256 example](../examples/rsa_encrypt.c) calculates the work
budget and calls this API with your public key, RNG and scratch buffer. It uses
SHA-256 for both hashes and checks budget overflow before requesting randomness.

The work is `1 + TC_RSA_oaep_work(&options, L) + TC_RSA_public_work(&key)`:
the seed request, the encoding with both masks, and the exponentiation. The
whole budget is checked before the seed request, so a smaller budget returns
`TC_RSA_LIMIT` without using randomness. Ciphertext remains unchanged.

## OAEP decryption

`TC_RSA_decrypt_oaep` decrypts with a validated, unchanged private key. Supply
the message hash, MGF hash, and label in `TC_RSA_oaep_options`. Both hashes must
be enabled.
An empty label is `{NULL, 0}`. Ciphertext length must equal the modulus length.
Provide `TC_RSA_DECRYPT_WORKSPACE_WORDS(bits)` limbs, or query
`TC_RSA_workspace_words(TC_RSA_OPERATION_DECRYPT, bits)`. The work is
`TC_RSA_private_work(&key, A) + TC_RSA_oaep_work(&options, L)`.

Allocate at least `modulus_bytes - 2*hash_digest_bytes - 2` bytes for the
plaintext, the largest message the key and hash can carry. A smaller buffer
returns `TC_RSA_LIMIT` before the private-key operation. That check and the
work check use only public sizes, draw no randomness and consume no work, so
the status never distinguishes valid from invalid padding (RFC 8017 section
7.1.2).

Plaintext is checked in scratch and copied to the output after OAEP decoding
succeeds. The plaintext buffer and returned length change only on `TC_RSA_OK`.
Wrong labels and invalid padding return `TC_RSA_INVALID`. Temporary plaintext
and arithmetic scratch are wiped on return.

Keep ciphertext, label, key components and metadata separate from the output,
length object, workspace and RNG state. The C++ wrapper,
`tiny_crypto::rsa_decrypt_oaep`, takes the output length by reference.
Validated CRT values can be borrowed through `TC_RSA_private_key.crt`. OAEP
decoding and output behavior are shared with the full-width path.

Private exponentiation processes padded exponent widths with fixed loop counts,
and arithmetic selection avoids secret-indexed memory. RNG rejection sampling
has a variable attempt count, and component encodings expose their public byte
lengths. Constant-time behavior depends on the compiler and target and has no
independent certification.

[fips1865]: https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.186-5.pdf
