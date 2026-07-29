# Changelog

## v1.1

Polish pass — same honest dual-path engine, a nicer instrument around it.

### Added
- **Animated boot intro** — a Nyx eye opening through expanding IR wave-rings.
  Runs ~1.6 s; any key skips it.
- **Settings persistence** — Mode, Sensitivity, probe pin, and sound / vibro /
  LED are saved to `/data/settings.bin` and restored on the next launch, via the
  firmware's `saved_struct` (a version bump or corrupt file falls back to
  defaults). New file: `helpers/nyx_store.c`.

### Changed
- **Redesigned Sweep screen** around an **eye that watches back**: the iris ring
  fills clockwise with the live level, the pupil dilates as you close on a
  source, a tick marks your best reading so far, and lock-on glare spikes rotate
  around the iris. A larger get-warmer trend arrow and a cleaner right-hand
  readout (source label · proximity state · peak / hit count) replace the old
  number-and-sparkline layout. The honest "onboard sees pulsed IR only" hint and
  the inverted alarm strip are unchanged.
- README and mock screenshots updated to match; added a splash mockup.

### Unchanged
- Detection engine, the two-mode model, and every honesty note. Onboard mode is
  still deaf to steady DC illuminators and still says so; Probe mode is still the
  one that finds night-vision cameras. Listen-only — Nyx never transmits IR.

## v1.0

Initial release. Dual-path IR emitter sweep for counter-surveillance:

- **Onboard mode** — the built-in TSOP-75338 receiver detects pulsed / modulated
  IR (remotes, beacons, PIR floodlights, PWM'd illuminators). Honest that its
  38 kHz band-pass makes it blind to steady DC illuminators.
- **Probe mode** — an optional ~$1 IR phototransistor on a GPIO ADC pin reads DC
  IR level and classifies the source STEADY / FLICKER / PULSED.
- Sweep meter, on-device probe wiring guide + live check, settings, about.
- Listen-only. `ufbt` + CI verified against Flipper API 87.1.
