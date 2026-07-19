# Changelog

All notable changes to Faraday are documented here.
This project adheres to [Semantic Versioning](https://semver.org/).

## [1.1] — 2026-07-18

### Added

- **Leak Hunt** — a sweep mode that finds *where* a pouch leaks, not just that it does. Seal the fob,
  hold its button, and sweep the seams: a live meter, a `COLD → BLAZING` warmer/colder word, a
  rolling trace of the sweep you just made, and geiger clicks that speed up as you close in. OK
  resets the peak to re-sweep a spot. Everything is measured against the tracked noise floor, so it
  reads the same for a strong or a weak fob.
- **Saved results** — every finished test is appended to `/ext/apps_data/faraday/results.csv` with a
  timestamp, band, both raw levels and the grade. The newest 20 are browsable on-device; pull the
  CSV off with qFlipper to compare pouches properly.
- **Persistent settings** — band, sound and LED survive a reboot, stored with a magic/version/checksum
  so a stale or corrupt file falls back to defaults instead of loading garbage.

### Notes

- The result log is plain CSV and may be hand-edited; unparseable lines are skipped rather than
  displayed as garbage, and a saved band index is range-checked before it is used to index anything.

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
