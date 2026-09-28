<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# RSA

Enable `TINY_CRYPTO_ENABLE_RSA=ON` and include `<tiny_crypto/rsa.h>`.
`TC_RSA_verify_v15_digest` verifies a precomputed SHA-1, SHA-224, SHA-256,
SHA-384 or SHA-512 digest using PKCS#1 v1.5. Supported modulus sizes are
1024, 2048 and 3072 bits. The caller decides which sizes and hashes its protocol
accepts and applies its trust policy separately.

Pass modulus and exponent as unsigned big-endian bytes, without ASN.1 INTEGER
sign padding. Signature length must match the modulus length. The digest length
must match the selected hash. Compute the message digest before calling the
verifier and validate certificate chains separately.

`TC_DER_rsa_public` in `<tiny_crypto/der.h>` reads a PKCS #1 `RSAPublicKey`
and returns borrowed modulus and exponent magnitudes with sign padding removed.
It checks integer encoding and positivity. Apply key-strength policy and use
the RSA API to check arithmetic constraints. X.509 key parsing uses this reader.

Allocate `TC_RSA_word` storage using the limb count returned by
`TC_RSA_verify_workspace_words(bits)`. The function returns zero for unsupported
sizes. Keep the workspace separate from input bytes and metadata. Verification
wipes used scratch; failures before arithmetic leave scratch unused.

Pass the hash in `TC_RSA_v15_options`. `TC_work_budget.remaining` bounds the
count of modular operations and encoding comparisons and is reduced by work
performed, including work before an invalid signature is detected.
For this verifier, a sufficient arithmetic budget is
`17 * modulus_bytes + 16 * exponent_bytes + 4`. Check the result explicitly:
`TC_RSA_OK`, `TC_RSA_INVALID`, `TC_RSA_LIMIT`, `TC_RSA_ARGUMENT`, or
`TC_RSA_UNSUPPORTED`. Only `TC_RSA_OK` accepts the signature.

`TINY_CRYPTO_RSA_SMALL=ON` selects byte limbs. Native builds otherwise use
32-bit limbs; AVR uses byte limbs. RSA verification does not require EC or a
hash implementation when the caller supplies the digest.

## Encoding for card and hardware signing

For card or hardware signing, `TC_RSA_encode_v15_digest` produces the complete
EMSA-PKCS1-v1_5 representative from a precomputed digest. Supply 128, 256 or
384 output bytes for RSA-1024, RSA-2048 or RSA-3072 and a work budget covering
that length. Input and output must be disjoint. Failures preserve the output.
The function shares the software signer's encoding implementation and requires
no hash context or RSA workspace. See [card-key authentication](credential-reader.md#card-key-authentication)
for an example that submits this representative to a card.

`TC_RSA_encode_pss_digest` builds an EMSA-PSS representative with explicit
message and MGF hashes and caller-supplied salt. Enable both hashes and obtain
the salt from a cryptographic random source. The salt length must match the
options. Output capacity is 128, 256 or 384 bytes, with `emBits` one less than
the modulus width in bits. Keep output and the work budget separate from all
inputs. Preflight errors preserve both; a failure during encoding wipes the
output and consumes work. The card transport submits the resulting bytes to
the private-key operation.

## C++11

`tiny_crypto::rsa_encode_v15_digest` and `tiny_crypto::rsa_encode_pss_digest`
accept `tiny_crypto::buffer` or a C array
whose size determines the representative length. Both overloads use the C
encoder and return `TC_RSA_result`.

Include `<tiny_crypto/rsa.hpp>`. `tiny_crypto::rsa_verify_v15_digest` calls the
same C verifier and returns `TC_RSA_result`. `rsa_workspace_for(words)` builds
a borrowed workspace view from a `TC_RSA_word` array, inferring its capacity.
Neither helper allocates memory or throws exceptions. Keep the array alive and
exclusive to the operation; copying a view does not create independent scratch.

`tiny_crypto::rsa_private_key` holds the same borrowed components as the C type.
`rsa_validate_private_key` takes references to the key, workspace, and
`rsa_execution`. The execution object groups the random source, rejection
limit, and remaining work.
The C++ key-generation wrappers expose the same caller-owned state and buffers:
`rsa_keygen_init`, `rsa_keygen_step`, and `rsa_keygen_clear`. They allocate no
memory and do not throw.

`TC_RSA_verify_pss_digest` verifies a precomputed digest using explicit message
and MGF hashes and salt length. Both hash implementations must be enabled.
It uses the same caller-owned limb workspace as v1.5 verification, plus a local
hash context and 64-byte digest buffer. Its work budget also covers PSS hashing
and mask generation; the v1.5 budget formula does not apply to PSS.

The public RSA API provides v1.5 and PSS signing and signature verification,
OAEP encryption/decryption, and private-key component validation. The
[native X.509 provider](x509-crypto.md) uses RSA signature verification for
certificates and CMS. See [testing](testing.md) for the OpenSSL cross-checks.

## Key generation

`TC_RSA_keygen_init` and `TC_RSA_keygen_step` generate two-prime RSA-1024,
RSA-2048 or RSA-3072 keys with public exponent 65537. The operation uses no
heap storage. Supply `TC_RSA_KEYGEN_WORKSPACE_WORDS(bits)` aligned limbs, or
query `TC_RSA_keygen_workspace_words(bits)`, plus caller-owned output buffers.
The modulus and private exponent need `bits/8` bytes, each prime needs
`bits/16` bytes, and the exponent needs three bytes.

Initialize `TC_RSA_keygen_state` to zero before its first use. State, scratch,
output metadata and output buffers must be mutually disjoint and remain alive
across steps. `TC_RSA_keygen_init` takes cumulative candidate and RNG-request
limits. Each `TC_RSA_keygen_step` also takes a work allowance. It returns
`TC_RSA_IN_PROGRESS` before exceeding that allowance; call it again with the
same state and callbacks. `TC_RSA_KEYGEN_STEP_WORK(bits)` is enough for any one
pending unit to make progress. A cancellation callback is checked between
bounded candidate, setup and Miller-Rabin units. Cancellation returns
`TC_RSA_CANCELLED` and wipes retained secrets.

The generator sets the top two bits of each prime, rejects small-prime factors,
checks compatibility with exponent 65537, enforces the FIPS 186-5 prime-distance
condition, and performs 65 independent Miller-Rabin rounds on each accepted
candidate. Candidate search has variable running time. Supply a cryptographic
RNG whose callback returns `TC_OK` only after filling the complete request.

Output is published only after both primes and `n`, `d` have been derived.
RNG errors and terminal limits preserve every output buffer and wipe retained
candidates. Call `TC_RSA_keygen_clear` after success or when abandoning an
in-progress operation. Generated components use fixed-width unsigned big-endian
encodings and can be placed directly in `TC_RSA_private_key` views. Validate or
test the generated key before provisioning it to persistent storage.

## Private-key validation

Include `<tiny_crypto/key.h>` for PKCS #8 RSA import. Enable
`TINY_CRYPTO_ENABLE_DER=ON`; the import reader also works with X.509 disabled.
`TC_KEY_rsa_private_read` returns a `TC_KEY_rsa_private_key` containing borrowed
components, attributes and algorithm parameters. An optional public key must
match the private key's modulus and exponent.

Before signing, pass the selected `TC_signature_algorithm` to
`TC_KEY_rsa_private_signature_check`. A `TC_KEY_RSA_PSS` key permits PSS only;
encoded restrictions also constrain the digest, mask digest and minimum salt
length. A `TC_KEY_RSA` key permits both RSA signature schemes. Apply application
policy and check compiled algorithm support separately. Keep the imported view
and its source bytes unchanged throughout validation and use.

`TC_DER_private_key_info` reads DER PKCS #8 containers, including the optional
public key in [RFC 5958](https://www.rfc-editor.org/rfc/rfc5958.html#section-2).
Version 0 carries the private key; version 1 also carries a public key.
It returns borrowed algorithm, key and optional attribute spans. Check the algorithm
OID and its parameters before passing the key span to an algorithm-specific
reader. Validate public/private key consistency before use. Attribute contents
require their own schema checks. Keep the complete
container buffer protected and stable while using these views.

`TC_DER_rsa_private` in `<tiny_crypto/der.h>` reads a DER-encoded PKCS #1
two-prime private key. It returns borrowed magnitudes for all eight components,
including the CRT exponents and coefficient. Keep the encoded buffer stable and
the output object separate from it. Parsing checks structure and integer encoding;
mathematical validation is a separate step. Multi-prime version 1 returns
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
key and validate it once while its bytes remain unchanged. The signing operation
uses full-width exponentiation with `n`, `e`, `d`, `p`, and `q`.

`TC_RSA_validate_private_key` checks a borrowed `TC_RSA_private_key`. Its public
modulus and exponent use the encodings described above. `d`, `p`, and `q` are
nonempty unsigned magnitudes of at most the modulus length. Leading zeros are
accepted. Keep all five components stable while
validation runs. Allocate `TC_RSA_VALIDATE_WORKSPACE_WORDS(bits)` limbs, or use
`TC_RSA_validate_workspace_words(bits)` for a runtime size.

Validation checks `n = p*q`, distinct odd factors, the private-exponent range,
and `e*d = 1` modulo each factor minus one. Each factor receives 65 Miller-Rabin
rounds; three is handled exactly. Supply an independent cryptographically secure
random source and a request limit of at least `TC_RSA_VALIDATION_ROUNDS` per
factor. Rejection sampling can require extra requests. `TC_RSA_LIMIT` reports
exhausted requests, work, or storage; `TC_RSA_ERROR` reports RNG failure.

For a fixed composite candidate, the Miller-Rabin bound is `4^-rounds`.
The 65-round policy gives a conservative combined bound of `2^-129` across two
factors, assuming independent uniform bases. This uses the validation bound
discussed in [FIPS 186-5 Appendix C.1][fips1865]. Key-strength policy, provenance,
and authorization require application checks.

For modulus length `L` bytes and at most `A` requests per factor, a sufficient
work budget is `32*L + 2 + 2*(24*L + 3 + 65*(24*L + 1) + A)`.
Use `uint32_t` arithmetic for this budget, including on 16-bit targets, and check
for overflow when calculating it. Workspace must be aligned
and separate from key bytes and metadata. The RNG context must also be separate
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

[fips1865]: https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.186-5.pdf

## Signing a digest

For imported CRT components, first validate the private key, then call
`TC_RSA_validate_crt` with `TC_RSA_crt` containing `dp`, `dq` and `q_inverse`.
The check compares the reduced exponents and verifies the coefficient and its
range. Allocate `TC_RSA_CRT_WORKSPACE_WORDS(bits)` limbs or query
`TC_RSA_crt_workspace_words(bits)`. Existing validation workspace can be reused.
Keep it separate from key bytes and metadata; used scratch is wiped.
A sufficient work budget is `32*modulus_bytes+1`.
The C++ wrapper is `tiny_crypto::rsa_validate_crt`.

Generated or imported keys that contain only `n`, `e`, `d`, `p`, and `q` can
derive the remaining values with `TC_RSA_derive_crt` or
`tiny_crypto::rsa_derive_crt`. The three caller-owned outputs each need half the
modulus length. They are published together only after all computations succeed.
The CRT validation workspace can be reused, and a sufficient work budget is
`48*modulus_bytes+3`.

`TC_RSA_sign_v15_digest` signs a precomputed SHA digest using PKCS#1 v1.5.
Validate the private components before signing and keep them unchanged while
in use. Allocate `TC_RSA_SIGN_WORKSPACE_WORDS(bits)` limbs or query
`TC_RSA_sign_workspace_words(bits)`. The signature buffer must have exactly
the modulus length and be separate from the key, digest, metadata and workspace.

Supply a `TC_RSA_execution` containing a cryptographically secure random source,
a blinding-attempt limit, and a work budget. The operation checks its result
using the public exponent
before publishing signature bytes. Failures preserve the signature buffer;
used workspace is wiped. A sufficient work budget for `A` attempts is
`33*modulus_bytes + 32*exponent_bytes + 8 + A*(16*modulus_bytes + 1)`.
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
with the public exponent before publishing it. For `A` blinding attempts, its
v1.5 budget is
`49*modulus_bytes + 32*exponent_bytes + 12 + A*(16*modulus_bytes + 1)`.

`TC_RSA_sign_pss_digest` and `tiny_crypto::rsa_sign_pss_digest` take a
`TC_RSA_pss_options` value containing the message hash, MGF hash, and salt
length. Both hash implementations
must be enabled. They use the same signing workspace. Salt occupies temporary
scratch while the encoded message is built, then that storage is reused for
blinded exponentiation. A nonempty salt adds one RNG request;
`execution.random_attempts` bounds blinding requests. PSS hashing and mask
generation also consume work,
so the v1.5 budget formula covers only v1.5 signing.
The optional `TC_RSA_private_key.crt` view selects the same CRT acceleration.

For a PSS budget, let `L` and `E` be the modulus and exponent lengths, `H` and
`G` the message-hash and MGF digest lengths, and `S` the salt length, all in bytes.
With at most `A` blinding attempts, calculate:

```text
db = L - H - 1
blocks = ceil(db / G)
encoding = L + H + S + 9 + db + blocks * (H + 5)
private_operation = 32*L + 32*E + 8 + A*(16*L + 1)
work = encoding + private_operation + (S != 0)
```

Check the salt limit `S <= L - H - 2` and use overflow-checked arithmetic.

## OAEP encryption

`TC_RSA_encrypt_oaep` takes a public key, `TC_RSA_oaep_options`, and plaintext.
The options contain the message hash, MGF hash, and borrowed label. Both hashes
must be enabled. The message can contain up to
`modulus_bytes - 2*hash_digest_bytes - 2` bytes. Use `{NULL, 0}` for an empty
message or label.

Allocate `TC_RSA_ENCRYPT_WORKSPACE_WORDS(bits)` limbs, or query
`TC_RSA_encrypt_workspace_words(bits)`. `TC_RSA_execution` supplies the RNG and
shared work budget. Its `random_attempts` field is unused because OAEP encryption
requests one seed.
Ciphertext storage must have exactly the modulus length and changes only on
`TC_RSA_OK`. Keep it and scratch separate from the key, plaintext, label and
metadata. Keep RNG state separate from those buffers too. Used scratch is wiped,
including when the RNG fails or the work budget runs out.

The C++ wrapper is `tiny_crypto::rsa_encrypt_oaep`. Encryption follows
[RFC 8017, section 7.1.1](https://www.rfc-editor.org/rfc/rfc8017.html#section-7.1.1).

The compiled [SHA-256 example](../examples/rsa_encrypt.c) calculates the work
budget and calls this API with your public key, RNG and scratch buffer. It uses
SHA-256 for both hashes and checks budget overflow before requesting randomness.

For modulus length `L`, exponent length `E`, label length `A`, message-hash
length `H` and MGF-hash length `G`, all in bytes, a sufficient work budget is:

```text
D = L - H - 1
1 + (L + A + 1)
  + D + ceil(D/G)*(H + 5)
  + H + ceil(H/G)*(D + 5)
  + 16*L + 16*E + 4
```

The terms cover the RNG request, encoding, two masks and exponentiation.
Use overflow-checked arithmetic when calculating the budget. A smaller budget
can return `TC_RSA_LIMIT` after consuming randomness; ciphertext remains unchanged.

## OAEP decryption

`TC_RSA_decrypt_oaep` decrypts with a validated, unchanged private key. Supply
the message hash, MGF hash, and label in `TC_RSA_oaep_options`; both hashes must
be enabled.
An empty label is `{NULL, 0}`. Ciphertext length must equal the modulus length.
Provide `TC_RSA_DECRYPT_WORKSPACE_WORDS(bits)` limbs, or query
`TC_RSA_decrypt_workspace_words(bits)`.

Plaintext is checked in scratch and copied to the output after OAEP decoding
succeeds. The plaintext buffer and returned length change only on `TC_RSA_OK`.
Wrong labels and invalid padding return `TC_RSA_INVALID`; insufficient plaintext
capacity returns `TC_RSA_LIMIT`. Temporary plaintext and arithmetic scratch are
wiped on return. Allocate up to `modulus_bytes - 2*hash_digest_bytes - 2` bytes
for the output, or use a smaller buffer when the protocol bounds the message.

Keep ciphertext, label, key components and metadata separate from the output,
length object, workspace and RNG state. The C++ wrapper,
`tiny_crypto::rsa_decrypt_oaep`, takes the output length by reference.
Validated CRT values can be borrowed through `TC_RSA_private_key.crt`; OAEP
decoding and output behavior are shared with the full-width path.

Private exponentiation processes padded exponent widths with fixed loop counts,
and arithmetic selection avoids secret-indexed memory. RNG rejection sampling
has a variable attempt count, and component encodings expose their public byte
lengths. This implementation has not been independently certified as a
constant-time implementation on every compiler and target.
