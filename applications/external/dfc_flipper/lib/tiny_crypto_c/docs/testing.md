<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# Running the tests

The ordinary suite is offline. It builds the enabled C munit tests, C++
doctest tests, examples, package checks and configuration checks from files in
the source tree.

```sh
cmake -S . -B build -DTINY_CRYPTO_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Build a named executable before selecting it with `ctest -R`. CTest reports a
test as "Not Run" when its executable is missing. Use
`ctest --test-dir build --show-only` to inspect the configured suite and
`--rerun-failed --output-on-failure` after correcting a failure.

## Profiles and feature configurations

`TINY_CRYPTO_RESOURCE_PROFILE` is empty for the default build, or `micro`, `mini`
or `desktop`. Individual `TINY_CRYPTO_*` options override profile defaults.
Configuration tests compile minimal, maximal and invalid combinations and
check that installed headers describe the same feature set as the library.

```sh
cmake -S . -B build-full \
  -DTINY_CRYPTO_BUILD_TESTS=ON \
  -DTINY_CRYPTO_RESOURCE_PROFILE=desktop
cmake --build build-full --parallel
ctest --test-dir build-full --output-on-failure
```

`test_piv_targets` configures the `piv-acu` and `piv-pd` roles under each
resource profile. RSA
tests exercise each enabled modulus-size gate independently. C++ header tests
compile with features both enabled and disabled so wrappers cannot expose
missing C operations.

## Test evidence

- Unit tests use munit for C and doctest for C++.
- Checked-in known answers cover ordinary crypto regressions without OpenSSL.
- CAVP, ACVP and Wycheproof adapters provide broader algorithm coverage.
- OpenSSL and Python `cryptography` jobs are independent supplemental oracles.
- Sanitizer jobs cover memory safety and undefined behavior. MemorySanitizer
  uses its dedicated Clang build because every linked object must be
  instrumented.
- Fuzz targets retain malformed parser inputs as regression seeds.
- AVR and other embedded jobs prove compilation, linking and static resource
  budgets. AVR known-answer tests also run on an emulated ATmega328P.

The test source is the authoritative case inventory. List current cases with
CTest instead of maintaining a second list in this document.

## Test options

Corpus, oracle, fuzzing, and hardware tests are opt-in CMake options. Inspect
the current names and defaults with `cmake -LAH -N build` after configuring.
External corpus locations use `TINY_CRYPTO_TEST_ECDSA_DSS_DIR`,
`TINY_CRYPTO_TEST_RSA_DSS_DIR`, `TINY_CRYPTO_TEST_EC_CAVP_DIR`,
`TINY_CRYPTO_TEST_WYCHEPROOF_DIR`, `TINY_CRYPTO_TEST_SM_CAPTURE_DIR`,
`TINY_CRYPTO_TEST_TLV_CORPUS`, `TINY_CRYPTO_TEST_TLV_MBEDTLS_SUITE`, and
`TINY_CRYPTO_TEST_UNICODE_DIR`. Optional oracles and integration jobs use
`TINY_CRYPTO_TEST_EC_ORACLE`, `TINY_CRYPTO_TEST_OPENSSL`,
`TINY_CRYPTO_TEST_ESP_ECDSA`, `TINY_CRYPTO_TEST_ESP_SIGNED_IMAGE`,
`TINY_CRYPTO_TEST_PIV_CARD`, and `TINY_CRYPTO_TEST_FULL`.

## AVR builds and budgets

AVR checks compile the public headers and selected operations with the AVR
toolchain, then enforce the configured flash, RAM, stack, and work ceilings.
The `*_compile_avr` tests run when CMake finds `avr-gcc`. With
`qemu-system-avr` also on the path, the `*_qemu_avr` tests run AES, AES key wrap
and a scripted PIV read on an emulated ATmega328P through
`tests/avr/run_qemu.py`.

## ESP-IDF signed image tests

The ESP-IDF tests verify checked-in signed-image fixtures and policy behavior
on the host.

## Vendored vectors

The CAVP, ACVP, Wycheproof, PIV, TWIC, X.509 and EAC vectors are checked in
under `tests/vectors/`. Tests read them from the source tree and download
nothing. [`tests/vectors/README.md`](../tests/vectors/README.md) lists the
collections.

Each corpus root has a provenance README and a recursive `SHA256SUMS`.
`test_vector_manifests` requires every vector file to appear in a manifest and
rejects missing files, unlisted files, entries outside their directory and
digest changes. The Wycheproof subset keeps only the documents the adapters
exercise, and the runner reports the out-of-scope parameter groups it skips.

`make test-full` runs the extended suite over these files. The external
directory options listed under [Test options](#test-options) point a test at
another copy of a corpus.

## Sanitizers and local workflow reproduction

`make test-sanitize` runs the quick suite under AddressSanitizer and
UndefinedBehaviorSanitizer in `build-sanitize`. `make test-sanitize-full` adds
the extended tests. `make test-msan` and `make test-msan-full` use
MemorySanitizer, which needs Clang on Linux.

The CI workflow is the source of truth for compiler flags. Local workflow
reproduction with `act` uses the checked-in sanitizer workflow:

```sh
act workflow_dispatch --bind -W .github/act/cms-sanitizer.yml -j gcc
act workflow_dispatch --bind -W .github/act/cms-sanitizer.yml -j clang
act workflow_dispatch --bind -W .github/act/cms-sanitizer.yml -j msan
```

Run one sanitizer configuration at a time when builds share a directory.
Preserve the complete failing command and seed before reducing a failure.

## Fuzzing

Configure the Clang fuzz build, build the desired targets, then run their
regression tests:

```sh
cmake -S . -B build-fuzz \
  -DTINY_CRYPTO_BUILD_TESTS=ON \
  -DTINY_CRYPTO_BUILD_FUZZERS=ON \
  -DCMAKE_C_COMPILER=clang
cmake --build build-fuzz \
  --target fuzz_tlv fuzz_pki fuzz_ocsp fuzz_piv_apdu fuzz_gzip fuzz_twic
ctest --test-dir build-fuzz -R '^test_fuzz_' --output-on-failure
```

Keep minimized seeds that exercise distinct parser states. Corpus growth needs
the same provenance and manifest checks as other external vectors.

## Arduino, PlatformIO and installed packages

Package checks compile the source-tree `.ino` sketches, pack the PlatformIO
library for inspection, and build CMake consumers from an installation. The
inspected archives must contain public sources, licenses and supported examples
while excluding external corpora. Package size limits catch accidental
repository-wide exports.

`library.properties` supports direct Arduino source imports and build testing.

## Hardware tests

Hardware tests are opt-in and carry the `hardware` CTest label. Configure
`TINY_CRYPTO_TEST_PIV_CARD=ON`, build the named targets, confirm the intended
reader and card are connected, then run only that label:

```sh
cmake -S . -B build-card -DTINY_CRYPTO_TEST_PIV_CARD=ON
cmake --build build-card --target test_piv_card_hardware test_piv_inspect_live
ctest --test-dir build-card -L hardware --output-on-failure
```

Hardware tests may change card authentication state or consume retry counters.
Read the target's help and fixture requirements before running it. Simulator,
host and link evidence remain separate from physical-card execution.

### PIV card hardware tests

PIV card checks require the explicitly configured reader and card fixture.
Run their named targets under the `hardware` label only. The tests skip with
status 77 until `TC_PIV_CARD_READER` names a substring of exactly one reader.
[`tests/piv/hardware/card_config.h`](../tests/piv/hardware/card_config.h) lists
the PIN, pairing code, retry floor, expected card, trust, revocation and
evaluation time variables.

## Regeneration and benchmarks

Vector and table generators live under `tools/` and share the common CAVP
parser and C-array emitter. Regeneration must be deterministic: run it twice
and require a clean `git diff` after the second run. Generator dependencies are
development requirements and are absent from shipped packages.

Benchmarks report flash, RAM, stack and bounded work for the named profile.
Treat them as measurements of that compiler, configuration and target. CI
checks the configured ceilings for each measured board separately.
