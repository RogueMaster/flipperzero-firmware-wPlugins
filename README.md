# Pocket CVSS

<p align="center">
  <a href="https://flipperzero.one/"><img alt="Flipper Zero app" src="https://img.shields.io/badge/Flipper%20Zero-app-blue"></a>
  <a href="https://www.first.org/cvss/v3.1/specification-document"><img alt="CVSS v3.1" src="https://img.shields.io/badge/CVSS-v3.1-blue"></a>
  <a href="https://github.com/vavkamil/pocket-cvss/deployments/activity_log?environment=github-pages"><img alt="Release status" src="https://img.shields.io/github/deployments/vavkamil/pocket-cvss/github-pages?logo=github&amp;label=release"></a>
  <a href="https://github.com/vavkamil/pocket-cvss/actions/workflows/build.yml"><img alt="Build status" src="https://img.shields.io/github/actions/workflow/status/vavkamil/pocket-cvss/build.yml?branch=main&amp;label=build"></a>
  <a href="https://github.com/vavkamil/pocket-cvss/actions/workflows/ci.yml"><img alt="Checks and tests status" src="https://img.shields.io/github/actions/workflow/status/vavkamil/pocket-cvss/ci.yml?branch=main&amp;label=checks%20%2F%20tests"></a>
  <a href="https://github.com/vavkamil/pocket-cvss/stargazers"><img alt="GitHub Repo stars" src="https://img.shields.io/github/stars/vavkamil/pocket-cvss"></a>
</p>

Pocket CVSS is a native Flipper Zero app for learning and calculating CVSS v3.1 Base
scores offline.

It provides a compact metric wizard, calculates the Base score, and shows the resulting
severity and vector in a Flipper-friendly layout.

| | |
|---|---|
| <img src="screenshots/01_menu.png" width="256" alt="Pocket CVSS main menu"> | <img src="screenshots/02_examples.png" width="256" alt="Pocket CVSS examples menu"> |
| <img src="screenshots/03_result.png" width="256" alt="Pocket CVSS score result"> | <img src="screenshots/04_severity.png" width="256" alt="Pocket CVSS severity reference"> |

## Features

- CVSS v3.1 Base metric wizard for offline scoring
- Automatic score, severity, and canonical vector formatting
- Built-in educational examples for common vulnerability classes
- Severity range reference for quick learning
- Compact Flipper-friendly result, vector, and navigation screens

## Build

```bash
ufbt
```

## Run On Flipper

```bash
ufbt launch
```

## Test

Host-side tests cover the CVSS scoring engine, real CVE vectors, and the educational
examples used by the app:

```bash
cc -std=c11 -Wall -Wextra -I. tests/cvss31_test.c cvss31.c -o /tmp/pocket_cvss_tests
/tmp/pocket_cvss_tests
```

## Development

```bash
ufbt format
ufbt lint
```
