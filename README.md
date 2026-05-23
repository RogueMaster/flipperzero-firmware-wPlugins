# Pocket CVSS

Pocket CVSS is a native Flipper Zero app for learning and calculating CVSS v3.1 Base
scores offline.

It provides a compact metric wizard, calculates the Base score, and shows the resulting
severity and vector in a Flipper-friendly layout.

<p>
  <img src="screenshots/01_menu.png" width="256" alt="Pocket CVSS main menu">
  <img src="screenshots/02_severity.png" width="256" alt="Pocket CVSS score result">
  <img src="screenshots/03_examples.png" width="256" alt="Pocket CVSS examples menu">
  <img src="screenshots/04_vector.png" width="256" alt="Pocket CVSS vector screen">
  <img src="screenshots/05_severity.png" width="256" alt="Pocket CVSS severity reference">
</p>

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

Host-side tests cover the CVSS scoring engine, real CVE vectors from CVEalert.io,
and the educational examples used by the app:

```bash
cc -std=c11 -Wall -Wextra -I. tests/cvss31_test.c cvss31.c -o /tmp/pocket_cvss_tests
/tmp/pocket_cvss_tests
```

## Development

```bash
ufbt format
ufbt lint
```
