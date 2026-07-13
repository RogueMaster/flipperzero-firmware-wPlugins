# Changelog

All notable changes to Rosetta are documented here.
This project adheres to [Semantic Versioning](https://semver.org/).

## [1.0] — 2026-07-11

First public release.

### Walkthroughs (animated)
- **Mifare Auth** — 5 steps: RF field & ATQA, anticollision & SAK, auth request,
  the 3‑pass Crypto1 handshake (cycling through its three legs), and why it's been
  broken since 2008 (padlock pops open).
- **OOK & PSK** — 4 steps: the carrier, On‑Off Keying, Phase‑Shift Keying, and
  recovering bits from the envelope, drawn as live scrolling waveforms.
- **1‑Wire** — 5 steps: one line & parasite power, reset/presence, write slots,
  read slots, and the 64‑bit ROM clocking out, drawn as scope‑style timing
  diagrams with a sweeping playhead.
- Step navigation with **Left / Right / OK**, step pips, and per‑step captions.

### Live capture‑and‑annotate
- **NFC** — read‑only anticollision: technology, UID, SAK/ATQA, with a
  clone‑risk note for 4‑byte UIDs.
- **iButton** — `READ ROM (0x33)`, split into family / 48‑bit serial / CRC, with
  an on‑device Maxim CRC‑8 validation verdict.
- **Sub‑GHz** — live RSSI envelope scope with an adaptive carrier threshold;
  frequency selectable (433.92 / 315 / 868.35 / 915 MHz).

### App
- Scene‑based UI: main menu → per‑protocol menu → walkthrough / live capture.
- Settings for Sound / Vibro / LED feedback and RF scope frequency.
- About screen, custom 10×10 icon, GitHub banner + social card + screen mockups.
- Builds against official firmware **fw 7 / API 87.1** with `ufbt`; CI on
  release and dev channels.
