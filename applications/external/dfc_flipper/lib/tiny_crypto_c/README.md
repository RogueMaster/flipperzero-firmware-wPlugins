<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# tiny_crypto_c

[![CI](https://github.com/mistial-dev/tiny_crypto_c/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/mistial-dev/tiny_crypto_c/actions/workflows/ci.yml)
[![License: GPL v2+](https://img.shields.io/badge/license-GPL--2.0--or--later-blue.svg)](LICENSE)

tiny_crypto_c provides small, portable cryptographic primitives for embedded C
and C++. Callers supply all memory. The C++11 wrappers use pointer-length pairs
and C arrays, work without the standard library or exceptions, and preserve the
C API's result types. Disabled algorithms and modes are left out of the build.

The default profile enables **AES-128 CTR and SHA-256**. DES, 3DES, SHA-1,
SHA-224, SHA-384, SHA-512, HMAC, KMAC256, the NIST SP 800-108 key-based KDF,
and other block-cipher modes can be enabled as needed.

## Build

```sh
make
make test
make test-full       # NIST CAVP and Wycheproof corpora
make test-sanitize   # address + undefined behavior sanitizers
make benchmark
make size
```

Make passes all `TINY_CRYPTO_*` command-line variables to CMake:

```sh
make TINY_CRYPTO_ENABLE_HMAC=ON TINY_CRYPTO_AES_GCM=ON
```

Or build with CMake directly:

```sh
cmake -S . -B build -DTINY_CRYPTO_ENABLE_HMAC=ON -DTINY_CRYPTO_AES_GCM=ON
cmake --build build
```

As a CMake dependency:

```cmake
add_subdirectory(path/to/tiny_crypto_c)
target_link_libraries(firmware PRIVATE tiny_crypto_c::tiny_crypto_c)
```

Public headers are in `src/tiny_crypto/`, following the Arduino library layout:

```c
#include <tiny_crypto/tiny_crypto.h>
```

Use `<tiny_crypto/tiny_crypto.hpp>` for the C++11 wrappers in the
`tiny_crypto` namespace.

## Configuration

All options are CMake cache variables. The defaults enable AES-128 CTR and
SHA-256, with memory wiping and pointer checks turned on.

`TINY_CRYPTO_RESOURCE_PROFILE=micro` favors small code and byte-limb EC
arithmetic. `mini` uses native arithmetic while keeping optional algorithms
off. `desktop` enables the supported capabilities, including legacy algorithms
and both PIV secure-messaging suites. Use this profile when broad compatibility
is required, and set application policy to restrict legacy algorithms.

`TINY_CRYPTO_TARGET=piv-acu` or `piv-pd` selects a fixed role-specific algorithm
set independently of resource tuning. See [PIV targets and ESP32-P4](docs/esp32-p4.md)
for the role requirements and ESP-IDF builds.

Feature options accept `AUTO`, `ON`, or `OFF`. `AUTO` follows the selected
profile; explicit settings survive a profile change. Direct-source builds
select `TC_RESOURCE_PROFILE=TC_RESOURCE_MICRO`, `TC_RESOURCE_MINI`, or
`TC_RESOURCE_DESKTOP`. The defaults below describe a build with no profile
selected.

| Option | Default | Meaning |
| --- | ---: | --- |
| `TINY_CRYPTO_ENABLE_AES` | ON | AES implementation |
| `TINY_CRYPTO_ENABLE_DES` | OFF | DES and 3DES implementation |
| `TINY_CRYPTO_ENABLE_SHA1` | OFF | SHA-1 implementation |
| `TINY_CRYPTO_ENABLE_SHA224` | OFF | SHA-224 (shares the SHA-256 core) |
| `TINY_CRYPTO_ENABLE_SHA256` | ON | SHA-256 implementation |
| `TINY_CRYPTO_ENABLE_SHA384` | OFF | SHA-384 (shares the SHA-512 core) |
| `TINY_CRYPTO_ENABLE_SHA512` | OFF | SHA-512 implementation |
| `TINY_CRYPTO_ENABLE_HMAC` | OFF | HMAC for enabled hashes |
| `TINY_CRYPTO_ENABLE_KMAC256` | OFF | Fixed-output KMAC256 with customization |
| `TINY_CRYPTO_ENABLE_TLV` | OFF | Bounded TLV readers and tree traversal |
| `TINY_CRYPTO_ENABLE_DER` | OFF | DER value helpers; requires TLV |
| `TINY_CRYPTO_ENABLE_X509` | OFF | X.509 certificate and public-key readers; requires DER |
| `TINY_CRYPTO_ENABLE_KEY_CHALLENGE` | OFF | Generic key proof-of-possession challenge; requires X.509 |
| `TINY_CRYPTO_ENABLE_X509_PATH` | OFF | X.509 path validation and stores; requires X.509 |
| `TINY_CRYPTO_ENABLE_X509_REVOCATION` | OFF | CRL parsing and path revocation; requires X.509 path support |
| `TINY_CRYPTO_ENABLE_PIV_OIDS` | OFF | Registered PIV and TWIC identifier classification |
| `TINY_CRYPTO_ENABLE_CMS` | OFF | CMS parsing and signature verification; requires X.509, BER, and PIV/TWIC identifiers |
| `TINY_CRYPTO_ENABLE_CMS_VALIDATION` | OFF | CMS/X.509 validation context, signer paths, and revocation; requires CMS and X.509 revocation |
| `TINY_CRYPTO_ENABLE_PIV_OBJECTS` | OFF | PIV and TWIC credential-object readers; requires CMS, TWIC UUID, and PIV/TWIC identifiers |
| `TINY_CRYPTO_ENABLE_CREDENTIAL` | OFF | Composed credential validation; requires PIV objects, CHUID, and CMS validation |
| `TINY_CRYPTO_ENABLE_PIV_CHUID` | OFF | PIV CHUID reader; requires TLV |
| `TINY_CRYPTO_ENABLE_PIV_CVC` | OFF | PIV secure messaging CVC reader; requires DER |
| `TINY_CRYPTO_ENABLE_EAC_CVC` | OFF | TR-03110 EAC CVC reader; requires DER |
| `TINY_CRYPTO_ENABLE_AAMVA` | OFF | ANSI AAMVA payload readers |
| `TINY_CRYPTO_ENABLE_FASCN` | OFF | FASC-N readers and writers |
| `TINY_CRYPTO_ENABLE_TWIC_UUID` | OFF | TWIC NEXGEN UUID helpers; requires FASC-N |
| `TINY_CRYPTO_ENABLE_TWIC_TPK` | OFF | TWIC privacy-key container reader; requires TLV |
| `TINY_CRYPTO_ENABLE_TWIC_OBJECT_CRYPTO` | OFF | TWIC private-object encryption; requires AES-128 ECB |
| `TINY_CRYPTO_TLV_BER` | OFF | ASN.1 BER, including constructed indefinite lengths; requires TLV |
| `TINY_CRYPTO_TLV_STREAM` | OFF | Incremental reader; requires TLV |
| `TINY_CRYPTO_ENABLE_KDF` | OFF | SP 800-108 KBKDF over the enabled HMAC and CMAC PRFs |
| `TINY_CRYPTO_ENABLE_SSKDF` | OFF | Hash-based single-step KDF with SHA-256 or SHA-384 |
| `TINY_CRYPTO_ENABLE_EC` | OFF | P-256 and P-384 ECDH and public-key generation |
| `TINY_CRYPTO_ENABLE_PIV_SM` | OFF | PD-side PIV secure messaging, CS2 and CS7 |
| `TINY_CRYPTO_AES_DYNAMIC` | OFF | Per-context AES-128/192/256 keys, CBC, and CMAC |
| `TINY_CRYPTO_ZEROIZE` | ON | Wipe contexts and stack secrets |
| `TINY_CRYPTO_STRICT` | ON | Validate public API pointers |
| `TINY_CRYPTO_AVR_PROGMEM` | ON | Keep constant tables out of AVR SRAM |

AES uses `TINY_CRYPTO_AES_KEY_BITS=128`, constant-time S-box access, and CTR by
default. `AES_CBC`, `AES_ECB`, `AES_OFB`, `AES_GCM`, `AES_CCM`,
`AES_EAX`, `AES_EAX_PRIME`, `AES_SIV`, and `AES_CMAC` enable individual modes
when prefixed with `TINY_CRYPTO_`. The S-box choices are `constant-time`,
`runtime`, and `fast`. GCM multiplication choices are `auto`, `bitwise`,
`wide`, `fast-table`, and `hardware`.

The non-constant-time `fast-table` mode adds 256 bytes to each GCM context.
Small MCUs can keep `TINY_CRYPTO_AES_TINY=ON` or use `auto`, `bitwise`, or
`wide` to avoid that RAM cost.

Enabling `TINY_CRYPTO_ENABLE_DES` also enables CTR and 3DES. `DES_ECB`, `DES_CBC`,
`DES_OFB`, `DES_CFB1`, `DES_CFB8`, `DES_CFB64`, and `DES_CMAC` select the
remaining modes when prefixed with `TINY_CRYPTO_`.
`TINY_CRYPTO_DES_REJECT_WEAK_KEYS=ON` rejects weak or semi-weak DES component
keys and TDEA bundles that collapse to single DES. It is off by default for
legacy-vector compatibility; firmware builds may instead define
`TC_DES_REJECT_WEAK_KEYS=1` directly.

SHA-1, SHA-224, and SHA-256 are implemented in `hash.c`. SHA-384 and SHA-512
share a 64-bit core in `sha512.c`. SHA-224 and SHA-384 reuse the compression
functions of SHA-256 and SHA-512, respectively, but each hash can be enabled
independently.

`TINY_CRYPTO_ENABLE_KDF` builds the SP 800-108 key-based key derivation
function in counter, feedback and double-pipeline mode. It needs at least one
PRF: HMAC with an enabled SHA digest, `TINY_CRYPTO_AES_CMAC`, or
`TINY_CRYPTO_DES_CMAC`. Each PRF gets its own function family
(`TC_KBKDF_HMAC_SHA256_counter`, `TC_KBKDF_AES_CMAC_feedback`, ...), so unused
PRFs compile out. AES-CMAC keys follow `TINY_CRYPTO_AES_KEY_BITS`; TDEA-CMAC is
kept for legacy interoperability only.

Run the host suite with every available GCC and Clang toolchain using
`make test-compilers`. Override the pairs when compiler names are versioned,
for example `make test-compilers TOOLCHAINS='gcc-15:g++-15 clang:clang++'`.

Projects that compile the C files directly can define the corresponding
`TC_*` macros documented in [`config.h`](src/tiny_crypto/config.h).

## API behavior

See [Working with the API](docs/api.md) for buffer lifetimes, workspace setup,
result handling and complete workflow guides.

Cryptographic operations that can fail return a `TC_status` code:

```c
TC_OK        /* success */
TC_MISMATCH  /* valid comparison or authentication failure */
TC_ERROR     /* malformed argument or invalid state */
```

Authentication checks examine the entire tag. One-shot GCM, CCM, and EAX
decryptors authenticate before writing plaintext. SIV writes candidate
plaintext to recompute its synthetic IV and wipes the output if it does not
match. Streaming GCM decryption writes
plaintext before `TC_AES_GCM_decrypt_finish` checks the tag. Do not use that
plaintext until the call returns `TC_OK`, and wipe it if any other status is
returned.

CTR, CBC, ECB, OFB, and CFB provide no authentication. Pair them with a MAC or
use an authenticated mode such as GCM, CCM, EAX, or SIV. Never reuse a CTR,
GCM, CCM, EAX, or OFB nonce with the same key.

DES has only a 56-bit effective key and exists for legacy interoperability.
Its table lookups are not designed to resist cache-timing attacks. 3DES also
belongs in compatibility code rather than new protocols.

SHA-1 remains available for compatibility. Do not use it for new
collision-resistant signatures or content identity. HMAC-SHA-1 is a separate
construction whose security does not rest on collision resistance; it remains
an acceptable MAC and KBKDF PRF, and is the smallest HMAC option on AVR.

For KBKDF, include the purpose, parties, and requested length in the fixed input
to distinguish keys derived for different uses. You can construct this input
with `TC_KBKDF_fixed_input` (`Label || 0x00 || Context || [L]_32`), and never
reuse a key-derivation key as a derived key. Output lengths are in bytes; a
derivation of `n = ceil(out_len / h)` PRF blocks needs `n <= 2^r - 1` for an
`r`-bit counter. Output buffers must not overlap any input, and `TC_ERROR`
wipes the output when derivation had already started.

## TLV parsing

Enable `TINY_CRYPTO_ENABLE_TLV` and include `<tiny_crypto/tlv.h>`:

```c
const uint8_t data[] = {0x30, 0x03, 0x02, 0x01, 0x2a};
const TC_TLV_limits limits = {4096, 4096, 256, 16};
TC_TLV_reader reader;
TC_TLV_element element;
TC_TLV_result result = TC_TLV_reader_init(&reader, data, sizeof data,
                                         TC_TLV_DER, &limits);
if (result == TC_TLV_OK) {
    result = TC_TLV_next(&reader, &element);
    /* element.value borrows data; use it only if result is TC_TLV_OK. */
}
```

The limits are input bytes, value bytes, element count, and nesting depth.
Zero means zero, not unlimited. The reader advances through siblings without
descending into their values. `TC_TLV_walk` checks nested containers using a
caller-provided frame array and one shared budget for the whole input.

Choose DER, ISO 7816, or optional ASN.1 BER explicitly. ISO padding has separate
profiles and is accepted only between root objects. `TC_TLV_read` and the
sibling reader handle definite lengths; use the walker or incremental reader
for indefinite BER. `TC_TLV_read_tree` reads one definite or indefinite object,
checks its constructed boundaries, and leaves following siblings unread.
See [TLV parsing](docs/tlv.md) for workspace setup and borrowed-span usage.
For SignedData envelopes and signed attributes, see [CMS parsing](docs/cms.md).

`TC_TLV_END` means the sibling reader is exhausted. `TC_TLV_MORE` means it needs
more input. Other results distinguish malformed input, resource
limits, unsupported features, and invalid arguments. Bounds checks remain on
with `TC_STRICT=0`.

Returned spans borrow the input. Don't reuse or modify that buffer while using
them. Incremental callbacks borrow bytes only during the callback; call
`TC_TLV_stream_finish` when the message ends to detect truncation. Discard a
stream after an error or reinitialize it for a new message.

`<tiny_crypto/der.h>` adds INTEGER, BIT STRING, OID, BOOLEAN, NULL, SEQUENCE,
and SET helpers. They take complete encoded values. Framing checks do not
validate an ASN.1 schema, a certificate, or its signature. C++11 code can use
`tiny_crypto::TLVReader` from `<tiny_crypto/tlv.hpp>`.

## Certificates and PIV objects

`TC_X509_read` reads one DER certificate using caller-owned scratch space:

```c
#include <tiny_crypto/x509.h>

TC_TLV_frame frames[16];
TC_bytes extension_oids[32];
TC_X509_workspace workspace = {frames, 16, extension_oids, 32};
const TC_TLV_limits limits = {8192, 8192, 1024, 16};
TC_X509_certificate certificate;
TC_TLV_result result = TC_X509_read(data, length, &limits, &workspace,
                                   &certificate);
```

Choose the limits for your application. Each extension needs one OID slot;
exceeding a limit returns `TC_TLV_LIMIT`. Results borrow the input buffer, so
keep it alive while using them. The workspace can be reused after the call.

`certificate.public_key` identifies the subject's algorithm, key size, and
named curve. `TC_X509_subject_public_key` also reads a standalone
SubjectPublicKeyInfo. The extension iterator exposes OIDs, critical flags,
and values; helpers decode Basic Constraints and Key Usage.

`<tiny_crypto/key_challenge.h>` prepares and verifies a fresh proof-of-possession
challenge from a validated public key and explicit signature parameters. It
contains no card commands or slot policy. Protocol code selects its algorithm,
key usage and transport identifiers before issuing a challenge.

`TC_PIV_CHUID_read` returns the FASC-N, card UUID (GUID), optional cardholder
UUID, expiration date, and signature. Select `TC_PIV_CHUID_CONTENTS` for the
object contents or `TC_PIV_CHUID_CONTAINER` for a `53`-wrapped object.
The default reader enforces the PIV field order and requires a nonempty
signature field. `TC_PIV_CHUID_read_profile` also accepts explicit
`TC_CHUID_PROFILE_TWIC_SIGNED` and `TC_CHUID_PROFILE_TWIC_UNSIGNED` profiles.
Unsigned TWIC omits the signature and cardholder UUID fields. Choose the
profile from the requested card object, not from the returned contents.

`TC_PIV_CVC_read` reads card and intermediate secure messaging CVCs as defined
in SP 800-73-5 Part 2, section 4.1.5. Its `signed_data` span contains the
original TLV bytes covered by the signature. `TC_PIV_CVC_chain_verify` checks
direct or intermediate issuer links and signatures under a validated content
signer, with suite and optional UUID binding. See [PIV CVC verification](docs/piv-cvc.md)
for trust prerequisites and workspace setup.

EAC certificates use a different schema. `<tiny_crypto/eac_cvc.h>` provides
`TC_EAC_CVC_read`, a standalone public-key reader, and an extension iterator.
The certificate reader takes `TC_TLV_limits` and a `TC_EAC_CVC_workspace`
containing caller-owned nesting frames. Returned fields borrow the input.
Its signed span includes the complete `7F4E` body, including tag and length.
Unknown extensions are preserved; unsupported key or authorization OIDs return
`TC_TLV_UNSUPPORTED`.

Call `TC_EAC_CVC_check_encoding` with the resolved issuer key and, for an EC
subject without explicit parameters, its inherited domain parameters. It
checks coordinate and signature widths; missing context returns
`TC_TLV_ARGUMENT`. Signature width comes from the issuer key, not the subject.
The standalone reader also accepts RI-ECDH public-key templates, but these
cannot be used as certificate-signing keys.

These readers parse encodings. Applications must separately verify signatures,
key validity, certificate trust, and expiration before using a credential.

## Benchmarks

We measure flash and static RAM usage for Arduino Uno and Raspberry Pi Pico 2
(RP2350, Arm Cortex-M33) builds. [docs/benchmarks.md](docs/benchmarks.md) lists
the sizes in bytes and as percentages of each board's flash and RAM capacity.
These are linked firmware sizes, not peak runtime memory measurements.

Run `make benchmark-report` to regenerate the report, or
`make benchmark-report-check` to check that it is up to date. Neither command
needs a connected board. Use `make benchmark` to measure throughput on the host
with the current build configuration.

## PIV secure messaging

`TINY_CRYPTO_ENABLE_PIV_SM` enables the peripheral-device side of CS2 and CS7.
The library handles ECDH, session-key derivation, confirmation, command
protection, response authentication, and CVC chain verification. Applications
supply APDU transport and an X.509 content-signing certificate accepted through
their trust, policy, time, and revocation checks.

The desktop profile enables its dependencies. For a smaller build, enable
AES with `TINY_CRYPTO_AES_DYNAMIC`, SHA-256, EC, SSKDF, TLV, DER, and PIV CVC
parsing. CS7 also needs SHA-384 and P-384; CS2 needs P-256. Disable a suite with
`TINY_CRYPTO_PIV_SM_CS2=OFF` or `TINY_CRYPTO_PIV_SM_CS7=OFF` and disable its
unused curve separately with `TINY_CRYPTO_EC_P256` or `TINY_CRYPTO_EC_P384`.

Start with a zero-initialized `TC_PIV_SM` and caller-owned
`TC_PIV_SM_workspace`. `TC_PIV_SM_begin` takes an RNG callback and returns the
GENERAL AUTHENTICATE APDU. Parse the response with `TC_PIV_SM_response_read`,
verify that CVC's signature and trust chain, then pass its authenticated public
key and the unchanged response to `TC_PIV_SM_finish`. With X.509 enabled,
`TC_PIV_SM_authenticate_response` combines CVC verification and key confirmation.
Its request takes the accepted content signer, optional intermediate CVC,
expected card UUID, parsing limits, signature provider, and transport result.

`TC_PIV_SM_wrap` produces a protected command data field. Fragment it afterward
if the transport requires chaining. Pass the reassembled response data and
outer status separately to `TC_PIV_SM_unwrap`. Only one command may be pending.
Peer authentication or framing errors clear the session; a short output buffer
allows the same response to be retried. Clear the session when the card is
removed or command delivery is uncertain.

The C++11 `tiny_crypto::piv_sm` wrapper clears its session on destruction and
cannot be copied or moved. Both APIs keep workspace outside the session so it
can be reused between operations.

The underlying `TC_ECDH`, `TC_EC_public_key`, and `TC_EC_validate_public_key`
APIs take fixed-width scalars and uncompressed SEC1 public keys. They support
P-256 and P-384 without heap allocation. `TC_SSKDF_SHA256` and
`TC_SSKDF_SHA384` accept OtherInfo as spans, avoiding a concatenation buffer.
They implement the single-step KDF, not SP 800-108 KBKDF or HKDF.

## Testing

See [Running the tests](docs/testing.md) for full-suite commands, external
corpora, sanitizers, compiler and profile runs, and fuzzing.

`test_default_profile` tests the configured `tiny_crypto_c` target. The other
tests share libraries built for specific configurations: full API, AES-192/256,
weak-key rejection, runtime S-box, and GHASH profiles. Each configuration is
compiled once and reused by its tests.

The fast suite covers all C modes and C++ wrappers. C tests use [µunit][munit],
and C++ tests use [doctest][doctest]. You can filter the C++ tests with doctest's
command-line options, for example `./build/test_cpp_hash -tc="*HMAC*"`.

`make test-full` adds the checked-in [NIST CAVP][cavp] response files and
[Wycheproof][wycheproof] authentication vectors,
including the complete 20,000-vector SP 800-108 KBKDF corpus split across
`test_kdf` (128-bit AES and every other PRF), `test_kdf_192` and
`test_kdf_256`.
CI tests with GCC, Clang, Apple Clang, and MSVC, runs sanitizers and the full
vector suite, and checks Arduino Uno and RP2350 build sizes.

TLV tests cover framing, DER values, resource limits, and split input. The
optional corpus adapter compares CVC fields with the supplied metadata and
reads ASN.1 objects without evaluating certificate trust. Set
`TINY_CRYPTO_TLV_CORPUS` to a directory containing `piv/` and `x509/` to run it.
If `eac/cvc/` is present, the EAC tests also check certificate fields,
inherited EC parameter widths, and malformed encodings.
`TINY_CRYPTO_TLV_MBEDTLS_SUITE` selects an external, pinned ASN.1 test data file.
CI downloads that file into its temporary directory.

Clang builds can enable `TINY_CRYPTO_BUILD_FUZZERS` and run `fuzz_tlv` and
`fuzz_pki`.
Keep its writable corpus and failure artifacts outside the source tree.

## License

Project code is licensed under [GPL-2.0-or-later](LICENSE). Unicode normalization
tables use the [Unicode License v3](LICENSES/Unicode-3.0.txt). Bundled test
materials retain their own terms. [µunit][munit] (`tests/support/munit.h`) and
[doctest][doctest] (`tests/support/doctest.h`) use the MIT license.
[Wycheproof][wycheproof] vectors use Apache-2.0. [NIST CAVP][cavp] response
files are U.S. Government works. Corpus READMEs under `tests/vectors/` record
the source, license, transformations, and checksums for each collection. Test
corpora are excluded from installed packages and embedded library images.
The adapted contribution policy retains its upstream
[MIT license](LICENSES/BoundedContributionPolicy-MIT.txt).

[doctest]: https://github.com/doctest/doctest
[munit]: https://nemequ.github.io/munit/
[cavp]: https://csrc.nist.gov/projects/cryptographic-algorithm-validation-program
[wycheproof]: https://github.com/C2SP/wycheproof
