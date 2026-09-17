# Contributing to tiny-DES-c

Thank you for your interest in contributing. This guide covers technical
contribution expectations for this heap-free DES / Triple-DES library.

## Contribution policy

This project uses the
[Bounded Contribution Policy](CODE_OF_CONDUCT.md)
([OpenPhysical BoundedContributionPolicy](https://github.com/OpenPhysical/BoundedContributionPolicy)
v2.0.3) to keep work focused on technical objectives and to evaluate
contributions by technical merit.

### Key principles

- **Technical relevance** — work must support library correctness, size/RAM,
  modes/CMAC, tests, builds, or documentation of the API and its limits
- **Individual evaluation** — assessed on technical merit, not identity
- **Clear communication** — specific, actionable reports and proposals
- **Project focus** — neutral, engineering-oriented discussions only

### Sister project

[tiny-AES-c](https://github.com/mistial-dev/tiny-AES-c) is the AES sibling. Prefer
aligning API style, build knobs, and docs with that project when the change
applies to both families of libraries.

## Getting started

### Prerequisites

- C99 toolchain (`cc` / `gcc` / `clang`)
- GNU Make and/or CMake 3.10+
- Optional: Python 3 for regenerating checked-in vector headers
  (`make regenerate-vectors`; needs OpenSSL / cryptography for edge vectors)

### Build and test

```text
make                    # des.o (default CTR + TDES)
make test               # full-mode tests + sparse configs
make DES_CAVP=1 test    # optional full TDES CAVP (long)
make size
make benchmark
```

CMake:

```text
cmake -S . -B build -DTINY_DES_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Types of contributions welcome

- Bug fixes with reproduction steps and, where possible, test coverage
- Mode, key-size, or API fixes that preserve MCU-friendly defaults
- Performance or size improvements with measurements (`make size` / benchmark)
- Documentation of security constraints, IV/nonce rules, and residual risks
- Test vectors, CAVP/edge-case coverage, multi-config CI improvements
- Platform packaging (PlatformIO, etc.) that does not bloat the core library

## Out of scope (examples)

- Protocol design, key management, padding schemes, or full TLS stacks
- Social, political, or off-topic discussion unrelated to the implementation
- Drive-by refactors that break sibling consistency without technical gain

## Pull requests

1. Fork and branch from `main`
2. Keep changes focused; avoid unrelated formatting churn
3. Add or update tests for behavioral changes
4. Prefer `DES_ENABLE_*` and status-code patterns already used in the tree
5. Fill out the PR template and note any footprint or API impact

## Review criteria

Contributions are evaluated on technical correctness, maintainability, alignment
with default size/safety goals, test coverage, and documentation of security
trade-offs.

## Questions

- Prefer GitHub issues with the provided templates for bugs and features
- For policy questions, see [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)
