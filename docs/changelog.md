# Changelog

## 2.7

- **Releases now ship two builds.** A `.fap`'s API version is fixed when it is compiled, and the loader warns when it trails the firmware's (`APP:87 < FW:88 — This app might not work`). Official firmware tracks API 87 while Unleashed / RogueMaster / Momentum track 88, so one build cannot satisfy both. Releases now carry `specter.fap` for official firmware and `specter-fw-dev.fap` for the newer line. Reported by @drdelaney in [#1](https://github.com/at0m-b0mb/Specter-FlipperZero/issues/1).

- **Fix: the `READER PRESENT` band strobed.** It alternated between filled and outlined on every UI tick — at a 100 ms tick that is a **5 Hz flash across the full width of the screen**. Unpleasant to look at, harder to read, and no more attention-grabbing than a steady block. The band is now solidly inverted, with a small marker pulsing at about 1 Hz as the "this is live, not frozen" cue.
- **Fix: `READER PRESENT` lingered ~2 seconds after the reader was taken away.** Presence is deliberately latched so a polling reader's quiet gaps don't read as it disappearing, but that latch was pinned at a blanket 1500 ms — sized for the slowest imaginable emitter. It now derives from the reader's *own measured polling period* (2.5 cycles, clamped to 600–1500 ms), so a typical reader releases in well under a second while a slow poller stays just as stable.

## 2.6

- **Fix: the divider line cut through the `NOISE FLOOR` text** during a noise-floor scan. `FontSecondary` occupies rows `[baseline-7 .. baseline]`, so a baseline of 59 put the glyph tops on row 52 — exactly where the strip divider is drawn. The text moved down a row and the progress bar became a plain fill along the bottom edge instead of a framed box that would then clip it from below.
- **Fix: the `ACTIVE READER` alarm text had its bottom pixel row erased** by the inner alarm frame, whose bottom edge runs along row 62. Found by the new checker, not reported — it had been there since 1.0.
- **New: `tools_check_layout.py`**, a static checker that reads the view sources and reports every drawing primitive whose vertical band overlaps another's. Two collisions had already reached users because nothing verified this and the mockup renderer's desktop font sits a pixel shorter than the device's, so it drew both as "fine". CI now pins the candidate count, so a new overlap fails the build.

## 2.5

- **Fix: the app could lock out every button and force a Flipper reboot** (reported in [#1](https://github.com/at0m-b0mb/Specter-FlipperZero/issues/1), most easily triggered by pressing keys during a noise-floor scan). The sampling worker paced itself with `furi_delay_us()`, which the firmware documents as a DWT busy-loop — it never yields to the scheduler. The thread therefore held the CPU at 100% for the whole scan, starving the GUI and input services; once the view dispatcher stopped draining its input queue quickly enough, the GUI thread blocked posting into it and took the entire UI down with it. The worker now sleeps with `furi_delay_tick()` and runs at low priority, below the UI. This affected every version since 1.0.
- **Fix: no way to stop a noise-floor scan.** `OK` now cancels one in progress, and the scan strip says so.
- **Fix: settings were written to the SD card from the event-loop thread** the instant a calibration finished — card I/O on the hot path, with the radio still sampling, exactly when the user is most likely pressing keys. The result is applied immediately and the write is deferred until the scan screen is closed and the worker has stopped.
- The noise-floor strip now reads `NOISE FLOOR … OK=cancel` instead of a bare status line, so it is clear what is happening and how to get out of it.

## 2.4

- **Fix: Watch Mode flickered between `READER PRESENT` and `CLEAR NOW` with a reader sitting right there.** Presence was decided one ~96 ms sampling window at a time, but readers *poll* — burst, sleep, burst — so consecutive windows legitimately alternated between "carrier seen" and "nothing". Presence is now latched and only released after 1.5 s of genuine silence, so a steady reader reads steady.
- **Fix: the app could lock up in Watch Mode and refuse to exit.** Same root cause. Every flicker edge counted a fresh contact and fired an alert sequence plus a screen wake; those are queued to the notification service with an unbounded wait, so posting a ~200 ms sequence every ~200 ms eventually filled the queue and blocked the GUI thread. Debouncing fixes the cause, and the alert paths are now rate-limited as a second line of defence so no radio input can produce an unbounded rate of notifications.
- **Fix: the meter stopped at 89–91% even resting on a reader.** Full scale was set at 35% raw duty; measurements on real hardware put a contactless terminal at 30–32%. Full scale is now 30%, so sitting on a reader reads `100% / MAX`.
- **Fix: `89%` and the `PK…` line overlapped by a pixel** on the Sweep screen, which read as one smudged block. The big number now sits clear of the row beneath it, and the contact count is clamped so a long run can't run past the panel edge.
- **New: warmer/colder trend arrow** on the Sweep screen (▲ / ▼ / –). While hunting by hand this matters more than the absolute reading.
- **New: `SEEN` total** in Watch Mode — how long a carrier was actually up across the whole watch, which is the figure you want when you come back to a Flipper you left somewhere.
- README rewritten around **what each of the five modes is for**, with a summary table and per-mode controls.
- Presence debouncing is a pure, host-tested layer (`helpers/present_hold.h`), including tick-counter wraparound; the suite is now 300 checks.

## 2.3

- **Fix: the field meter never went above ~31%, even resting on a reader.** The gauge was showing raw carrier duty-cycle. Readers *poll* — a burst, a sleep, another burst — so a typical terminal only radiates 20–35% of the time and the raw number **saturates** near 30% no matter how close you get. Nothing was mis-detected; it was displayed on the wrong scale. The meter is now mapped against that real polling band, so sitting on a reader reads **~90–100%** instead of 31%.
- Two knock-on bugs fixed by the same change: the proximity words `CLOSE` and `STRONG` were **unreachable** (they need ≥45/≥70 on a scale that stopped at ~31, so it only ever said `FAINT`/`NEAR`), and Site Survey's "peak ≥ 50" test for an `ACTIVE READER` verdict could **essentially never fire** for a polling reader — the exact device Specter is built to find.
- New `MAX` proximity word: the meter says when it is **pegged**, so a needle that stops moving reads as saturation rather than a fault.
- New **Settings → Meter** toggle: `Boost` (default, full-scale) or `Raw` (the literal duty-cycle, as before).
- The raw duty is still the source of truth everywhere it matters — noise floor, auto-calibration, the emitter classifier and the Fingerprint screen's `DUTY` all continue to work in true duty-cycle.
- Meter scaling is a pure, host-tested layer (`helpers/field_scale.c`); the suite is now 284 checks.

## 2.2

- **Fix: `BACK` could look dead when leaving a stealth screen.** Stealth force-darkens the display, and the old exit path only *unlocked* that force — it didn't turn the backlight back on — so a `BACK` press landed you on an unlit menu that looked frozen. Exiting a stealth screen now actively re-lights the display.
- The `BACK` contract is now explicit in every capture view (Sweep/Fingerprint/Survey/Watch): it always bubbles to the scene manager and can't be swallowed by the OK long-press handling.

## 2.1

- **Watch Mode** — an unattended monitor that stands guard between Survey and Logbook: a large mm:ss clock, live contact count, first/last-seen and peak field, plus a blinking "READER PRESENT" band. It wakes the screen the moment a reader appears and auto-logs new contacts (rate-limited). Watch never enters stealth, so it can alert you.
- **Live CSV logging** — the logbook is now written as both a grouped `logbook.txt` and a spreadsheet-friendly `logbook.csv` (`timestamp,type,detail`). Commas and newlines inside a field are scrubbed so columns never shift.
- Sensitivity level (`S:High` etc.) is now shown on the idle Sweep strip, width-measured so the waveform can't overlap it.

## 2.0

- **Fingerprint** — classifies a detected reader's polling cadence (CONTINUOUS / POLLING / INTERMITTENT) with a confidence readout and a logic-analyzer-style pulse train.
- **Site Survey** — sweeps a room over time and returns a CLEAN / TRACE / ACTIVE verdict card.
- **Logbook** — timestamped, RTC-stamped detection history saved to the SD card and browsable on-device.
- **Settings** with persistent config, **Stealth mode** (backlight + LED suppressed, sound/vibe kept), and LEFT-on-Sweep **noise-floor auto-calibration** saved as a Custom sensitivity.
- Hold-OK on Sweep/Fingerprint logs the current reading.

## 1.0

- Initial release. Passive **Sweep** detector for active 13.56 MHz NFC reader/skimmer fields using the onboard NFC chip — analog-style EMF gauge with sweep needle, peak-hold, hot-zone, live waveform, an "ACTIVE READER" alarm, and optional geiger clicks that speed up with field strength. Listen-only; never transmits.
