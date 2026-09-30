# Changelog

## 3.1.1

One fix, reported within the hour by the first person to watch the thing boot.

- **Fix: the boot intro flashed the whole screen black.** The "contact" beat -
  the moment the poll is found - inverted all 128x64 for two ticks. It was
  reported as a bug, and that is the right verdict: a panel going entirely
  black during startup does not read as emphasis, it reads as the display
  glitching or the app crashing, which is a terrible thing for an intro to
  imply about a tool you are about to trust. Only the band the carrier lives in
  inverts now - rows 34-52, clear of everything drawn at that moment. Same
  beat, same meaning, unmistakably part of the drawing rather than part of the
  hardware.

  The general lesson is worth keeping: on a screen this small, **an effect that
  covers everything is indistinguishable from a fault**. Emphasis has to be
  local to the thing being emphasised, or it stops being emphasis.

Off-device, in the same pass: the README and the project site were each showing
several captures twice (a contact sheet plus the same screens again as per-mode
stills), and the site's Didone headings were pinned to a sturdier optical size
because a hairline on true black stops being a thin line and starts being an
intermittent one.

## 3.1

A boot intro, a real capture pipeline, and the end of the mock-ups.

- **New: a boot intro.** Specter now opens by drawing itself. A trace writes
  across the screen from the left, flat and silent - what a clean room looks
  like on this instrument - then a reader's poll cuts in, the screen inverts
  for a fifth of a second, and the SPECTER nameplate engraves itself a letter
  at a time. The inversion is deliberately the same gesture the Sweep screen
  makes when it locks on, so the intro and the instrument share a vocabulary,
  and the waveform is generated from `SPECTER_FULL_SCALE_DUTY` - the duty cycle
  the whole meter is scaled against - rather than from a shape that merely
  looks good. Any key skips it, and **Settings -> Intro** turns it off for good.
- **Fix: Fingerprint claimed 100% confidence that there was nothing there.**
  With no carrier present the screen drew a completely full confidence bar and
  "CONF 100%" next to "NO FIELD" - pixel-for-pixel the same loud, solid block
  it draws for a nailed-on POLLING reader. The classifier is right (within the
  noise floor, silence is the one thing it can be certain of) but the screen
  was using its most emphatic element to say "strong finding" on the state that
  means "nothing here". The bar and the readout are now simply absent when
  there is no field; `NO FIELD` over `No carrier` already says it. The
  classifier is untouched, so nothing downstream changes.
- **Settings survive a version bump now.** Adding a field changes
  `sizeof(SpecterSettings)`, and `saved_struct` validates size as well as
  version - so every previous release quietly reset everyone's sensitivity,
  survey length, stealth and logging preferences as the price of one new
  option. The old layout is the exact prefix of the new one, so it is now tried
  as a fallback and copied forward. This is the last release that will lose
  your settings, and it does not lose them either.
- **New: `tools_screenshot.py` - every image in this repository now comes off
  the device.** It drives the Flipper over its own protobuf RPC session,
  injects key presses, and pulls the framebuffer back: `--all` walks every
  screen, `--reader` captures the ones that need a live reader held against
  the back of the unit, `--splash` records the intro from the launch request
  onwards, and `--tour-gif` records one continuous GIF of the app being used.
  Captures land at 4x in the panel's own two colours, which is what both the
  Apps Catalog and the branding rules want.
- **Removed: `tools_gen_mockups.py` and `tools_gen_gif.py`.** A renderer that
  draws the UI is a second implementation of it, and a second implementation
  disagrees with the firmware sooner or later while still looking completely
  convincing - which is exactly what happened in 3.0.1, where mock-ups drawn
  at the wrong glyph advance hid a readout running off the right edge of the
  screen for several releases. There is nothing left in the project that can
  draw a screen Specter cannot produce.
- **Fix: a CONTINUOUS emitter printed polling timings it does not have.** An
  unbroken carrier has no period, burst or jitter - that is what "Always on"
  means - but the cadence figures were retained from before the transition, so
  a reader that stopped polling and held its field up showed a full set of
  stale timings underneath a verdict denying they exist. The classifier also
  forces `timing_reliable` for that class, which suppressed the `~` that marks
  an unresolved number: stale timings, stated at full confidence. Those three
  rows now read `--`, and UP still prints, because duty is the one figure that
  does mean something for a continuous carrier.
- **Fix: `Meter scale = Duty %` put CLOSE, STRONG and PEGGED out of reach.**
  On that setting the displayed strength is raw carrier duty, which tops out
  around 30 on a live terminal - so three of the five proximity words could
  never appear, and the geiger clicks never got faster than their slowest
  third. That is exactly the unreachable-vocabulary bug 2.3 fixed for the
  default scale, re-created by the setting added alongside it. Proximity and
  click rate are now judged on the canonical scale, for the same reason
  `peak_ref` already was: how close you are to a reader is a fact about the
  room, not a display preference.
- **Fix: Site Survey ran a countdown over its own fault screen.** The header
  was drawn before the error early-return, so "NFC radio busy" was served with
  a live timer ticking down above it, which then froze at 0:00 and sat there
  for the life of the scene - a screen claiming simultaneously to be measuring
  and to be broken. It now shows the shared `NFC BUSY` state word like every
  other measurement screen. And because the acquire failure is sticky, OK was
  a silent no-op on the one card that tells you to close the other app *and
  retry* - OK is now that retry.
- **Fix: the CLEAN verdict card argued with itself.** It printed PEAK, AVG and
  UP - none of which are gated by the sensitivity threshold, so an ordinary
  room's noise puts a few percent on them - directly above the flat sentence
  "No field detected". The advice now reads **"Nothing above floor"**, which is
  what CLEAN actually decided and what the numbers above it are consistent with.
- **Fix: Watch hid `OK=re-arm` exactly when it mattered.** The hint was drawn
  only in the not-present branch, so the one state where a short OK destroys
  the most - an overnight record, mid-alarm - was the only state where nothing
  on screen warned that OK destroys anything. It now takes the footer slot that
  `NOW %` had, because during an alarm `NOW %` is the redundant one: the
  strength bar under the banner is the same measurement, readable across a room.
- **Fix: the logbook described the meter in words the device never shows.**
  Findings were stamped `m:boost` / `m:raw` while Settings said `0-100` /
  `Duty %` - names the UI deliberately stopped using in 3.0, because
  Boost/Raw read as a quality setting rather than as a scale. Both now come
  from one table, so they cannot drift apart again.
- **Fix: the README documented a proximity word that does not exist.** It
  listed `FAINT -> NEAR -> CLOSE -> STRONG -> MAX` and explained `MAX`, while
  the code has said `PEGGED` since 2.4 - and the same README used `PEGGED`
  correctly in two other places.
- **The Fingerprint capture is now chosen by measurement, not by timing.** A
  reader polls in bursts and the trace window is about a second wide, so
  whichever frame the shutter happens to catch decides whether the carrier is
  a rich square wave or a flat line with two blips. `--reader` now scores every
  frame by how many polls it actually caught and keeps the best one. This
  matters beyond the screenshot: the banner reads its entire signature waveform
  back out of that file.

## 3.0.1

Fixes found by looking at the thing on real hardware, which is the only place
some of these show up.

- **Fix: the Site Survey progress bar was touching the row of stats under it.**
  Widening the bar in 3.0 to match the verdict banner put its bottom edge on
  row 27 with the capitals of "FIELD 0%" starting on row 28 - not overlapping,
  so nothing flagged it, but with no white rows between them the text visibly
  fused with the bar. The bar is three rows shorter; its left edge, width and
  top row still line up with the banner, which was the point of the change.
- **The layout checker now looks for clearance, not just overlap.** That bug
  shipped because `tools_check_layout.py` only ever asked whether two things
  ink the same row. It now also asks how much white is between them, using an
  ink model measured off 4x device captures rather than assumed: FontSecondary
  capitals light rows [baseline-7 .. baseline-1] and leave the baseline row
  blank, FontPrimary lights [baseline-8 .. baseline-1]. Zero rows against a bar
  or box edge is a defect; two text rows stacked closer than the app's 10px
  pitch is a defect; one row against the screen border is the house edge and is
  fine. Reintroducing the 3.0 bar raises the count and fails CI.
- **Fix: Fingerprint's two stat rows were on an 8px pitch** where every other
  stacked pair in the app uses 10, leaving a single blank row between PER/BST
  and JIT/UP. The confidence bar sat one row off the CONF readout for the same
  reason. Every vertical gap on that screen is now two rows, with the divider
  and pulse train moved into the two spare rows at the bottom of the screen so
  the trace keeps its full swing.
- **Fix: the "reader locked on" throb ring was drawn through the calibration
  text.** It is a full circle, unlike the rest of the gauge, and its lower arc
  reaches the bottom of the screen; in a normal alarm frame the inverted strip
  is painted over it afterwards, but calibration paints no strip. Calibrating
  while standing in a reader's field drew the arc straight through "HOLD
  STILL". It is now suppressed while calibrating, which also matches the header
  already reading CALIBRATING and the scene already silencing every other
  reader alert for that window.
- **Fix: a one-second Site Survey could report CLEAN.** Letting OK end a run
  early - new in 3.0 - made that reachable, and it duly turned up in a
  screenshot: "SURVEY 1s / CLEAN / No field detected". A run cut shorter than
  10 s now reports **TOO SHORT** instead. The rule is deliberately asymmetric,
  and it is the same one the rest of the app follows: presence is proof,
  absence is not. Finding something in one second is a real finding, so TRACE
  and ACTIVE are never withheld for being quick; finding nothing in one second
  is a claim about a whole room that one second cannot support. The all-clear
  chime is suppressed too, since it would say "nothing here" on a second
  channel.
- **Fix: long logbook entries broke mid-word.** The viewer's TextBox wraps by
  character, so a Watch contact rendered as "...contact 2 at 8s fiel" / "d 17%
  peak 100% m:boost", splitting "field" and losing the indent that marks a line
  as belonging to the timestamp above it. Entries are now wrapped at spaces
  onto indented continuation lines, which is also what keeps them filterable -
  the filter decides what belongs to a finding by indentation. The .csv is
  untouched and stays one flat row per finding. New pure helper with 44 host
  checks, including running a wrapped entry back through the filter.
- The published logbook screenshot is now generated by compiling that helper,
  so it shows what the device actually writes. It previously showed invented
  one-line entries about a third the length of the real ones.

## 3.0

A user-experience release. Nothing here changes what Specter can hear - the
radio code is untouched - but a great deal changes about whether you can tell
what it is telling you. Four screens written at four different times had drifted
into four different dialects, and the single most useful instruction in the
README ("press LEFT and hold still for three seconds") appeared nowhere on the
device.

**One name for one thing.** The strongest reading was `PK` on Sweep, `PEAK` on
Survey and Watch, and `MAX` on the verdict card. The contact count was `C` on
Sweep and `HITS` everywhere else. Carrier-up time was `DUTY`, `SEEN` and
`FIELD %` on three different screens - while `FIELD` *also* meant the live meter
one keypress away. Now: `PEAK` is the strongest reading, `HITS` is the count,
`UP` is carrier-up time, `FIELD` is the live meter and nothing else, and `CONF`
labels the confidence figure that used to be a bare percentage sitting next to
another bare percentage.

- **The keys are on the screen now.** Sweep bound three keys and advertised
  none; until you have found your first reader its bottom strip carries
  `LEFT=cal hold OK=log`, and retires the hint once a contact registers. Watch
  shows `OK=re-arm` whenever there is something to lose - it used to show that
  hint *only* when the count was zero, i.e. only while it was harmless. Site
  Survey's running screen had no hint at all.
- **The menu says what each mode is for.** "Fingerprint" reads as biometrics and
  "Site Survey" reads as a Wi-Fi tool. They are now `Sweep - find it`,
  `Fingerprint - type`, `Site Survey - room`, `Watch Mode - guard`,
  `Logbook - findings`, and - the highest-value word change here - `Help & About`.
- **The dial answers "what counts as a reader?".** The top three ticks used to be
  drawn bolder as a "danger zone", which was decoration pretending to be
  information: presence is decided against the sensitivity threshold, which on
  the default setting sits near 30% of the dial, not 80%. The bold ticks are
  gone and a real mark is drawn at the real threshold, moving when you change
  sensitivity or calibrate.
- **`UP` / `DOWN` change sensitivity mid-hunt.** It is the setting you most need
  to change with the Flipper against a terminal, and reaching it meant about ten
  keypresses and taking the device off the target.
- **Site Survey can be stopped early.** `OK` mid-run used to silently bin the
  whole walk and restart the countdown. It now ends the survey and grades what
  it actually has, and the verdict card prints the duration it was graded over -
  a 30-second `CLEAN` is not the same finding as a two-minute one.
- **Watch Mode's alarm screen was a fifth blank.** Rows 27-39 were empty at the
  exact moment something was happening. They now carry a strength bar, so "is it
  on top of the Flipper or at the edge of range" is readable from across a room.
- **One pulse when the meter pegs.** That is the "you are on it, stop moving"
  moment, and it was announced only on a screen you are usually not looking at -
  your hand under an ATM lip, the Flipper face-down on a pump.
- **Help leads with the keys, and defines the jargon.** `PER`, `BST`, `JIT` and
  `CONF` are the entire payload of the Fingerprint screen and were defined
  nowhere on the device. About now opens with the full key map and a
  READING THE NUMBERS legend, instead of burying both under five paragraphs.
- **Settings say what they change.** `Meter: Boost / Raw` read as a quality
  setting, so flipping it dropped every reading on every screen to about a third
  and the obvious conclusion was that the app was broken. It is now
  `Meter scale: 0-100 / Duty %`. `Logging` is `Save findings`, and the logbook
  size reads `kB` / `B` rather than `k` / `b`.
- **The empty logbook stops guessing.** It used to tell everyone to turn Logging
  on - including the people who already had it on - and three of its lines ran
  past the width of the box and wrapped mid-phrase.

**Bugs fixed**

- **Watch's clock froze at 99:59** while `LAST` kept counting, so a screen left
  running overnight disagreed with itself. Past 99:59 it now rolls to `hh:mm`
  with a marker.
- **Re-entering a mode could flash the previous run's alarm.** No view cleared
  its model on scene entry, so for up to 100 ms - until the first tick - Watch
  could draw a full-screen `ACTIVE READER` for a reader that had long gone.
- **The Sweep readout could run off the screen.** `PK100 C999+` is eleven
  characters starting at x=68 on a 128-pixel screen. It is now `PEAK 100%`.
- **Dead code removed.** `first_ms` was plumbed through three files and drawn by
  nothing; `anim` was incremented on every tick in three views that never read
  it.
- **The published screenshots were wrong in three ways.** The menu image omitted
  Watch Mode entirely, so it showed Logbook in the row where a real Flipper
  shows Watch Mode; the settings image omitted LED, putting Stealth in LED's
  row; and Watch's image drew a pair of "liveness" markers that the app
  deliberately does not draw. The generator also rendered text 20% narrower than
  the device does, which is precisely why the overrunning readout above looked
  fine in every published screenshot. It now draws at the device's own advance.

**Under the hood.** The chrome every measurement screen shares - the header, the
presence dot, the divider, the "another app is using the NFC radio" screen -
lives in one file instead of four copies that had drifted apart, which is what
made the four screens read as one instrument again. 473 host-side checks and the
layout checker both still pass.

## 2.9

A security and correctness audit of the whole codebase. The good news first: a
strict compiler sweep (`-Wshadow -Wcast-align -Wformat=2 -Wnull-dereference
-Wduplicated-cond -Wvla -Wstack-usage=1536` and more) found **nothing** across
all 3,900 lines, there are no unbounded string operations, no division by zero,
no out-of-bounds indexing, every allocation is paired with its free, and the
worker thread uses 224 bytes of its 2 KB stack. What follows is what the
compiler could not see.

- **Fix: `threshold` was the only cross-thread field not marked `volatile`.** It
  is written by the UI thread and read by the sampling worker in its hot loop,
  sitting directly beside `full_scale` which was volatile. A single byte cannot
  tear on this core, but the compiler was entitled to hoist the read out of the
  loop, in which case a sensitivity change would never reach the worker.
- **Fix: booleans loaded from the SD card were not normalised.** `saved_struct`
  validates a magic, a version and a size — it cannot check that the bytes make
  sense. A `_Bool` holding anything other than 0 or 1 is undefined behaviour the
  moment it is read, so a hand-edited or corrupted `specter.conf` could put the
  app somewhere the language has no answer for. Index fields were already
  clamped; the booleans now are too.
- **Fix: the logbook grew without any ceiling.** Watch mode appends every few
  seconds for as long as you leave it standing guard, and nothing ever deleted
  anything — left running it would grow by a few megabytes a day until it filled
  the card, taking every other app's storage with it. Each file is now capped at
  1 MB (roughly twenty thousand findings), and when it is reached the app stops
  writing and says **`LOG FULL`** rather than failing vaguely or quietly eating
  the card. Clear the logbook in Settings to carry on.
- Removed `field_detector_is_running()`: unused, and it reported `true` after the
  NFC radio failed to open, which was simply untrue.
- New **`SECURITY.md`** documenting the threat model — chiefly that Specter never
  transmits, so a hostile reader has no channel to deliver anything over, and the
  realistic untrusted input is the SD card.

## 2.8

- **Fix: the field meter could never reach 100%, even resting on a reader.** The strength smoother was `ema = (ema * 3 + duty) / 4`, and integer division discards the remainder on every update, so the filter cannot converge on its own input — fed a steady 31% it settles at 28 and stays there. That is a permanent ~3-point under-read of raw duty, about ten points of displayed field. Keeping the smoother's state at 1/16 resolution fixes it: on a real terminal the meter now pegs at `100% MAX` as it always should have.
- **Fix: the Watch alarm band still read as flashing.** The strobe was toned down in 2.7 to a pair of markers pulsing at 1 Hz, which was still movement on the one part of the screen you are staring at. Nothing on that band animates now — a solid inverted block is already the loudest thing on a light screen, and liveness is carried by the readouts that genuinely change.
- **New: filter the logbook by type.** Opening the Logbook now asks what you want to see — everything, sweep readings, readers found, site surveys, or watch contacts. Filtering keeps both lines of a matched entry, because a finding without its timestamp is not evidence.
- The demo animation is now generated from the app's own C rather than from numbers chosen by hand. The old one was quietly impossible: it showed an 81% field while still claiming to be `SCANNING` with zero contacts, when anything over the noise floor latches presence and flips to the alarm strip immediately. `tools_gif_data.c` links the real smoother, presence latch, meter scaling, proximity vocabulary, trend rule, classifier and survey verdict, and prints what the device would actually display.
- The proximity vocabulary and the warmer/colder trend moved out of the view into the shared pure layer, so they are host-tested and the demo generator reaches the same words and arrows the device shows.
- Host suite is now **437 checks** across seven pure modules.

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
