# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2026-07-05

### Added

- Reset the filter-life counter on Xiaomi air purifier NTAG213 filter tags to 100%.
- UID-derived NTAG password authentication (`SHA-1`-based), verified against real-tag
  golden vectors.
- Single-tap flow: authenticate, clear the page-8 counter, and read back to verify.
- Scenes for start menu, active reset, success, error (with retry), and about/help.
- Host unit tests for the pure core (password derivation, memory map, counter parsing).
- Bundled, dependency-free SHA-1 so host tests exercise the exact shipping code.
- GitHub Actions CI: FAP build (dev + release channels), host tests, format/lint.
- Flipper App Catalog submission manifest under `catalog/`.

[Unreleased]: https://github.com/khmm12/flipper-xiaomi-filter-reset/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/khmm12/flipper-xiaomi-filter-reset/releases/tag/v1.0.0
