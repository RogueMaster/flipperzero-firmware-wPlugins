# Contributing to tiny-AES-c

Thank you for your interest in contributing. This guide covers technical
contribution expectations for this heap-free AES library (Mistial Dev fork of
kokke/tiny-AES-c).

## Contribution policy

This project uses the
[Bounded Contribution Policy](CODE_OF_CONDUCT.md)
([OpenPhysical BoundedContributionPolicy](https://github.com/OpenPhysical/BoundedContributionPolicy)
v2.0.3) to keep work focused on technical objectives and to evaluate
contributions by technical merit.

### Key principles

- **Technical relevance** — work must support library correctness, size/RAM,
  modes/AEAD/MAC, tests, builds, or documentation of the API and residual risks
- **Individual evaluation** — assessed on technical merit, not identity
- **Clear communication** — specific, actionable reports and proposals
- **Project focus** — neutral, engineering-oriented discussions only

### Sister project

[tiny-DES-c](https://github.com/mistial-dev/tiny-DES-c) is the DES/3DES sibling.
Prefer aligning API style, build knobs, and docs with that project when the
change applies to both families of libraries.

## Getting started

### Prerequisites

- C99 toolchain (`cc` / `gcc` / `clang`)
- GNU Make and/or CMake 3.12+
- Optional: Python 3 for vector tooling used by some AEAD tests

### Build and test

```text
make                    # aes.o (default CTR)
make test               # mode/profile matrix
make AES_CAVP=1 test    # optional full CAVP (long)
make size
make benchmark AES_ENABLE_GCM=1
```

CMake:

```text
cmake -S . -B build -DTINY_AES_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Types of contributions welcome

- Bug fixes with reproduction steps and, where possible, test coverage
- Mode, key-size, GHASH/S-box profile, or API fixes that preserve MCU defaults
- Performance or size improvements with measurements (`make size` / benchmark)
- Documentation of IV/nonce rules, AEAD contracts, and residual risks
- CAVP/Wycheproof coverage and multi-config CI improvements
- Platform packaging that does not bloat the core library

## Out of scope (examples)

- Full TLS, certificate stacks, or application protocols
- Side-channel lab work claimed as “fixed” without portable, reviewable design
- Social, political, or off-topic discussion unrelated to the implementation
- Drive-by refactors that break sibling consistency without technical gain

## Pull requests

1. Fork and branch from the default branch (`master` unless otherwise noted)
2. Keep changes focused; avoid unrelated formatting churn
3. Add or update tests for behavioral changes
4. Prefer existing compile-time gates (`CBC`, `GCM`, `AES_ZEROIZE`, etc.) and
   `AES_OK` / `AES_ERR` patterns
5. Fill out the PR template and note any footprint or API impact
6. Update [CHANGELOG.md](CHANGELOG.md) for user-visible changes

## Review criteria

Contributions are evaluated on technical correctness, maintainability, alignment
with default size/safety goals, test coverage, and documentation of security
trade-offs.

## Questions

- Prefer GitHub issues with the provided templates for bugs and features
- For policy questions, see [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)
