# Changelog

## Unreleased

- Moved C sources into src/ and updated build, test, and CI paths.
- Hardened CVSS metric and vector validation for invalid defensive inputs.
- Added regression coverage for invalid metric IDs and corrupt vectors.

## v0.3 - UI Polish

- Switched custom info screen footer hints to native Flipper button elements.
- Tightened result, vector, About, and severity screen layouts.
- Simplified About and severity screens to reduce visual clutter.

## v0.2 - CVSS Examples

- Added built-in CVSS examples for common vulnerability classes.
- Added a severity range reference for CVSS v3.1 learning and score checks.
- Added regression coverage for CVSS scoring, real CVE vectors, and bundled examples.
- Polished result, vector, navigation, and About screen layouts.
- Refreshed README badges, screenshots, and feature summary.
- Added GitHub Actions checks for host tests, formatting, and linting.

## v0.1 - Initial Release

- Initial Pocket CVSS release.
