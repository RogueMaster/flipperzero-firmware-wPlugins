<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# tiny_crypto_c

[![CI](https://github.com/mistial-dev/tiny_crypto_c/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/mistial-dev/tiny_crypto_c/actions/workflows/ci.yml)
[![License: GPL v2+](https://img.shields.io/badge/license-GPL--2.0--or--later-blue.svg)](LICENSE)
[![CLA assistant](https://cla-assistant.io/readme/badge/mistial-dev/tiny_crypto_c)](https://cla-assistant.io/mistial-dev/tiny_crypto_c)

tiny_crypto_c provides small, portable cryptographic primitives for embedded C
and C++. Library code is heap free. Callers supply all memory. The C++11 wrappers
take `bytes` spans and C arrays, transform block-mode data in place, work
without the standard library or exceptions, and return the C API's result types
as `[[nodiscard]]` values. See [C++ wrappers](docs/cpp.md). Disabled
algorithms and modes are left out of the build.

The default profile enables **AES-128 CTR and SHA-256**. DES, 3DES, SHA-1,
SHA-224, SHA-384, SHA-512, HMAC, KMAC256, the NIST SP 800-108 key-based KDF,
[SP 800-90A DRBGs](docs/drbg.md) and other block-cipher modes can be enabled
as needed.

## Uses

Use the primitives in firmware that needs bounded AES, hashing, MACs, key
derivation, or caller-seeded random-bit generation. The optional credential
modules support PIV and TWIC reader workflows: parsing card data, checking
CMS signatures and X.509 paths, applying credential policy, and checking CRLs.
Feature gates let a small device link only the algorithms its application uses.
See the [credential reader guide](docs/credential-reader.md) and
[API guide](docs/api.md) for the supported workflows and buffer requirements.

## Versioning

Releases follow [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html)
using `MAJOR.MINOR.PATCH`. A major version can change public C or C++ calls or
documented behavior in ways that require caller changes. A minor version adds
compatible functionality. A patch version contains compatible fixes.

The public API comprises installed headers, documented behavior and status
values, build options, and the installed CMake target. Applications should
rebuild the library with their toolchain when upgrading. Binary compatibility
across different toolchains is outside this versioning policy. Version
**2.0.0** includes public API changes that require callers upgrading from 1.x
to review and update their code.

## Build

```sh
make
make test            # quick suite, skips tests labelled extended
make test-full       # every test, including NIST CAVP and Wycheproof corpora
make test-sanitize   # address + undefined behavior sanitizers
make benchmark
make size
```

Make passes all `TINY_CRYPTO_*` command-line variables to CMake:

```sh
make TINY_CRYPTO_ENABLE_HMAC=ON TINY_CRYPTO_AES_ENABLE_GCM=ON
```

Or build with CMake directly:

```sh
cmake -S . -B build -DTINY_CRYPTO_ENABLE_HMAC=ON -DTINY_CRYPTO_AES_ENABLE_GCM=ON
cmake --build build
```

As a CMake dependency:

```cmake
add_subdirectory(path/to/tiny_crypto_c)
target_link_libraries(firmware PRIVATE tiny_crypto_c::tiny_crypto_c)
```

`cmake --install` installs the headers, the archive, a CMake package and a
generated `tiny_crypto/build_config.h`. That header records the configuration
the archive was built with, so consumers that use the installed headers without
CMake get the same structure layouts. A `-D` definition that contradicts it
fails with "differs from the installed library configuration".

Public headers are in `src/tiny_crypto/`, following the Arduino library layout:

```c
#include <tiny_crypto/tiny_crypto.h>
```

Use `<tiny_crypto/tiny_crypto.hpp>` for the C++11 wrappers in the
`tiny_crypto` namespace.

## Configuration

All options are CMake cache variables. With no profile selected, the build
enables AES-128 CTR and SHA-256. Secret wiping and public argument checks are
always on. Stateful finals, one-shot operations and documented failure paths
clear their secret state. Status-returning operations validate required spans
and pointers before processing input.

`TINY_CRYPTO_RESOURCE_PROFILE=micro` favors small code and byte-limb EC and
RSA arithmetic. `mini` uses native arithmetic while keeping optional algorithms
off. `desktop` enables the supported capabilities, including legacy algorithms
and both PIV secure-messaging suites. Use this profile when broad compatibility
is required, and set application policy to restrict legacy algorithms.

`TINY_CRYPTO_TARGET=piv-acu` or `piv-pd` selects a fixed role-specific algorithm
set independently of resource tuning. A role default overrides the profile
column below, and an explicit setting that contradicts the role stops the
configuration. See [PIV targets and ESP32-P4](docs/esp32-p4.md) for the role
requirements and ESP-IDF builds.

Feature options accept `AUTO`, `ON`, or `OFF`. `AUTO` follows the selected
profile. Explicit settings survive a profile change. Configuration compiles
`config.h` with the selected values and stops with its `#error` text when an
option lacks a dependency, so CMake and direct-source builds accept the same
combinations. Projects that compile the C files directly define the matching
`TC_*` macros documented in [`config.h`](src/tiny_crypto/config.h), and select
a profile with `TC_RESOURCE_PROFILE=TC_RESOURCE_MICRO`, `TC_RESOURCE_MINI`, or
`TC_RESOURCE_DESKTOP`.

Each feature option is `TINY_CRYPTO_` followed by its `config.h` macro without
the `TC_` prefix. `TINY_CRYPTO_AES_ENABLE_CBC` sets `TC_AES_ENABLE_CBC`, and
`TINY_CRYPTO_AES_GCM_GHASH_MODE` sets `TC_AES_GCM_GHASH_MODE`.
[`cmake/features.json`](cmake/features.json) lists every feature with its
description, parent, value set and the sources it compiles. Configuration stops
on an unknown `TINY_CRYPTO_*` name and names the replacement of a retired one.

The tables give the `AUTO` value for each profile. Default is the build with
no profile selected. Mode, TDEA and curve options take effect only when their
algorithm is enabled.

### Minimal build

Every algorithm, mode, parser and protocol module is a feature. With every
feature option `OFF`, the archive holds only `src/common.c`, which provides
secret wiping, constant-time comparison and the span helpers every module
uses. Shared helpers, such as the hash core, the block-mode core and the PKI
storage planner, compile only while a feature that uses them is on. Start from
that build and enable the features an application needs. Configuration names
each dependency that `config.h` requires.

### Algorithms

| Option                           | Default | micro | mini | desktop | Purpose                                                            |
| -------------------------------- | ------- | ----- | ---- | ------- | ------------------------------------------------------------------ |
| `TINY_CRYPTO_ENABLE_AES`         | ON      | ON    | ON   | ON      | AES block cipher with the key size from `TINY_CRYPTO_AES_KEY_BITS` |
| `TINY_CRYPTO_AES_ENABLE_DYNAMIC` | OFF     | OFF   | OFF  | ON      | Per-context AES-128/192/256 keys, CBC and CMAC                     |
| `TINY_CRYPTO_ENABLE_DES`         | OFF     | OFF   | OFF  | ON      | DES, with TDEA and the DES modes below                             |
| `TINY_CRYPTO_DES_ENABLE_TDES`    | ON      | ON    | ON   | ON      | Two- and three-key TDEA                                            |
| `TINY_CRYPTO_ENABLE_EC`          | OFF     | OFF   | OFF  | ON      | ECDH, ECDSA and key generation on the enabled curves               |
| `TINY_CRYPTO_EC_ENABLE_P192`     | OFF     | OFF   | OFF  | OFF     | P-192 for legacy protocols                                         |
| `TINY_CRYPTO_EC_ENABLE_P256`     | ON      | ON    | ON   | ON      | P-256                                                              |
| `TINY_CRYPTO_EC_ENABLE_P384`     | ON      | ON    | ON   | ON      | P-384                                                              |
| `TINY_CRYPTO_EC_SMALL`           | OFF     | ON    | OFF  | OFF     | Byte limbs for EC arithmetic (always used on AVR)                  |
| `TINY_CRYPTO_ENABLE_RSA`         | OFF     | OFF   | OFF  | ON      | RSA verification, signing, OAEP, key validation and key generation |
| `TINY_CRYPTO_RSA_ENABLE_1024`    | OFF     | OFF   | OFF  | OFF     | Legacy RSA-1024, requires an explicit override                     |
| `TINY_CRYPTO_RSA_ENABLE_2048`    | ON      | ON    | ON   | ON      | RSA-2048                                                           |
| `TINY_CRYPTO_RSA_ENABLE_3072`    | ON      | ON    | ON   | ON      | RSA-3072                                                           |
| `TINY_CRYPTO_RSA_ENABLE_4096`    | ON      | ON    | ON   | ON      | RSA-4096                                                           |
| `TINY_CRYPTO_RSA_SMALL`          | OFF     | ON    | OFF  | OFF     | Byte limbs for RSA arithmetic (always used on AVR)                 |
| `TINY_CRYPTO_ENABLE_SHA1`        | OFF     | OFF   | OFF  | ON      | SHA-1                                                              |
| `TINY_CRYPTO_ENABLE_SHA224`      | OFF     | OFF   | OFF  | ON      | SHA-224 on the SHA-256 core                                        |
| `TINY_CRYPTO_ENABLE_SHA256`      | ON      | ON    | ON   | ON      | SHA-256                                                            |
| `TINY_CRYPTO_ENABLE_SHA384`      | OFF     | OFF   | OFF  | ON      | SHA-384 on the SHA-512 core                                        |
| `TINY_CRYPTO_ENABLE_SHA512`      | OFF     | OFF   | OFF  | ON      | SHA-512                                                            |
| `TINY_CRYPTO_ENABLE_MD5`         | OFF     | OFF   | OFF  | ON      | MD5 checksums for legacy data                                      |
| `TINY_CRYPTO_ENABLE_HMAC`        | OFF     | OFF   | OFF  | ON      | HMAC over the enabled SHA algorithms                               |
| `TINY_CRYPTO_ENABLE_KMAC256`     | OFF     | OFF   | OFF  | ON      | Fixed-output KMAC256 with customization                            |
| `TINY_CRYPTO_ENABLE_KDF`         | OFF     | OFF   | OFF  | ON      | SP 800-108r1 KBKDF over the enabled HMAC and CMAC PRFs             |
| `TINY_CRYPTO_ENABLE_HKDF`        | OFF     | OFF   | OFF  | ON      | RFC 5869 HKDF over the enabled HMAC-SHA algorithms                 |
| `TINY_CRYPTO_ENABLE_SSKDF`       | OFF     | OFF   | OFF  | ON      | SP 800-56C one-step hash KDF over the enabled SHA algorithms       |
| `TINY_CRYPTO_ENABLE_DRBG`        | OFF     | OFF   | OFF  | ON      | [SP 800-90A DRBGs](docs/drbg.md)                                   |
| `TINY_CRYPTO_DRBG_ENABLE_HASH`   | OFF     | OFF   | OFF  | ON      | Hash_DRBG over the enabled SHA algorithms                          |
| `TINY_CRYPTO_DRBG_ENABLE_HMAC`   | OFF     | OFF   | OFF  | ON      | HMAC_DRBG, requires HMAC                                           |
| `TINY_CRYPTO_DRBG_ENABLE_CTR`    | OFF     | OFF   | OFF  | ON      | CTR_DRBG, requires `TINY_CRYPTO_AES_ENABLE_DYNAMIC`                |

### Block-cipher modes

| Option                             | Default | micro | mini | desktop | Purpose                                          |
| ---------------------------------- | ------- | ----- | ---- | ------- | ------------------------------------------------ |
| `TINY_CRYPTO_AES_ENABLE_CTR`       | ON      | ON    | ON   | ON      | AES-CTR                                          |
| `TINY_CRYPTO_AES_ENABLE_CBC`       | OFF     | OFF   | OFF  | ON      | AES-CBC                                          |
| `TINY_CRYPTO_AES_ENABLE_ECB`       | OFF     | OFF   | OFF  | ON      | AES-ECB                                          |
| `TINY_CRYPTO_AES_ENABLE_OFB`       | OFF     | OFF   | OFF  | ON      | AES-OFB                                          |
| `TINY_CRYPTO_AES_ENABLE_GCM`       | OFF     | OFF   | OFF  | ON      | AES-GCM                                          |
| `TINY_CRYPTO_AES_ENABLE_CCM`       | OFF     | OFF   | OFF  | ON      | AES-CCM                                          |
| `TINY_CRYPTO_AES_ENABLE_EAX`       | OFF     | OFF   | OFF  | ON      | AES-EAX                                          |
| `TINY_CRYPTO_AES_ENABLE_EAX_PRIME` | OFF     | OFF   | OFF  | ON      | ANSI C12.22 EAX'                                 |
| `TINY_CRYPTO_AES_ENABLE_SIV`       | OFF     | OFF   | OFF  | ON      | AES-SIV (RFC 5297)                               |
| `TINY_CRYPTO_AES_ENABLE_CMAC`      | OFF     | OFF   | OFF  | ON      | AES-CMAC                                         |
| `TINY_CRYPTO_AES_ENABLE_KW`        | OFF     | OFF   | OFF  | ON      | AES key wrap, KW and KWP (SP 800-38F)            |
| `TINY_CRYPTO_AES_WIDE_OPS`         | OFF     | OFF   | ON   | ON      | Native-width AES helpers                         |
| `TINY_CRYPTO_AES_TINY`             | OFF     | ON    | OFF  | OFF     | Reject the 256-byte `fast-table` GHASH context   |
| `TINY_CRYPTO_DES_ENABLE_CTR`       | ON      | ON    | ON   | ON      | DES-CTR                                          |
| `TINY_CRYPTO_DES_ENABLE_ECB`       | OFF     | OFF   | OFF  | ON      | DES-ECB                                          |
| `TINY_CRYPTO_DES_ENABLE_CBC`       | OFF     | OFF   | OFF  | ON      | DES-CBC                                          |
| `TINY_CRYPTO_DES_ENABLE_OFB`       | OFF     | OFF   | OFF  | ON      | DES-OFB                                          |
| `TINY_CRYPTO_DES_ENABLE_CFB1`      | OFF     | OFF   | OFF  | ON      | DES-CFB1                                         |
| `TINY_CRYPTO_DES_ENABLE_CFB8`      | OFF     | OFF   | OFF  | ON      | DES-CFB8                                         |
| `TINY_CRYPTO_DES_ENABLE_CFB64`     | OFF     | OFF   | OFF  | ON      | DES-CFB64                                        |
| `TINY_CRYPTO_DES_ENABLE_CMAC`      | OFF     | OFF   | OFF  | ON      | TDEA-CMAC                                        |
| `TINY_CRYPTO_DES_ENABLE_ISO9797`   | OFF     | OFF   | OFF  | OFF     | ISO/IEC 9797-1 MAC algorithms 1 and 3            |
| `TINY_CRYPTO_DES_REJECT_WEAK_KEYS` | OFF     | OFF   | OFF  | OFF     | Reject weak DES keys and degenerate TDEA bundles |

### Formats, compression and trust

| Option                                     | Default | micro  | mini   | desktop | Purpose                                                |
| ------------------------------------------ | ------- | ------ | ------ | ------- | ------------------------------------------------------ |
| `TINY_CRYPTO_ENABLE_TLV`                   | OFF     | OFF    | OFF    | ON      | Bounded TLV readers and tree traversal                 |
| `TINY_CRYPTO_TLV_ENABLE_BER`               | OFF     | OFF    | OFF    | ON      | ASN.1 BER, including indefinite lengths                |
| `TINY_CRYPTO_TLV_ENABLE_STREAM`            | OFF     | OFF    | OFF    | ON      | Incremental TLV reader                                 |
| `TINY_CRYPTO_ENABLE_DER`                   | OFF     | OFF    | OFF    | ON      | DER value readers, requires TLV                        |
| `TINY_CRYPTO_ENABLE_X509`                  | OFF     | OFF    | OFF    | ON      | X.509 certificate and public-key readers, requires DER |
| `TINY_CRYPTO_ENABLE_X509_PATH`             | OFF     | OFF    | OFF    | ON      | Path validation and trust stores                       |
| `TINY_CRYPTO_ENABLE_TRUST_ANCHOR_FORMAT`   | OFF     | OFF    | OFF    | ON      | RFC 5914 trust-anchor lists                            |
| `TINY_CRYPTO_TAF_ENABLE_CERTIFICATE`       | format  | format | format | format  | Certificate choice in RFC 5914 lists                   |
| `TINY_CRYPTO_TAF_ENABLE_TBS_CERTIFICATE`   | format  | format | format | format  | TBSCertificate choice in RFC 5914 lists                |
| `TINY_CRYPTO_TAF_ENABLE_TRUST_ANCHOR_INFO` | format  | format | format | format  | TrustAnchorInfo choice in RFC 5914 lists               |
| `TINY_CRYPTO_ENABLE_X509_REVOCATION`       | OFF     | OFF    | OFF    | ON      | CRL parsing and path revocation                        |
| `TINY_CRYPTO_ENABLE_X509_OCSP`             | OFF     | OFF    | OFF    | ON      | OCSP requests and responses, requires SHA-1            |
| `TINY_CRYPTO_ENABLE_KEY_CHALLENGE`         | OFF     | OFF    | OFF    | ON      | Public-key proof-of-possession challenges              |
| `TINY_CRYPTO_ENABLE_GZIP`                  | OFF     | OFF    | OFF    | ON      | Bounded GZIP decompression                             |

### PIV, TWIC and credentials

| Option                                  | Default | micro | mini | desktop | Purpose                                           |
| --------------------------------------- | ------- | ----- | ---- | ------- | ------------------------------------------------- |
| `TINY_CRYPTO_ENABLE_APDU`               | OFF     | OFF   | OFF  | ON      | ISO/IEC 7816-4 APDU encoding and exchange         |
| `TINY_CRYPTO_ENABLE_PIV_COMMAND`        | OFF     | OFF   | OFF  | ON      | PIV and TWIC card commands, requires APDU and TLV |
| `TINY_CRYPTO_ENABLE_PIV_OIDS`           | OFF     | OFF   | OFF  | ON      | PIV and TWIC identifier classification            |
| `TINY_CRYPTO_ENABLE_CMS`                | OFF     | OFF   | OFF  | ON      | CMS parsing and signer verification, requires BER |
| `TINY_CRYPTO_ENABLE_CMS_VALIDATION`     | OFF     | OFF   | OFF  | ON      | CMS signer paths and revocation                   |
| `TINY_CRYPTO_ENABLE_PIV_OBJECTS`        | OFF     | OFF   | OFF  | ON      | PIV and TWIC object readers                       |
| `TINY_CRYPTO_ENABLE_CREDENTIAL`         | OFF     | OFF   | OFF  | ON      | Composed PIV and TWIC credential validation       |
| `TINY_CRYPTO_ENABLE_PIV_CHUID`          | OFF     | OFF   | OFF  | ON      | PIV CHUID reader                                  |
| `TINY_CRYPTO_ENABLE_PIV_CVC`            | OFF     | OFF   | OFF  | ON      | PIV secure-messaging CVC reader                   |
| `TINY_CRYPTO_ENABLE_EAC_CVC`            | OFF     | OFF   | OFF  | ON      | BSI TR-03110 EAC CVC reader                       |
| `TINY_CRYPTO_ENABLE_PIV_SM`             | OFF     | OFF   | OFF  | ON      | Client-side PIV secure messaging                  |
| `TINY_CRYPTO_ENABLE_PIV_SM_APDU`        | OFF     | OFF   | OFF  | ON      | PIV SM framing, requires PIV command, SM and CVC  |
| `TINY_CRYPTO_ENABLE_PIV_VCI`            | OFF     | OFF   | OFF  | ON      | PIV VCI, requires SM framing and PIV objects      |
| `TINY_CRYPTO_ENABLE_PIV_CATALOG`        | OFF     | OFF   | OFF  | ON      | PIV and TWIC catalogs and card inventory          |
| `TINY_CRYPTO_ENABLE_PIV_KEY_PROOF`      | OFF     | OFF   | OFF  | ON      | PIV and TWIC card key proofs                      |
| `TINY_CRYPTO_ENABLE_PIV_CARD_CHECK`     | OFF     | OFF   | OFF  | ON      | Composed PIV and TWIC card check report           |
| `TINY_CRYPTO_PIV_SM_ENABLE_CS2`         | ON      | ON    | ON   | ON      | Cipher suite 2 (P-256, AES-128)                   |
| `TINY_CRYPTO_PIV_SM_ENABLE_CS7`         | ON      | ON    | ON   | ON      | Cipher suite 7 (P-384, AES-256)                   |
| `TINY_CRYPTO_ENABLE_FASCN`              | OFF     | OFF   | OFF  | ON      | FASC-N readers and writers                        |
| `TINY_CRYPTO_ENABLE_TWIC_UUID`          | OFF     | OFF   | OFF  | ON      | TWIC NEXGEN UUID helpers                          |
| `TINY_CRYPTO_ENABLE_TWIC_CCL`           | OFF     | OFF   | OFF  | ON      | TWIC canceled card list reader                    |
| `TINY_CRYPTO_ENABLE_TWIC_TPK`           | OFF     | OFF   | OFF  | ON      | TWIC privacy-key container reader                 |
| `TINY_CRYPTO_ENABLE_TWIC_OBJECT_CRYPTO` | OFF     | OFF   | OFF  | ON      | TWIC private-object encryption                    |
| `TINY_CRYPTO_ENABLE_AAMVA`              | OFF     | OFF   | OFF  | ON      | ANSI AAMVA payload readers                        |
| `TINY_CRYPTO_AVR_PROGMEM`               | ON      | ON    | ON   | ON      | Keep constant tables in AVR flash                 |

The three `TINY_CRYPTO_TAF_ENABLE_*` choices follow
`TINY_CRYPTO_ENABLE_TRUST_ANCHOR_FORMAT` under `AUTO`. At least one choice must
be enabled when the format is enabled. See
[Trust anchors](docs/x509-trust-anchors.md) for importing authenticated lists
and applying their constraints, and [X.509 OCSP](docs/x509-ocsp.md) for OCSP
requests, response verification and responder authorization.

### Value options

| Option                           | Values                                                         | Default         |
| -------------------------------- | -------------------------------------------------------------- | --------------- |
| `TINY_CRYPTO_RESOURCE_PROFILE`   | empty, `micro`, `mini`, `desktop`                              | empty           |
| `TINY_CRYPTO_TARGET`             | empty, `piv-acu`, `piv-pd`                                     | empty           |
| `TINY_CRYPTO_AES_KEY_BITS`       | `128`, `192`, `256`                                            | `128`           |
| `TINY_CRYPTO_AES_SBOX_MODE`      | `constant-time`, `runtime`, `fast`                             | `constant-time` |
| `TINY_CRYPTO_AES_GCM_GHASH_MODE` | `profile`, `auto`, `bitwise`, `wide`, `fast-table`, `hardware` | `profile`       |

`TINY_CRYPTO_AES_KEY_BITS` fixes the key size of the `aes.h` API, including
AES-CMAC keys used by KBKDF. `TINY_CRYPTO_AES_ENABLE_DYNAMIC` adds per-context key
sizes. The key wrap KEK follows `TINY_CRYPTO_AES_KEY_BITS`, and
`TINY_CRYPTO_AES_ENABLE_DYNAMIC` adds 128, 192 and 256-bit KEKs. `constant-time`
computes the S-box algebraically. `runtime` builds it in RAM and reads it with
a masked scan. `fast` uses direct table lookups and has
no cache-timing protection.

`TINY_CRYPTO_AES_GCM_GHASH_MODE=profile` selects `auto` for the default and mini builds,
`bitwise` for micro and `wide` for desktop. `fast-table` uses key-dependent
table lookups and adds 256 bytes to each GCM context. Set
`TINY_CRYPTO_AES_TINY=ON`, the micro default, to reject `fast-table` at
configuration time. `hardware` needs a platform GHASH hook, declared in
`aes.h`.

### Build and test options

| Option                         | Default                              | Purpose                                      |
| ------------------------------ | ------------------------------------ | -------------------------------------------- |
| `TINY_CRYPTO_BUILD_TESTS`      | ON at top level, OFF as a subproject | Host tests                                   |
| `TINY_CRYPTO_BUILD_BENCHMARKS` | ON at top level, OFF as a subproject | Host benchmarks for the selected profile     |
| `TINY_CRYPTO_BUILD_FUZZERS`    | OFF                                  | libFuzzer targets, Clang only                |
| `TINY_CRYPTO_SANITIZE`         | empty                                | Test sanitizers, such as `address,undefined` |
| `TINY_CRYPTO_TEST_FULL`        | OFF                                  | Run the checked-in CAVP corpora              |
| `TINY_CRYPTO_TEST_OPENSSL`     | OFF                                  | OpenSSL 3 cross-checks                       |
| `TINY_CRYPTO_TEST_PIV_CARD`    | OFF                                  | PIV card hardware tests over PC/SC           |

[Running the tests](docs/testing.md#test-options) lists the corpus and fixture
options.

### Implementation notes

Enabling `TINY_CRYPTO_ENABLE_DES` also enables CTR and TDEA. One
`struct TC_DES_ctx` serves single DES and TDEA, selected by an 8, 16 or 24-byte
key. See [DES and TDEA](docs/api.md#des-and-tdea). ISO 9797-1 MAC stays off in
every resource profile. Enable it explicitly. See
[DES message authentication](docs/api.md#des-message-authentication) for
algorithm, padding and tag requirements. `TINY_CRYPTO_DES_REJECT_WEAK_KEYS=ON`
rejects weak or semi-weak DES component keys and TDEA bundles that collapse to
single DES. It is off by default for legacy-vector compatibility.

SHA-1, SHA-224, and SHA-256 are implemented in `hash.c`. SHA-384 and SHA-512
share a 64-bit core in `sha512.c`. SHA-224 and SHA-384 reuse the compression
functions of SHA-256 and SHA-512, respectively, but each hash can be enabled
independently.

`TINY_CRYPTO_ENABLE_KDF` builds the SP 800-108r1 key-based key derivation
function in counter, feedback and double-pipeline mode. It needs at least one
PRF: HMAC with an enabled SHA digest, `TINY_CRYPTO_AES_ENABLE_CMAC`, or
`TINY_CRYPTO_DES_ENABLE_CMAC`. Each PRF gets its own function family
(`TC_KBKDF_HMAC_SHA256_counter`, `TC_KBKDF_AES_CMAC_feedback`, ...), so unused
PRFs compile out. TDEA-CMAC is kept for legacy interoperability only. `kdf.h`
describes the SP 800-108r1 key-control mitigations for the CMAC PRFs.

`TINY_CRYPTO_ENABLE_HKDF` needs HMAC and at least one enabled SHA family.
The C and C++ APIs provide extract, expand, and one-shot derive operations.
They also accept a revision 2 hybrid secret as separate `Z` and `T` spans.
See [HKDF usage](docs/hkdf.md) and the [C example](examples/hkdf.c).

`TINY_CRYPTO_AES_ENABLE_KW` builds the SP 800-38F AES key wrap functions KW
(RFC 3394) and KWP (RFC 5649) for storing and transporting keys under a
key-encryption key. See [AES key wrap](docs/aes-kw.md) and the
[C example](examples/aes_kw.c).

## API behavior

See [Working with the API](docs/api.md) for buffer lifetimes, workspace setup,
result handling and complete workflow guides.

Every public status-returning operation uses `TC_result`. Module typedefs and names keep the
call site descriptive: symmetric cryptography uses `TC_status`, RSA uses
`TC_RSA_result`, and parsers use `TC_TLV_result`. They are aliases of the same
type and share values for OK, invalid input, resource limits, caller errors,
unsupported input and internal errors. The
[result model](docs/api.md#result-model) lists the additional state-machine and
protocol results.

One-shot AEAD functions take inputs as `TC_bytes` and outputs as `TC_buffer`.
The tag buffer capacity selects the tag length. The text output must hold the
whole text input, and it may be the same buffer as the input:

```c
uint8_t ciphertext[sizeof message], tag[16];
TC_status status = TC_AES_GCM_encrypt(key, (TC_bytes){iv, 12}, (TC_bytes){aad, sizeof aad},
                                      (TC_bytes){message, sizeof message},
                                      (TC_buffer){ciphertext, sizeof ciphertext},
                                      (TC_buffer){tag, sizeof tag});
if (status != TC_OK)
  return status; /* ciphertext and tag hold no usable output */

status = TC_AES_GCM_decrypt(key, (TC_bytes){iv, 12}, (TC_bytes){aad, sizeof aad},
                            (TC_bytes){ciphertext, sizeof ciphertext},
                            (TC_bytes){tag, sizeof tag},
                            (TC_buffer){ciphertext, sizeof ciphertext});
if (status == TC_MISMATCH)
  return status; /* in-place ciphertext has been wiped */
```

Authentication checks examine the entire tag. GCM, CCM, EAX and EAX'
decryptors authenticate before writing plaintext. SIV writes candidate
plaintext to recompute its synthetic IV, so its associated data must be
disjoint from the output. Every AEAD failure after the argument checks wipes
the text output, separate or in-place. GCM decryption is one-shot. The
streaming GCM context encrypts only. See the
[AEAD contract](docs/api.md#authenticated-encryption) for overlap rules and
error conditions.
GCM requires a 12 to 16-byte tag by default. Use the explicit
`TC_AES_GCM_init_short_tag` or one-shot `_short_tag` functions when a protocol
requires a 4 or 8-byte tag. The GCM packet limits still apply.
CCM, EAX, AES-CMAC and DES-CMAC take tags of at least `TC_MIN_TAG_LEN` bytes
(default 8, raise-only up to 16). Protocols with shorter tags, such as 4-byte
CCM tags, call the `_short_tag` forms. EAX' keeps its fixed 4-byte tag. See
[Tag lengths](docs/api.md#tag-lengths).

CTR, CBC, ECB, OFB, and CFB provide no authentication. Pair them with a MAC or
use an authenticated mode such as GCM, CCM, EAX, or SIV. Never reuse a CTR,
GCM, CCM, EAX, or OFB nonce with the same key.

DES has only a 56-bit effective key and exists for legacy interoperability.
Its table lookups have no cache-timing protection. Limit 3DES to compatibility
code as well.

SHA-1 remains available for compatibility. Do not use it for new
collision-resistant signatures or content identity. HMAC-SHA-1 is a separate
construction whose security is independent of collision resistance. It remains
an acceptable MAC and KBKDF PRF, and is the smallest HMAC option on AVR.

For KBKDF, include the purpose, parties, and requested length in the fixed input
to distinguish keys derived for different uses. `TC_KBKDF_fixed_input` builds
this input as `Label || 0x00 || Context || [L]_32`. Never reuse a
key-derivation key as a derived key. Output lengths are in bytes. A
derivation of `n = ceil(out_len / h)` PRF blocks needs `n <= 2^r - 1` for an
`r`-bit counter. Output buffers must not overlap any input, and `TC_ERROR`
wipes the output when derivation had already started.

HKDF output lengths are in bytes and must be from 1 to `255 * HashLen`.
Pass purpose and protocol context as `info`. Extracted keys are cleared by the
one-shot derive functions. Callers using extract and expand clear their PRK
after the final expansion.

## TLV and DER parsing

Enable `TINY_CRYPTO_ENABLE_TLV` and include `<tiny_crypto/tlv.h>` for bounded
readers over DER, ISO/IEC 7816-4 and, with `TINY_CRYPTO_TLV_ENABLE_BER`, ASN.1 BER.
Select the encoding explicitly. `TC_TLV_limits` bound input bytes, value bytes,
element count and nesting depth. The sibling reader, `TC_TLV_read_tree` and
`TC_TLV_walk` take the input as a borrowed `TC_bytes` span, return spans into
it, and allocate nothing. Decoders that check nesting take caller-owned
`TC_TLV_frames`. `TINY_CRYPTO_TLV_ENABLE_STREAM` adds an incremental reader for
fragmented input. See [TLV parsing](docs/tlv.md) for profiles, results and
examples.

`TINY_CRYPTO_ENABLE_DER` adds `<tiny_crypto/der.h>`: INTEGER, BIT STRING, OID,
BOOLEAN, NULL, SEQUENCE and SET readers plus AlgorithmIdentifier,
SubjectPublicKeyInfo, PKCS #1, PKCS #8 and ECDSA signature structures. See
[DER values](docs/der.md). These readers check encodings. Schema, certificate
and signature validation are separate steps. C++11 code can use
`tiny_crypto::TLVReader` from `<tiny_crypto/tlv.hpp>`. For SignedData
envelopes and signed attributes, see [CMS parsing](docs/cms.md).

## Certificates and PIV objects

`TC_X509_read` reads one DER certificate using caller-owned scratch space:

```c
#include <tiny_crypto/x509.h>

TC_TLV_result read_certificate(TC_bytes der, TC_X509_certificate* certificate)
{
  TC_TLV_frame frames[16];
  TC_bytes extension_oids[32];
  const TC_X509_workspace workspace = {{frames, 16}, extension_oids, 32};
  const TC_TLV_limits limits = {8192, 8192, 1024, 16};

  /* certificate borrows der and changes only on TC_TLV_OK. */
  return TC_X509_read(der, &limits, &workspace, certificate);
}
```

Choose the limits for your application. The workspace needs one frame per
nesting level and one OID slot per extension. Exceeding a limit returns
`TC_TLV_LIMIT`. Results borrow the input buffer, so keep it alive while using
them. The workspace can be reused after the call.

`certificate.public_key` identifies the subject's algorithm, key size, and
named curve. `TC_X509_subject_public_key` also reads a standalone
SubjectPublicKeyInfo. The extension iterator exposes OIDs, critical flags,
and values. Extension decoders take the value as a `TC_bytes` span with its
own limits, which cover the outer element and every element beneath it.
Readers such as `TC_X509_general_names_init` bind their input, limits and
frames at init, and each `next` call spends the budget left by earlier calls.

`<tiny_crypto/key_challenge.h>` prepares and verifies a fresh proof-of-possession
challenge from a validated public key and explicit signature parameters. Card
commands and slot policy stay in protocol code, which selects its algorithm,
key usage and transport identifiers before issuing a challenge.

`TC_PIV_CHUID_read` returns the FASC-N, card UUID (GUID), optional cardholder
UUID, expiration date, and signature. Select `TC_PIV_CHUID_CONTENTS` for the
object contents or `TC_PIV_CHUID_CONTAINER` for a `53`-wrapped object.
Choose the profile from the requested card object before reading its contents.
`TC_CHUID_PROFILE_PIV` enforces the SP 800-73-4 Part 1 Table 9 field order and
requires a nonempty signature field. It accepts the deprecated Buffer Length
(`EE`), Organizational Identifier (`32`) and DUNS (`33`) fields found on older
cards. `signed_content` excludes Buffer Length, as section 3.1.2 requires.
`TC_CHUID_PROFILE_TWIC_SIGNED` and `TC_CHUID_PROFILE_TWIC_UNSIGNED` follow the
TWIC field schema. Unsigned TWIC omits the signature and cardholder UUID
fields.

`TC_PIV_certificate_read` reads a `53` certificate container and returns
borrowed spans for the certificate, the optional secure messaging intermediate
CVC and the optional historic MSCUID. The MSCUID lies outside the signed
certificate and is unauthenticated. Pass
`TC_PIV_CERTIFICATE_RECOMMENDED_BYTES` (1856) as the certificate bound, or a
larger application limit. SP 800-73-5 treats 1856 bytes as a recommendation.
`TC_PIV_certificate_decode` adds GZIP decompression into a caller buffer and
returns one DER certificate. The [card object readers](docs/piv-card.md#card-object-readers)
cover the Discovery Object, CCC, Key History, BIT group and Pairing Code
container.

`TC_PIV_CVC_read` reads card and intermediate secure messaging CVCs as defined
in SP 800-73-5 Part 2, section 4.1.5. Its `signed_data` span contains the
original TLV bytes covered by the signature. `TC_PIV_CVC_chain_verify` checks
direct or intermediate issuer links and signatures under a validated content
signer, with suite and optional UUID binding. See [PIV CVC verification](docs/piv-cvc.md)
for trust prerequisites and workspace setup.

EAC certificates use a different schema. `<tiny_crypto/eac_cvc.h>` provides
`TC_EAC_CVC_read`, a standalone public-key reader, and an extension iterator.
The certificate reader takes the encoding as `TC_bytes`, `TC_TLV_limits` and a
`TC_EAC_CVC_workspace` holding caller-owned `TC_TLV_frames`. Returned fields borrow the input.
Keep the input, frames and output disjoint. Both CVC readers take one complete
object and report a truncated encoding as `TC_TLV_INVALID`.
Its signed span includes the complete `7F4E` body, including tag and length.
Unknown extensions are preserved. Unsupported key or authorization OIDs return
`TC_TLV_UNSUPPORTED`.

Call `TC_EAC_CVC_check_encoding` with the resolved issuer key and, for an EC
subject without explicit parameters, its inherited domain parameters. It
checks coordinate and signature widths. Missing context returns
`TC_TLV_ARGUMENT`. Signature width comes from the issuer key.
The standalone reader also accepts RI-ECDH public-key templates, but these
cannot be used as certificate-signing keys.

These readers parse encodings. Applications must separately verify signatures,
key validity, certificate trust, and expiration before using a credential.

## Benchmarks

We measure flash and static RAM usage for Arduino Uno and Raspberry Pi Pico 2
(RP2350, Arm Cortex-M33) builds. [docs/benchmarks.md](docs/benchmarks.md) lists
the sizes in bytes and as percentages of each board's flash and RAM capacity.
The figures are linked firmware sizes and exclude peak runtime stack use.

Run `make benchmark-report` to regenerate the report, or
`make benchmark-report-check` to check that it is up to date. Both commands
build the board firmware without a connected board and need the toolchains
listed in [Updating the numbers](docs/benchmarks.md#updating-the-numbers). Use
`make benchmark` to measure throughput on the host with the current build
configuration. PR CI uploads a fresh resource report and
enforces flash and stack budgets. The checked-in report is refreshed for releases.

## Smart-card APDUs

`TINY_CRYPTO_ENABLE_APDU` adds `<tiny_crypto/apdu.h>`, an ISO/IEC 7816-4 command
and response codec with a bounded exchange channel. `TC_APDU_command_encode`
writes short or extended commands, and `TC_APDU_response_read` checks the status
bytes. `TC_APDU_transceive` sends a command over a caller transport callback,
chains long SHORT commands, follows `61XX` with GET RESPONSE, applies one `6CXX`
correction per step and collects the response in a caller buffer. The channel
honours the card's DO `7F66` size limits and a fixed exchange budget. See
[Smart-card APDUs](docs/apdu.md).

`TINY_CRYPTO_ENABLE_PIV_COMMAND` adds `<tiny_crypto/piv_command.h>`, the PIV and
TWIC card commands of SP 800-73-5 Part 2 on a `TC_PIV_link`. `TC_PIV_select`
reads the application property template and applies its size limits.
`TC_PIV_get_data` checks the object framing of each application.
`TC_PIV_verify_status` and `TC_PIV_pin_verify` query and verify the PIN with a
retry floor and refuse a plaintext PIN on the contactless interface.
`TC_PIV_status_classify` gives each status word its PIV or TWIC meaning.
`TINY_CRYPTO_ENABLE_PIV_OBJECTS` adds readers for the Discovery Object, the
Card Capability Container, Key History, the BIT group and the Pairing Code
container, and `TC_PIV_certificate_decode` for plain and GZIP certificate
containers. `TINY_CRYPTO_ENABLE_PIV_CATALOG` adds `<tiny_crypto/piv_catalog.h>`: the SP
800-73-5 and TWIC Part 2 data object catalogs with their access rules, and
`TC_PIV_inventory_read`, which reads every object the link state allows into
one caller pool and reports the others as restricted, denied, absent or
oversized. `TINY_CRYPTO_ENABLE_PIV_KEY_PROOF` adds `<tiny_crypto/piv_key_proof.h>`:
`TC_PIV_key_prove` has a card key sign a fresh challenge with GENERAL
AUTHENTICATE and verifies the signature under its validated certificate, with
the SP 800-78-5 algorithm policy of `TC_PIV_key_parameters_select`. See
[PIV card commands](docs/piv-card.md).
`TINY_CRYPTO_ENABLE_PIV_CARD_CHECK` adds `<tiny_crypto/piv_card_check.h>`:
`TC_PIV_card_check` turns an inventory into a report of certificate paths,
revocation evidence, the CHUID, Security Object digests, biometrics, the
secure messaging signer and CVC, and plain copies. Each entry passed, failed
or is not checkable with a reason. `TC_PIV_card_prove_keys` adds the key
proofs, and `TC_PIV_card_report_accepts` compares the report with the
application's requirements. See [PIV card check](docs/piv-card-check.md).
`examples/piv_inspect` runs the complete flow on a PC/SC reader and prints the
report ([piv_inspect](docs/piv-card-check.md#inspect-a-card)).

## PIV secure messaging

`TINY_CRYPTO_ENABLE_PIV_SM` enables the client side of SP 800-73-5 Part 2
section 4 secure messaging for CS2 (P-256, AES-128) and CS7 (P-384, AES-256).
The library performs ECDH, session-key derivation, key confirmation, command
protection and response authentication. The application supplies the APDU
transport and an X.509 content-signing certificate accepted through its
trust, policy, time and revocation checks.

The workflow in `<tiny_crypto/piv_sm.h>` follows the protocol:

1. `TC_PIV_SM_begin` starts a session and fills a `TC_PIV_SM_handshake` with the
   host identifier and ephemeral public key.
1. The application encodes them into GENERAL AUTHENTICATE and decodes the
   card's response into a `TC_PIV_SM_peer`: the exact CVC bytes, nonce,
   cryptogram and received CB_ICC byte.
1. `TC_PIV_SM_finish` takes the CVC public key after the application has
   authenticated it. `TC_PIV_SM_authenticate_response` in
   `<tiny_crypto/piv_sm_authenticate.h>` verifies the CVC chain and completes key
   confirmation in one call.
1. `TC_PIV_SM_protect` encrypts command data and tags the caller's ordered
   authenticated spans. `TC_PIV_SM_ciphertext_size` gives the padded length.
1. `TC_PIV_SM_unprotect` authenticates the response spans and then decrypts.

The caller owns the zero-initialized `TC_PIV_SM` and the `TC_PIV_SM_workspace`.
Only one command may be pending. `TC_PIV_SM_get_state` tells a retryable
unprotect error from one that ended the session.

`TINY_CRYPTO_ENABLE_PIV_SM_APDU` adds `<tiny_crypto/piv_sm_apdu.h>`, the
secure messaging layer of a PIV card link. `TC_PIV_SM_key_request` sends the
key establishment command and binds the session, and `TC_PIV_link_secure`
protects every later GET DATA, VERIFY and GENERAL AUTHENTICATE with the
`87/97/99/8E` wire format, `1C` chaining and in-place decryption. Any secure
messaging failure ends the session, and the link refuses protected commands
until `TC_PIV_link_unsecure`. `TINY_CRYPTO_ENABLE_PIV_VCI` adds
`<tiny_crypto/piv_vci.h>`: `TC_PIV_discovery_get` reads the Discovery Object
over the secured link, and `TC_PIV_vci_establish` opens the virtual contact
interface with the pairing code, or without it when the card's policy allows.
The C++11 `tiny_crypto::PIVSM` wrapper clears its session on destruction and cannot be
copied or moved. See [PIV secure messaging](docs/piv-sm.md) for build options,
span layouts, state transitions and every result.

The underlying `TC_ECDH`, `TC_EC_public_key`, and `TC_EC_validate_public_key`
APIs take fixed-width scalars and uncompressed SEC1 public keys as spans, plus a
work budget, and return a `TC_EC_result`. They support P-256 and P-384. See
[Elliptic-curve operations](docs/ec.md). `TC_SSKDF_SHA1` through
`TC_SSKDF_SHA512` implement the SP 800-56C Rev. 2 one-step KDF, one function
per enabled SHA. They accept FixedInfo as spans, avoiding a concatenation
buffer. SP 800-108r1 KBKDF and HKDF have separate APIs.

## Testing

See [Running the tests](docs/testing.md) for full-suite commands, external
corpora, sanitizers, compiler and profile runs, and fuzzing.
The standard suites use vendored vectors without OpenSSL.
`TINY_CRYPTO_TEST_OPENSSL=ON` adds optional cross-checks.

Run the host suite with every available GCC and Clang toolchain using
`make test-compilers`. Override versioned compiler names with, for example,
`make test-compilers TOOLCHAINS='gcc-15:g++-15 clang:clang++'`.

`test_default_profile` tests the configured `tiny_crypto_c` target. The other
tests share libraries built for specific configurations: full API, AES-192/256,
weak-key rejection, runtime S-box, and GHASH profiles. Each configuration is
compiled once and reused by its tests.

The fast suite covers all C modes and C++ wrappers. C tests use [µunit][munit],
and C++ tests use [doctest]. You can filter the C++ tests with doctest's
command-line options, for example `./build/test_cpp_hash -tc="*HMAC*"`.

`make test-full` adds the tests labelled `extended`: the checked-in
[NIST CAVP][cavp] response files including the SP 800-90A DRBG answers,
FIPS 186 signature and key-generation vectors, and
[Wycheproof] vectors,
including the complete 20,000-vector SP 800-108 KBKDF corpus split across
`test_kdf` (128-bit AES and every other PRF), `test_kdf_192` and
`test_kdf_256`.
CI tests with GCC, Clang, Apple Clang, and MSVC, runs sanitizers, and checks
Arduino Uno and RP2350 build sizes. The manually triggered
[Full test suite](.github/workflows/full-tests.yml) runs the vendored cryptographic
and parser vectors with the Unicode reference files fetched by that workflow.

TLV tests cover framing, DER values, resource limits, and split input. The
optional corpus adapter compares CVC fields with the supplied metadata and
reads ASN.1 objects without evaluating certificate trust. It runs against
`TINY_CRYPTO_TEST_TLV_CORPUS`, which defaults to the checked-in `tests/vectors`.
Point it at another directory containing `piv/` and `x509/` for an external corpus.
If `eac/cvc/` is present, the EAC tests also check certificate fields,
inherited EC parameter widths, and malformed encodings.
`TINY_CRYPTO_TEST_TLV_MBEDTLS_SUITE` selects an external, pinned ASN.1 test data file.
CI downloads that file into its temporary directory.

Clang builds can enable `TINY_CRYPTO_BUILD_FUZZERS` and run `fuzz_tlv`,
`fuzz_pki`, `fuzz_ocsp`, `fuzz_piv_apdu`, `fuzz_gzip` and `fuzz_twic`. The
`test_fuzz_*_regression` tests replay the corpora in `tests/fuzz`.
Keep its writable corpus and failure artifacts outside the source tree.

## License

Project code is licensed under [GPL-2.0-or-later](LICENSE), with an additional
permission to link substantially unmodified Espressif ESP-IDF libraries,
including by static linking. The exception text is at the top of
[LICENSE](LICENSE). Unicode normalization
tables use the [Unicode License v3](LICENSES/Unicode-3.0.txt). Bundled test
materials retain their own terms. [µunit][munit] (`tests/support/munit.h`) and
[doctest] (`tests/support/doctest.h`) use the MIT license.
[Wycheproof] vectors use Apache-2.0. [NIST CAVP][cavp] response
files are U.S. Government works. Corpus READMEs under `tests/vectors/` record
the source, license, transformations, and checksums for each collection. Test
corpora are excluded from installed packages and embedded library images.
The adapted contribution policy retains its upstream
[MIT license](LICENSES/BoundedContributionPolicy-MIT.txt).

Individuals and corporations that require alternate licensing terms may contact
[licensing@mistial.dev](mailto:licensing@mistial.dev) by email.

[cavp]: https://csrc.nist.gov/projects/cryptographic-algorithm-validation-program
[doctest]: https://github.com/doctest/doctest
[munit]: https://nemequ.github.io/munit/
[wycheproof]: https://github.com/C2SP/wycheproof
