# Integration

This document explains how to add DFC core to a C project.

## 1. Add the source files

Add these include directories:

```text
dfc-core/src
dfc-core/port
```

Compile all `.c` files in `dfc-core/src`. Each file compiles to nothing when
the build role or profile leaves its feature out, so the source list never
changes with the configuration.

## 2. Add a platform port

Implement the interfaces in these headers:

- `port/dfc_port.h` provides assertions, logging, random data, memory, and user
  notifications.
- `port/dfc_bytebuf.h` provides an append-only byte buffer.

Do not use a predictable random generator. DESFire authentication requires
unpredictable random data.

For a hosted build, compile these example files:

```text
port/host/dfc_port_host.c
port/host/dfc_bytebuf_host.c
```

## 3. Select a crypto backend

For mbedTLS 3.x, compile with:

```sh
-DDFC_CRYPTO_BACKEND_MBEDTLS=1
```

Link `libmbedcrypto`, or compile the target's mbedTLS AES and DES sources.
mbedTLS 4 removed DES, so DFC requires mbedTLS 3.x.

For tiny_crypto_c, compile `src/aes.c`, `src/des.c`, and `src/common.c` and use:

```sh
-DDFC_CRYPTO_BACKEND_TINY=1 -I/path/to/tiny_crypto_c/src \
-DTC_ENABLE_AES=1 -DTC_AES_ENABLE_CBC=1 -DTC_AES_ENABLE_CTR=0 \
-DTC_ENABLE_DES=1 -DTC_DES_ENABLE_ECB=1 -DTC_DES_ENABLE_CBC=1 \
-DTC_DES_ENABLE_CTR=0 -DTC_DES_ENABLE_TDES=1 -DTC_ENABLE_SHA256=0
```

Select exactly one backend. The tiny_crypto_c feature macros must match across
DFC and the library sources because they affect context layouts.

## 4. Select a feature profile

Set `DFC_BUILD_PROFILE` to one value from `src/dfc_build_config.h`. The default
value is `DFC_PROFILE_FULL_EV3`.

Example:

```sh
-DDFC_BUILD_PROFILE=DFC_PROFILE_FULL_EV2
```

You can override individual `DFC_ENABLE_*` macros. The header checks invalid
feature combinations during compilation.

`DFC_ENABLE_ISO7816_AUTH` controls standard ISO 7816 mutual authentication
and defaults to the profile's ISO 7816 support. Set it to `0` when a small
target does not need that command sequence. `DFC_ENABLE_SPECIAL_KEY_STATUS`
controls EV3 responses for disabled DAM and virtual-card key slots; it defaults
on only in the full EV3 profile. Static IC signatures use
`DFC_ENABLE_STATIC_SIGNATURE` and do not require symmetric originality keys.

## 5. Select a build role

Set `DFC_BUILD_ROLE` to choose which parts of the library a build carries. The
role is independent of the profile: the profile selects a card generation, and
the role selects the halves of the library.

| Role | Carries | Use |
|---|---|---|
| `DFC_ROLE_TARGET` | Emulator, `.dfcb` decoder and encoder | The card itself, on a device |
| `DFC_ROLE_HOST` | Reader, command encoder, `.dfc` and `.dfcb` | A client that drives a card |
| `DFC_ROLE_SIMULATOR` | Everything | A workstation that runs the card and drives it |

The default value is `DFC_ROLE_SIMULATOR`.

A target never parses text. A host compiles `.dfc` to `.dfcb` with
`dfc_text_parse` and `dfc_der_encode`, then loads the octets onto the target,
which reads them with `dfc_der_decode`. A target that never dumps its
credential can also drop the encoder:

```sh
-DDFC_BUILD_ROLE=DFC_ROLE_TARGET -DDFC_ENABLE_DER_ENCODER=0
```

These macros override single parts of a role:

| Macro | Part |
|---|---|
| `DFC_ENABLE_EMULATOR` | The card emulator and the virtual PICC |
| `DFC_ENABLE_READER` | The reader session and the command encoder |
| `DFC_ENABLE_DER_DECODER` | `dfc_der_decode` and `dfc_der_length` |
| `DFC_ENABLE_DER_ENCODER` | `dfc_der_encode` and `dfc_der_encoded_size` |
| `DFC_ENABLE_TEXT_CODEC` | `dfc_text_*` and `dfc_credential_load`. It requires both directions of `.dfcb` |
| `DFC_ENABLE_BINARY_CODEC` | Both directions of `.dfcb` at once. It is kept for older builds |

A declaration is visible only when its part is built, so a call into an omitted
part fails at compile time.

Storage for the credential model is fixed at compile time. Override
`DFC_FILE_POOL_SIZE`, `DFC_KEY_POOL_SIZE`, `DFC_MAX_APPS`, `DFC_MAX_FILES`, and
`DFC_MAX_KEYS` in the same way to trade memory against card capacity.

## 6. Build the shared library

The CMake build produces the core as a static archive, `dfc_core`, and as the
shared library `dfc`. The shared library carries the flat interface in
`ffi/dfc_ffi.h` for foreign runtimes and a platform port that draws randomness
from the operating system or from a callback per virtual PICC:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

These cache variables select the build:

| Variable | Default | Values |
|---|---|---|
| `DFC_ROLE` | `simulator` | `target`, `host`, `simulator` |
| `DFC_PROFILE` | `full_ev3` | `minimal_ev1`, `full_ev1`, `full_ev2`, `full_ev3`, `minimal_ev3` |
| `DFC_CRYPTO` | `tiny` | `tiny` fetches tiny_crypto_c at a pinned commit; `mbedtls` uses an installed mbedTLS 3.x |
| `DFC_TINY_CRYPTO_DIR` | empty | A local tiny_crypto_c checkout to build from instead |
| `DFC_HOST_CAPACITY` | `OFF` for target, `ON` otherwise | Sizes the credential model for a workstation |
| `DFC_BUILD_SHARED` | `OFF` for target, `ON` otherwise | Builds the flat interface shared library |
| `DFC_FEATURE_DEFINITIONS` | empty | Semicolon-separated `DFC_ENABLE_*` values ending in `=0` or `=1` |
| `DFC_MAX_APPS`, `DFC_MAX_FILES`, `DFC_MAX_KEYS`, `DFC_FILE_POOL_SIZE`, `DFC_KEY_POOL_SIZE`, `DFC_MAX_FILE_DATA` | profile defaults | Set fixed model and transfer buffers for the device |

For a small embedded card build with only the binary decoder:

```sh
cmake -S . -B build-target -DDFC_ROLE=target -DDFC_PROFILE=minimal_ev1 \
  -DDFC_CRYPTO=tiny -DDFC_FEATURE_DEFINITIONS=DFC_ENABLE_DER_ENCODER=0 \
  -DDFC_MAX_FILE_DATA=512
cmake --build build-target
```

For a reader-only mobile native library:

```sh
cmake -S . -B build-mobile -DDFC_ROLE=host -DDFC_PROFILE=full_ev3 \
  -DDFC_CRYPTO=tiny -DDFC_HOST_CAPACITY=OFF -DDFC_BUILD_TESTS=OFF
cmake --build build-mobile
```

The .NET package uses a simulator-role native library so it can expose both
reader and virtual-card APIs. Its native builds use tiny_crypto_c. A host-role
library is suitable when an app only uses the reader.

The interface uses fixed layouts and opaque handles only, so a binding does not
depend on the features or pool sizes a library was built with.
`dfc_ffi_capabilities` reports both, and `dfc_ffi_struct_sizes` lets a binding
check its declarations against the library it loaded.

## 7. Verify the integration

Run the host tests before you build for the target:

```sh
make -C tests test
```

On the target, verify these items:

- The random-data function returns unpredictable data.
- Allocation failures have the required product behavior.
- Assertions have the required product behavior.
- AES, DES, 2-key 3DES, and 3-key 3DES authentication pass known vectors.
- Sensitive log output is disabled in production.
