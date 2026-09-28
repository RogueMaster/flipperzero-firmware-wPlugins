# DFC core

DFC core is a portable DESFire-compatible engine. It provides these parts:

- A credential data model
- `.dfc` text encoding and decoding
- `.dfcb` binary encoding and decoding
- Secure messaging
- A virtual card emulator
- A reader with command encoding and secure messaging
- A flat interface for foreign runtimes, and a .NET library over it

See the [changelog](CHANGELOG.md) for release notes.
The [credential format specification](docs/credential-format-v6.md) defines
`.dfc` and `.dfcb` versions 4 through 6. The
[full v6 schema](docs/credential-schema-v6.md) lists every field and binary
tag. Start with the [v6 example credentials](examples/credentials/README.md),
then use the [migration guide](docs/credential-migration.md) when updating
stored credentials.

It is suitable for hosted systems and freestanding C targets. A build role
selects the parts one product carries. A card carries the emulator and the
`.dfcb` codec. A host-side client carries the reader and both encodings. A
simulator carries everything.

## Requirements

Use a C11 compiler and select either mbedTLS 3.x or
[tiny_crypto_c](https://github.com/mistial-dev/tiny_crypto_c). See
[Integration](docs/integration.md) for compiler options.

## Run the tests

With mbedTLS 3.x installed:

```sh
make -C tests test
```

With tiny_crypto_c checked out:

```sh
make -C tests test CRYPTO_BACKEND=tiny TINY_CRYPTO_DIR=/path/to/tiny_crypto_c
```

Run the suite under a sanitizer. AddressSanitizer and UndefinedBehaviorSanitizer
work with gcc or clang on any host; MemorySanitizer needs clang on Linux:

```sh
make -C tests test SANITIZE=address,undefined
make -C tests test CC=clang SANITIZE=memory
```

The test command builds and runs every unit suite and checks that each
reduced build profile compiles.

## Integrate the library

Add `src/` and `port/` to the compiler include path. Compile all files in
`src/`. Also compile one platform implementation and the crypto libraries.

For a complete procedure, see [Integration](docs/integration.md).

Use `port/dfc_port.h` to implement the platform services. Use
`port/dfc_bytebuf.h` to implement the response buffer. The `port/host/`
directory contains workstation implementations that you can use as examples.

Use `src/dfc_build_config.h` to select a feature profile. A reduced profile
rejects a known feature when the profile does not support that feature. It does
not silently discard data.

## Build the shared library

The CMake build produces the core as a static archive and as a shared library
that exports `ffi/dfc_ffi.h`:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

## Use it from .NET

`bindings/dotnet` holds the `Dfc` library, which runs every rule in native code.
It provides the credential records and codec, `DfcVirtualPicc` and
`DfcReader`. See its [README](bindings/dotnet/README.md).

## License

DFC core is licensed under the GNU General Public License, version 2 or (at
your option) any later version (`GPL-2.0-or-later`). See [LICENSE](LICENSE).
