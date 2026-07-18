# Changelog

All notable changes to Faraday are documented here.
This project adheres to [Semantic Versioning](https://semver.org/).

## [1.0] — 2026-07-18

First public release.

### Added

- **Sub-GHz shielding test** — measures a key fob's carrier in real dBm on the internal CC1101
  across 315 / 433.92 / 868.35 / 915 MHz, and reports the pouch's attenuation in **dB**.
- **NFC shielding test** — measures how much of an external 13.56 MHz reader field the pouch keeps
  out, using the onboard ST25R3916 external-field detector. Scored in **% of field blocked**.
- **Baseline → Shielded → Verdict flow** — two OK presses per test, with peak-hold doing the work.
- **Grading engine** — `A+ SEALED` through `F OPEN`, each with a plain-English verdict line.
  Separate scales for the dB and percentage measurements.
- **Noise-floor tracking** — powers two honest behaviours: refusing to lock a baseline when the fob
  never transmitted, and reporting `>= N dB` when the shielded signal sinks below the noise floor.
- **Settings** — Sub-GHz band selection, sound and LED feedback toggles.
- **About** — the method and its limitations, on-device.
- **Host unit tests** for the grading engine (`make -C test`), run in CI on every push.

### Notes

- Listen-only: Faraday never transmits on either radio.
- Built against Flipper SDK Target 7, API 87.1; CI covers both the `release` and `dev` channels.
