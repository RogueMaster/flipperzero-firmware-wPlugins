# Pocket CVSS

<p align="center">
  <a href="https://flipperzero.one/"><img alt="Flipper Zero app" src="https://img.shields.io/badge/Flipper%20Zero-app-blue"></a>
  <a href="https://www.first.org/cvss/v3.1/specification-document"><img alt="CVSS v3.1" src="https://img.shields.io/badge/CVSS-v3.1-blue"></a>
  <a href="https://github.com/vavkamil/pocket-cvss/releases"><img alt="Latest release" src="https://img.shields.io/github/v/release/vavkamil/pocket-cvss?label=release&amp;color=green"></a>
  <a href="https://github.com/vavkamil/pocket-cvss/actions/workflows/build.yml"><img alt="Build success" src="https://img.shields.io/badge/build-success-brightgreen"></a>
  <a href="https://github.com/vavkamil/pocket-cvss/actions/workflows/ci.yml"><img alt="Tests passing" src="https://img.shields.io/badge/tests-passing-brightgreen"></a>
  <a href="https://github.com/vavkamil/pocket-cvss/stargazers"><img alt="GitHub Repo stars" src="https://img.shields.io/github/stars/vavkamil/pocket-cvss"></a>
</p>

Pocket CVSS is a native Flipper Zero app for offline CVSS v3.1 Base scoring and
learning.

Calculate CVE severity offline, browse example vectors, and check CVSS score
ranges in a Flipper-friendly layout.

<table align="center">
  <tr>
    <td><img src="screenshots/01_result.png" width="256" alt="Pocket CVSS score result"></td>
    <td><img src="screenshots/02_severity.png" width="256" alt="Pocket CVSS severity reference"></td>
  </tr>
  <tr>
    <td><img src="screenshots/03_examples.png" width="256" alt="Pocket CVSS examples menu"></td>
    <td><img src="screenshots/04_menu.png" width="256" alt="Pocket CVSS main menu"></td>
  </tr>
</table>

## Features

- Guided CVSS v3.1 Base scoring, fully offline
- Instant score, severity, and canonical vector
- Built-in examples for common vulnerability classes
- Severity reference for quick checks
- Compact screens designed for Flipper navigation

## Build

```bash
ufbt
```

## Run On Flipper

```bash
ufbt launch
```

## Test

Tests cover scoring, CVE vectors, and built-in examples:

```bash
cc -std=c11 -Wall -Wextra -Isrc tests/cvss31_test.c src/cvss31.c -o /tmp/pocket_cvss_tests
/tmp/pocket_cvss_tests
```

## Development

```bash
ufbt format
ufbt lint
```
