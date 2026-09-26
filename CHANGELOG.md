# Changelog

All notable changes to Faraday are documented here.
This project adheres to [Semantic Versioning](https://semver.org/).

## [1.3] — 2026-09-26

The correctness release. A ten-dimension audit of the firmware turned up 48 confirmed defects,
including two that made whole features lie: **every NFC pouch graded F**, and the refusal that is
supposed to stop the app grading thin air was passing on bare noise. Both are fixed, and every
picture in the repository is now a capture off a real device rather than a drawing of one.

### Added — find the band for me

- **Find my fob's band.** A fob transmits on one band, and which one depends on where the car was
  sold — 315 MHz across much of North America, 433.92 across Europe, 868 and 915 elsewhere. Picking
  the wrong one measures ambient noise and grades it, which is a confident answer to a question
  nobody asked. Hold the fob against the Flipper and this sweeps all four, then offers to save the
  winner.

  It refuses to guess: a band has to lead the runner-up by 25 dB to be called. That figure is
  measured — with no fob pressed at all, this room read 315 MHz 14 dB above the other three purely
  from ambient traffic, and an earlier 12 dB threshold announced a fob that did not exist. Naming
  the wrong band is not recoverable, because every later measurement is then taken against noise.

  This is *not* the "sweep every band and grade each" idea this app deliberately leaves out. That
  one is meaningless — a fob is only ever on one band, so the other three would report a flattering
  attenuation measured against noise. This finds the one band the fob is actually on and hands it
  to the normal single-band test.

### Fixed — measurements that were wrong

- **Every NFC pouch graded F.** The worker cleared the peak-hold on reset and then immediately
  re-seeded it from its own low-pass, which still held the *baseline* field — so the shielded
  capture read back the baseline no matter how good the pouch was. The reset now drops the
  low-pass too.
- **The NFC test could not be performed at all.** It is the Flipper that goes in the pouch, so the
  user cannot see the screen or press OK while the measurement is being taken; peak-hold was
  re-latched by the bare reader field on the way in and again on the way out. The shielded half now
  runs on a timer — arm, measure, freeze — with a countdown on screen, and the frozen peak survives
  taking the Flipper back out.
- **The "did anything actually transmit" gate was decorative.** It required 8 dB over the noise
  floor. Measured on a quiet 433.92 MHz channel with nothing transmitting, peak-hold reaches
  +13 dB within five seconds and +14 dB by ninety — so a baseline made of pure noise sailed
  through. The gate is now 20 dB, chosen from that measurement.
- **The noise floor climbed onto the carrier it was measuring.** It drifted upward on every sample
  regardless of what was on air, so a fob held down for a second dragged the floor up onto its own
  transmission: the app then decided nothing was transmitting, refused to lock while the user was
  still pressing the fob, and shrank every attenuation measured against it. The floor now only
  creeps while the channel is genuinely quiet, paced by a sample counter rather than a wall-clock
  coincidence.
- **Both trackers were seeded from a display constant.** `FDY_RSSI_MIN` is the meter's scale, not
  this room's ambient. Seeding from it reported a permanent phantom carrier on a quiet band and an
  unrecoverably low floor on a noisy one. They now prime from the first real sample.
- **The Leak Hunt trace showed 0.13 seconds.** The ring advanced once per 2 ms sample, so the
  "rolling trace of the sweep you just made" covered about a tenth of a second. It now spans ~7.7 s.
- **Leak Hunt went quiet exactly when you found the leak.** The click interval underflowed past a
  38 dB margin and clamped to the *slowest* rate, the opposite of what the hunt is for.
- **A finished test could vanish silently.** The result-log write returned a status that was
  discarded, so a missing or full SD card lost the measurement with no warning. It now says so.

### Fixed — controls that did not respond

- **A long press on OK did nothing.** The measurement and leak-hunt screens handled `InputTypeShort`
  only, and the firmware emits `InputTypeLong` (never Short) once a key is held past the long-press
  threshold — so pressing OK a moment too firmly was silently dropped, on the one control the whole
  test depends on.
- **Both radio workers starved the UI.** They paced their sampling loops with `furi_delay_us()`, a
  non-yielding busy-wait, and ran at normal thread priority. The GUI service stopped draining its
  input queue and the app appeared to ignore buttons. Now paced in kernel ticks at low priority.
- **A refused OK press changed nothing on screen.** It was a beep and nothing else, which from the
  user's side is indistinguishable from a dead button. The action strip now inverts and says why.
- **The NFC "busy" screen was a dead end.** The worker gave up on the first refused acquire, leaving
  the error flag set forever with no thread behind it; OK was swallowed and no key was advertised.
  The worker now retries until the chip is free, so the screen heals itself.
- **Double-tapping to skip the intro opened the Sub-GHz test.**

### Added — so the result means something

- **The ceiling, before you commit.** A shielded reading can never sink below the noise floor, so
  the most attenuation a test can *prove* is (peak − floor). A baseline 55 dB above the floor caps
  the result at A however good the pouch is. The baseline screen now shows `max A+` / `max A` while
  moving the fob closer still helps, and a floor-limited verdict is captioned `FLOOR LIMIT` so an A
  that hit the floor never reads as a verdict on the pouch.
- **Which object goes in the bag.** The two tests are opposites — Sub-GHz shields the *fob*, NFC
  shields the *Flipper* — and getting it the wrong way round produces a confident, meaningless
  grade. An opening card on each test says which, the phase label reads `fob in bag` / `Flipper in
  bag`, and About has a section on it.
- **The number the lock is actually made on.** The bar tracks the live reading while the lock tracks
  the peak, so an empty bar beside a strip reading "Signal found" looked like the app contradicting
  itself. The peak margin is now on screen as `PK +N dB`.
- **The grade scale, on the device.** `fdy_rating_blurb()` had been written, unit-tested and shown
  to nobody. About now builds the A+..F table from the grading engine's own thresholds, so it
  cannot drift from the code.
- **Leak Hunt says what to do.** An opening card, and the margin is labelled `+N dB` instead of a
  bare `+N` in the corner.

### Changed

- The measurement screen's header names the test (`SUB-GHZ` / `NFC`) instead of repeating the app
  name, which was the only thing distinguishing the two capture faces apart from the band.
- The verdict's open-air bar is labelled `AIR`, not `OPEN` — `OPEN` is also the one-word verdict for
  grade F, so on a failing test the same word appeared twice meaning opposite things.
- A shielded reading that is not weaker than the baseline is captioned `NO DROP` rather than
  printing a confident `0 dB`; it is a measurement that did not work, not zero attenuation.
- `"Peak captured"` became `"Signal found"`. It claimed a capture the user had not made yet, so the
  one screen that needs them to press OK read as already finished.
- Every heading rendered a stray `#`. The SDK documents `\e#` as a line prefix ("until next
  `\n`"), not a wrapper, so the closing one was printing literally.
- Settings shows the band with its unit.

### Fixed — packaging

- **`fap_version` had been stuck at 1.1 since the 1.2 release**, so the version the app catalog
  showed was a release behind the one the firmware compiled and the About screen printed.
- **Releases only shipped the release-channel build**, which the loader refuses on Unleashed,
  RogueMaster and Momentum (`APP:87 < FW:88`). Both channels are now built and attached.
- CSV rows loaded from the SD card are range-checked before being shown as measurements, log reads
  are bounded, and the settings file's booleans are normalised on load.

### Tooling

- **`tools_screenshot.py`** — captures every screen and GIF from a real Flipper over the protobuf
  RPC session. The hand-drawn mockup renderer it replaces has been deleted: a drawing of the UI is
  a second implementation that can disagree with the firmware while looking convincing.
- **`tools_check_text.py`** — measures every on-screen string against the 120 px it is drawn in.
  Calibrated against a device capture; it found twenty overflowing lines and every stray escape.
- **`tools_check_meta.py`** — asserts the version, icons and catalog images agree with each other.
- **`tools_gen_banner.py`** — rebuilt in the house style, reading the version and the grade
  thresholds out of the firmware's own headers, with the device's orange quarantined to real
  captures and the accent reserved for measurement. Both laws are asserted on the rendered pixels.
- Both checkers run in CI.

## [1.2] — 2026-07-19

Polish release — same measurements, a much nicer app to use.

### Added

- **Animated launch splash** — a branded intro on first open: the pouch containing a fob's radiating
  waves, the wordmark and tagline, and a progress bar that auto-advances (~1.7 s). Any key skips it.
  It plays once per launch and never replays when you back out of a test.
- **Verdict that feels earned** — the comparison bars grow in, the attenuation figure tallies up from
  zero and the pips fill as the result lands, instead of snapping on all at once.
- **Pass/fail badge treatment** — a B-or-better grade is shown in a *filled* badge (a reward); a
  poorer grade is only outlined (a warning). A top A+ grade gets a small corner sparkle.
- **Livelier capture screens** — a marker sweeps the empty meter while the app is listening, and a
  live sparkline traces the signal under the bar during the baseline capture.

### Notes

- Purely presentational. No measurement, threshold, radio path or saved-result format changed, so
  v1.1 result logs remain valid and comparable.

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
