# PocketCVSS

PocketCVSS is a native Flipper Zero app for learning and calculating CVSS v3.1 Base
scores offline.

It provides a compact metric wizard, calculates the Base score, and shows the resulting
severity and vector in a Flipper-friendly layout.

Current scope:

- CVSS v3.1 Base metrics
- Base score and severity calculation
- Canonical vector formatting
- Compact score explanations

## Build

```bash
ufbt
```

## Run On Flipper

```bash
ufbt launch
```

## Test

The scoring engine can be checked on the host without building the Flipper app:

```bash
cc -std=c11 -Wall -Wextra -I. tests/cvss31_test.c cvss31.c -o /tmp/pocket_cvss_tests
/tmp/pocket_cvss_tests
```

## Development

```bash
ufbt format
ufbt lint
```

## License

MIT
