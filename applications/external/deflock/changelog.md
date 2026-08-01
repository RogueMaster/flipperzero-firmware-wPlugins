# Changelog

## v0.53
**Two UI requests from
[@h00die](https://github.com/h00die) in
[#5](https://github.com/ReconGrunt/FlipDeFlock/issues/5), both verified on hardware
rather than in CI.**

### Added

- **Remove a detection from the detail screen.** Left opens a confirmation showing
  the MAC; OK deletes. Persistence made a false positive permanent: before
  `hits.csv` you cleared one by backing out and rescanning, and afterwards there
  was no way at all. The delete writes through to the card immediately rather than
  waiting for the scan session to end, so pulling the battery cannot resurrect the
  entry. It removes the RECORD, not the device: anything still in range reappears
  on the next sighting, exactly as a rescan used to behave. This is not a mute list.
- **A Wi-Fi or Bluetooth glyph on every hit**, on the list rows and beside the
  confidence word on the detail screen. Which radio saw a device was previously
  unknowable from the row. "Has an SSID" was not the tell either, since hidden APs
  and probe requests have no name and are still Wi-Fi. Drawn from canvas primitives
  rather than image assets, to stay inside the RAM budget.

### Fixed

- **Deleting the last stored detection now removes `hits.csv`.** It used to leave
  the file untouched when the table was empty, which was harmless while the only
  way to reach zero was a fresh install. With removal available, deleting the last
  entry would have kept the old file and restored everything on next launch.
- **List rows no longer run off the right edge.** A full-length SSID, or a MAC on
  a row that now also carries a glyph, was drawn straight through the signal bars
  and off the screen. Row text is measured and trimmed with a `..` marker, the same
  way the detail screen already did it.

## v0.52
**GPS off the companion board, and two bugs that made features look broken when
they were only unreachable.** Most of this comes from a field report by
[@h00die](https://github.com/h00die) in
[#5](https://github.com/ReconGrunt/FlipDeFlock/issues/5) running an ESP32 card with
GPS wired to the ESP rather than to the Flipper.

### Added

- **GPS can now come from the companion instead of the Flipper.** Settings gains a
  **GPS source** choice (`Flipper` / `Companion`) plus the ESP pin the module's TX
  lands on. On boards that wire GPS to the ESP32 there was previously no way to use
  it at all: the Flipper's own UART pins are not connected to that module, so
  entering *any* pin number there could never work. Needs companion firmware
  v0.52+; the Flipper never transmits to the GPS, it only reads sentences the
  companion forwards.

### Fixed

- **Detection alerts never fired while the Locator screen was open** — the one
  screen you are most likely to be staring at while hunting a camera. Alert
  delivery was driven from a per-scene tick handler, and the Locator installs its
  own, so `Beep+Vibrate` with alert level `Any` produced silence on a `!`-level hit.
  Alerts are now delivered from the app's tick before the scene sees it, so every
  screen behaves the same.
- **A GPS that can never get a fix now says so instead of "searching" forever.**
  If GPS and the companion are pointed at the same UART, or the port is already
  held, the badge reads `GPS!` rather than sitting blank and hopeful. The two
  cannot share one port.
- **The companion no longer drops GPS sentences during its BLE scan.** A BLE scan
  blocked its loop for the whole scan window, so a wardriving pass lost every fix
  that arrived in it. The scan is now run in 1-second slices with the GPS buffer
  drained between them, accumulating results across slices, and sentences are
  coalesced per type so a slow reader still gets the newest position. Verified on
  hardware: a sliced 6-second scan returned 36 devices / 12 trackers.
- **Flashing the companion failed on slower flash chips, before writing a byte.**
  The vendored flasher allowed 10 s/MB to erase; esptool allows 30. A 1.45 MB image
  therefore had 14.8 s, and a chip that erases slower than that timed out at
  `flash_start failed (2)` every single time — for the released companion image too,
  not just development builds. Now matches esptool's budget; this only extends how
  long we wait.
- **A successful flash no longer ends with `Error: COMMAND_FAILED`.** The ESP32 ROM
  answers the final `FLASH_END` with a failure status by design and we already
  ignored it, but the flasher library prints the status itself, so the last line on
  screen contradicted the `Verified OK.` above it.

## v0.51
**A quarter of the app's memory footprint, gone.** Users on heavier firmware were
being refused at launch with *"Not enough RAM to run the app"*
([#5](https://github.com/ReconGrunt/FlipDeFlock/issues/5)) — that is the Flipper's
loader failing to find one contiguous block for the app image, before any of our
own allocation happens. The image is now **86,810 → 65,054 bytes (−25.1%)**, and
the block that has to be found is **67,056 → 50,808 (−24.2%)**.

- **BREAKING: the NFC / RFID Audit and WiFi Audit screens are removed.**
  FlipDeFlock detects surveillance hardware; a card-security grader was never part
  of that, and WiFi Audit was a second product sharing the menu. Together they were
  13.4 KB of an image that was failing to load. **Net Guardian is unaffected** — it
  still runs the Wi-Fi sweep and still flags evil-twin APs, because its fused score
  is only allowed to reach ELEVATED when two independent radios agree. The screens
  went; the scan and the detection stayed.
- **The QR encoder now loads on demand.** Nayuki's generator is ~4.6 KB and is
  reachable from one screen, so it ships as a plugin inside the `.fap` and is mapped
  in only while *Share to DeFlock* is open. **Still a single-file install** — one
  `.fap`, copied to `apps/Tools/`, exactly as before. If the plugin can't be loaded
  the screen degrades to "QR n/a" and still shows the coordinates and OSM tags, so
  you can submit by hand at deflock.org/report.
- **Cheaper maths.** `sinf`/`cosf` dragged in newlib's large-argument range
  reduction (~3.2 KB) for arguments that are only ever a latitude or a compass
  heading, and `powf` was being called with an integer exponent (~1.5 KB). Both
  replaced; the new trig is checked against the host's libm across the real input
  domain, at a tolerance 100× tighter than a float coordinate's own resolution.
- **Smaller detection tables.** `FlockEntry` and `BleDevice` are ordered by field
  width now; the thematic order was leaving 7 and 9 bytes of padding per entry,
  multiplied by 64 and 48 entries.

### Fixed

- **Share to DeFlock never found any marked cameras.** It reported "No marked
  cameras" no matter how many were marked and geotagged, in **v0.48, v0.49 and
  v0.50**. v0.48 moved three scene snapshot arrays off BSS onto the heap and added
  the allocation to two of the three; this screen got the pointer and the `free()`
  but never a `malloc`, and a NULL guard on the collection loop is why it failed
  silently instead of crashing.

## v0.50
Finishes a v0.49 fix that only landed on one of the three scanner screens. **No
detection logic changed.**

- **The WiFi Audit and BLE / Tracker lists still fell back to raw `-70dB` text on
  the selected row.** v0.49 stopped the signal-bar helper forcing black — the
  reason bars vanished on an inverted row — and updated the Flock list to draw
  them on every row. These two screens kept their old "selected? print dB
  instead" branch, above a comment describing the exact behaviour that had just
  been removed. So the two-notations-for-one-column mismatch
  [#5](https://github.com/ReconGrunt/FlipDeFlock/issues/5) reported outlived the
  release meant to fix it, on two screens out of three, and v0.49's own note
  claiming *every* list was corrected was wrong. Both now draw bars on every row,
  selected or not. The exact dBm is unchanged and still on each detail screen.
- README screenshots refreshed for the v0.49 UI.

## v0.49
A UI pass over the Flock/ALPR screens, all of it from a field report by
[@h00die](https://github.com/h00die) running an ESP32-C5 card (issues
[#5](https://github.com/ReconGrunt/FlipDeFlock/issues/5) and
[#6](https://github.com/ReconGrunt/FlipDeFlock/issues/6)). No detection logic
changed — what a hit *is* is identical to v0.48; this is about being able to read
and act on one.

- **"What made it a possible hit?" is now answered on screen.** The detail screen
  gains a `Method:` line — `OUI + beacon`, `SSID + beacon`, `BLE mfg ID`,
  `IE fp + probe req` — so an OUI-prefix lead and an SSID-pattern match are no
  longer both just "Possible". It is re-derived from the stored MAC/SSID/IE-fp
  rather than taken from the companion, so it cannot inherit an over-claim from
  firmware that lags the app. When nothing this side can re-derive matched, it
  says `ESP probe rule` rather than inventing an indicator.
- **The detail screen is a real view instead of a wall of text.** One labelled
  field per line (`MAC:`, `SSID:`, `RSSI:`, `Ch:`, `Seen:`), scrollable, and the
  RSSI row draws the same graphical signal bars the list does. The old layout ran
  RSSI, channel, sighting count and frame type together on one line, which wrapped
  mid-word — the reported `via be`/`acon`.
- **"Lock In".** Right on any detection jumps straight into the Locator's homing
  HUD for *that* device — live RSSI, hot/cold meter, peak hold — instead of making
  you mark it, back out, and pick it from a list. Back returns to the detection;
  back again resumes the general sweep. A Wi-Fi sighting homes on Wi-Fi, a BLE one
  on BLE.
- **Alert level is configurable.** A new Settings item chooses the lowest
  confidence that may buzz: `Any` / `Likely` / `Confirm`. **The default is
  unchanged** (Likely-or-better). `Any` is opt-in and *will* raise false positives
  — it exists because in a thin deployment "Possible" can be all you ever see, and
  an alert you cannot switch on is not an alert.
- **The header no longer prints the same number twice.** Channel and hit count
  lived in both the title bar and the line under it; they now appear once, the
  channel is width-padded so it stops jittering as the sweep hops, and the freed
  width is what stopped the rows overrunning.
- **GPS is a badge, not a code.** A boxed `GPS` — filled with the satellite count
  when locked, hollow while searching — replacing `G:3` / `G:-`.
- **Signal strength is drawn one way, not two.** The bars helper forced black, so
  on the selected (inverted) row it was invisible and every list quietly fell back
  to raw `-82dB` text. One screen, two notations for one field. It now inherits the
  row colour.
- Costs ~1.6 KB of flash and no BSS. The screens above were checked on real
  hardware this time, not just in CI — though with stored hits and a bench build,
  since live detection still needs a companion board.

## v0.48
A false-positive fix on the BLE side, a bug that quietly disabled four features,
and the coverage gaps that let both hide. **If you use the Guardian screens or
BLE detection, upgrade.**

- **BLE devices were announced as CONFIRMED Flock on a shared-vendor OUI.** The
  companion classifies a BLE device as Flock from several signals, and one of them
  is a plain OUI-prefix match on the BLE address. Those prefixes are *shared
  silicon-vendor* ranges — Espressif, Liteon and friends — so any ordinary
  ESP32-based BLE device with a static address was reported as a confirmed
  surveillance camera. **This will reduce what you see flagged**: devices that
  previously showed CONFIRMED now show Possible unless there is a Flock-specific
  tell (the `0x09C8` manufacturer id, the Raven GATT services, or `Penguin-*` /
  `FS Ext *` naming). That is the correct direction — a false positive is worse
  than a missed detection.
- **Same bug class as v0.47, on a path the v0.47 guard never covered.** That fix
  re-derives a claimed CONFIRMED on the Wi-Fi path. Nothing did so for BLE. The new
  rule is a *floor*, not an allow-list: a newer companion may classify on a tell
  this build predates, and the honest answer to "Flock, for a reason I don't
  recognise" is Possible.
- **Freshness stopped updating without a GPS fix.** A BLE device's `last_tick` was
  only refreshed inside the GPS-valid branch, and GPS is off by default, so it froze
  when the device was first seen. Four things silently expired ~90 s into every
  session: the WATCHSCORE "Flipper near" signal (a Flipper sitting next to you
  dropped out of the score while still being seen every scan), the Guardian "Flip N"
  counter, the BLE anomaly window, and the "FOLLOWING you … over *n*s" readout,
  which always displayed `0s`.
- **Companion firmware hardening.** A truncated SSID element was reported as a
  hidden network — the parser zeroed the length but kept the "found it" flag, so the
  all-NUL test passed on zero bytes. A parse miss is not evidence of concealment.
  Also fixed a `snprintf` idiom that could walk off the detection-line buffer if the
  SSID cap were ever raised, and added the missing `volatile`/critical sections
  around state shared between the Wi-Fi task and the main loop (the beacon ring's
  count bounds an array write; the Locator target is a 6-byte MAC that must swap
  atomically). **Reflash the companion to get these** — the app-side fixes work
  without it.
- **A dropped serial byte no longer corrupts a record silently.** Both receive
  interrupts discarded the buffer-full return value, and a byte lost mid-line
  produces a damaged record the parser reads as a valid one. Damaged lines are now
  discarded whole and counted, the same way overlong lines already were.
- **`flock_score()` is gone.** v0.47 found it had no production callers and
  documented that; this removes it. Six checks were pinning a scoring ladder nothing
  on the device ran, while the ladder that *does* run had no such guard — the same
  trap one level up. The assertions moved onto the real wire-protocol boundary.
- **Three modules that had no tests now do**, including the BLE decoder that parses
  raw advertisement bytes, and the entire Marauder backend — which had *zero*
  regression tests despite being the backend that runs on a stock Marauder board.
  Host tests 639 → 763 checks. Every new guard was verified to fail against the bug
  it claims to catch, in a throwaway tree.
- **The OUI tables can no longer drift apart.** They are compiled into both binaries
  from two files with no shared header, and both carried a "keep in step by hand"
  comment with nothing enforcing it. Now a required CI gate.
- **~5.9 KB of RAM reclaimed** — three scenes each held a 64-entry snapshot array in
  memory for the whole run of the app to serve a screen you are usually not on.
  Part of that was spent raising the stack, after measuring one parser frame at
  1344 bytes of the 4 KB budget during startup. Also cut the per-frame OUI scan in
  the companion's Wi-Fi callback by ~8×, and removed two redundant full rescans.

### Companion firmware: builds on current Arduino again, and speaks 5 GHz

- **The companion did not compile at all on Arduino ESP32 core 3.x** — which is
  what a fresh install has been getting for a while. Core 3.x moved the BLE API to
  Arduino `String`, changed `BLEScan::start()` to return a pointer, and reshaped
  `BLEAddress::getNative()`. Anyone following our own README hit a wall of ten
  compile errors. Fixed with 2.x/3.x shims, so **no version pin is needed** and
  both cores work. Reported by [@h00die](https://github.com/h00die) in
  [#4](https://github.com/ReconGrunt/FlipDeFlock/issues/4).
- **Why CI never caught it:** the firmware job pinned core 2.0.17 and only ran on
  release tags. A pin without a matching compat build is a blind spot, not a
  policy. There is now a core-3.x job that builds the classic ESP32, the emitter
  and the C5 **on every push and PR**.
- **ESP32-C5 dual-band (5 GHz) — EXPERIMENTAL.** The C5 is the first Espressif
  part with a 5 GHz radio, and a 2.4-only companion *cannot see* a Flock uplink on
  5 GHz at all. On a C5 the sweep now covers 13 channels on 2.4 GHz plus 28 on
  5 GHz, selectable at runtime with `band 2g|5g|all`. **Nobody on this project
  owns a C5**, so this is compile-verified only — it has never been run on the
  chip, and the release asset is named `..._esp32c5_EXPERIMENTAL.bin` so the
  warning travels with the download. The 5 GHz code is gated on the SoC
  capability, so every other chip compiles exactly as before and pays nothing
  (verified: the channel table is absent from the classic ESP32 binary).
- **The cost of scanning both bands**, stated plainly: 41 channels instead of 13,
  so at the same 300 ms dwell a full sweep takes ~12.3 s instead of ~3.9 s and any
  given camera is revisited a third as often. `band 2g` restores the fast sweep.
- **Companion README corrected.** It told users to download four files that either
  are not release downloads or never existed under that name. One merged image is
  all you need, flashed whole at `0x0`. It also never mentioned FlipDeFlock's own
  built-in flasher, and omitted the required `PartitionScheme=huge_app`. Also
  documents the C5's different bootloader offset (`0x2000`, not `0x1000`) — wrong
  offset means an `invalid header` boot-loop.

**Still not field-validated.** Everything since v0.20 is verified by host tests and
compilers, not against real hardware in the field. Exercising the confidence ladder
over the air needs two ESP32 boards — one to transmit, one to receive — so the bench
emitter remains compile-verified only. The ladder is instead pinned by fixtures at
the real parser boundary. Treat detections as indicators, as always.

## v0.47
A false-positive fix. **If you are on v0.46, upgrade.**
- **Benign networks whose name merely contains `flock-` were shown as CONFIRMED.**
  `Flock-Guest`, `Flock-Safety-Corp`, `Flock-12345`, `MyFlock-Net` — anything with
  that substring that is not `Flock-` followed by exactly 6 hex digits. The real
  provisioning-AP name (`Flock-A1B2C3`) and the `test_flck` dev SSID are unaffected
  and still Confirm; the affected names now score **Likely**, which is what they
  always should have been.
- **Why it happened.** The strict, anchored SSID rule lived in `flock_db.c` and was
  regression-tested there, but nothing on the default (companion) code path ever
  called it — the app took the companion's own looser verdict verbatim and printed
  it. The host tests were green the whole time, because they were testing a function
  that path never reached.
- **Fixed on both sides, deliberately.** The companion's matcher is now anchored the
  same way, *and* the app re-derives any claimed CONFIRMED from the SSID it was sent
  rather than trusting it. The second half is what matters if you don't reflash:
  companion firmware is flashed separately and can lag the app by releases, so an
  already-flashed board gets the correct rung with no reflash. A weaker rung from the
  companion is still trusted — it knows things the app cannot see from one line, like
  probe behaviour and the silent receiver's OUI.
- **Pinned where it actually broke.** New tests drive the real companion wire
  protocol, not the helper in isolation, and were verified to fail without the fix.
  The bench emitter grows an eighth identity so both hidden-SSID encodings
  (zero-length and all-NUL) are exercised — the all-NUL branch had no coverage at
  all. Host tests 613 → 639 checks.

## v0.46
A targeted harvest from [JakeSwiz/WatchFlock](https://github.com/JakeSwiz/WatchFlock),
whose research surfaced that Flock moved their Falcon V2 cameras to hidden SSIDs and
probe requests. No code was taken — the signatures and the finding were.
- **A second device class: SoundThinking (formerly ShotSpotter) acoustic gunshot
  sensors**, via OUI `d4:11:d6`. Deliberately *not* folded into the Flock OUI list.
  A gunshot sensor is not a licence-plate reader, and adding it to that table would
  have the app announce a camera it never saw. It gets its own table, its own `ST`
  tag in the list, and its own line on the detail screen — "detections are
  indicators" applies to *what* a thing is, not just how sure we are.
- **The same ladder and the same caps.** An acoustic OUI scores Possible alone and
  Likely with probe behaviour. There is no known SSID tell for that hardware, so it
  can never reach Confirmed.
- **Hidden-SSID beaconing is now reported.** The companion flags a beacon or
  probe-response whose SSID IE is zero-length or all-NUL; the list shows `[hid]`, and
  the detail screen, the report and `hits.csv` all carry it. The SSID column now
  distinguishes "(SSID withheld)" from "(none seen)".
- **It is reported, not scored — on purpose.** This is WatchFlock's headline finding
  and it would have been easy to promote a hidden Flock-OUI AP to Likely. But hiding
  an SSID is also ordinary consumer-router behaviour, and the OUI tables are shared
  silicon-vendor prefixes, so that rule would flag every hidden ESP32-based AP in
  range. Precision over recall: surface the observation, leave the rung alone, and
  revisit when there is bench or field evidence to justify a change.
- **Six more candidate OUIs**, in `docs/signatures.seed.json` rather than the
  built-ins — WatchFlock's list is flat and states no corroboration, so none of them
  meets the built-in table's bar. `cc:cc:cc` and `f8:a2:d6` were **not** imported
  even though it still lists them: both are our own retractions, and a statusless
  upstream list cannot express a retraction.
- **Channel hop 1-11 → 1-13.** 12 and 13 are unusable for APs in the US so the old
  bound cost nothing there, but they are ordinary channels across most of the world,
  and probe requests are not restricted the same way anywhere.
- **A bench emitter** (`tools/flock_emitter/`) that impersonates every rung of the
  ladder in rotation — including the ones that must *not* fire, like `Flock-Guest`
  staying Likely. Everything since v0.20 has been compile-verified and never put in
  front of a radio; this is the rig that closes that. It transmits, so it lives
  outside the app and is never flashed to a Flipper. The app itself remains passive.
- **`test_flck` is now attributed** to CVE-2025-59409 in the code and the docs. It
  was always matched; nothing said what it was.
- Host tests 458 → 613 checks. `hits.csv` moves to schema v2 (new `class` and
  `hidden` columns); v1 files still load rather than being discarded on upgrade.
- Fixes a latent wire-protocol bug found by the new tests: the detection line's
  field array held 8 slots, so a line carrying two trailing `key=value` fields glued
  the second onto the first and the IE fingerprint silently parsed as 0.

## v0.45
The first release driven by outside field reports. Both features come from
issues filed by [@h00die](https://github.com/h00die) after testing FlipDeFlock
with a Marauder dev board around two known cameras.
- **A detection can now announce itself (issue #1).** Both cameras were
  detected — and neither was noticed until several blocks later, because a hit
  was silent: a new row on a screen you had to be looking at. New **"Alert on
  hit"** setting: OFF / Vibrate / Beep / Beep+Vibe, defaulting to **Vibrate**.
  Haptic-first is deliberate; reading a surveillance-detection screen in public
  is itself a personal-safety exposure, so the discreet option is the default
  rather than the one you have to go find. Every mode also raises the backlight,
  which is what makes a hit noticeable on a device in a pocket or a cupholder.
- **It fires once per camera, not once per frame.** The alert triggers on a
  device's first crossing to **Likely or better** — so a unit first seen as
  "Possible" and later confirmed alerts exactly once, and a camera you are
  parked next to never buzzes again. OUI-only "Possible" leads are deliberately
  excluded: generic vendor prefixes turn up on unrelated hardware, and precision
  over recall applies to the alert exactly as it does to the display. A 3 s
  cross-device cooldown keeps a MAC-randomising unit, or driving into a dense
  deployment, from machine-gunning the vibro motor.
- **`Sound = OFF` still mutes the tone** while the vibro fires, so one global
  mute switch stays honest instead of two settings disagreeing.
- **Hits can survive closing the app (issue #2).** Two detections, back out to
  the Flipper menu, reopen — both gone. New **"Save hits"** setting persists the
  detection table to `apps_data/flipdeflock/hits.csv` and restores it at startup.
  Restored hits reappear in the Flock list *and* on the Flock Map, which is the
  "show someone what the hits look like" case from the report.
- **Off by default, and off means erased.** A hit log is a durable record of
  where you have been — the sort of trail a tool for people evading surveillance
  should not create unless asked. Turning the setting back off **deletes**
  `hits.csv`; a privacy toggle that leaves the old file on the card reads as
  "off" while the record is still sitting there. *Reports → Clear Saved Hits*
  erases it at any time, and drops the restored entries from the screen too.
- **A restored hit never poses as a live one.** Its stored RSSI is from an
  earlier run, so the list shows the **age** of that sighting (`5m`, `3h`, `2d`)
  where the signal bars would be, and the detail screen says `Last RSSI` and adds
  a `Saved: <date time>` line. Detections are indicators, not proof — and a
  stale reading dressed up as a live one is exactly that rule being broken.
- **Two traps found while building it, both fixed before shipping.**
  A restored entry has no meaningful tick timestamp, so WATCHSCORE's freshness
  test `(now - last_tick) > 60 s` would have reduced to `now > 60 s` — **false
  for the first minute after a reboot**, making a hit from days ago read as a
  camera watching you *right now*, in exactly the window a user is most likely
  to open the app. WATCHSCORE now skips archived entries by flag, never by tick
  arithmetic. Separately, the detection table is capped at 64 and used to drop
  new hits once full, so a full `hits.csv` would have blocked every live
  detection for the rest of the session; a new hit now reclaims the least
  valuable *archived* slot (weakest evidence first, oldest breaking a tie) and
  never evicts a live one.
- **Host tests: 291 → 458 checks.** The alert gate and the whole record format
  are pure and covered: round trips through SSIDs containing commas, quotes and
  control characters; "no fix" staying distinct from a real 0,0; and malformed
  lines (wrong column count, bad MAC, unterminated quote, out-of-range enum)
  being rejected outright rather than half-parsed into a plausible-looking wrong
  detection.

## v0.44
A signature-quality release — no new detection capability, one fewer way to be
wrong.
- **Better source for the OUI list.** The built-in Flock OUI table was imported
  from a **flat list with no status column** (`colonelpanichacks/flock-you` →
  `datasets/NitekryDPaul_wifi_ouis.md`). Once a prefix landed in it, nothing
  recorded that it was later doubted. The same researcher now keeps a **curated
  per-prefix table with `Confidence` and `Status` columns**
  (`nitekry/nite-oui-collection` → `groups/flockers/my_tested_flock.md`), and
  that is now the source of truth. Re-checking the shipped list against it
  surfaced a retraction FlipDeFlock had never learned about.
- **Dropped `f8:a2:d6` (32 prefixes → 31).** Upstream marks it *Removed*: "low
  confidence; hit on a Sony Media Player." Removing it can only *lose*
  detections, never gain them — but the loss is bounded (it only ever scored
  "Possible" on its own) and the project's rule is that a false positive is
  worse than a missed detection. The reason is now recorded in the source, so
  re-importing the old flat list can't silently undo it.
- **Two new candidate prefixes — as *user* signatures, not built-ins.**
  `e0:0a:f6` (upstream *Active*, but carrying no confidence note) and
  `14:b5:cd` ("new finding testing") ship in a new
  **`docs/signatures.seed.json`**, giving the SD-card signature file its first
  real content. They are deliberately *not* compiled in: the built-in table is a
  claim of field corroboration, and neither prefix has that yet. Scoring is
  unaffected either way — an OUI hit caps at "Possible" whatever its source.
  `signatures.example.json` stays a pure placeholder template; the unverified
  status the JSON schema can't express is documented in `docs/signatures.md`.
- **Reviewed and rejected: the 1,200-prefix bulk list.** The same upstream repo
  carries a "Nationwide OUIs" dump scraped from wigle.net with no confidence
  column, no dates, and no vetting. It was evaluated and **not** imported — it
  would blow the 64-entry signature cap many times over, and a scrape for
  networks *named* "Flock-something" collects whatever hardware happened to be
  nearby, not Flock hardware.

## v0.43
A precision, correctness & reliability release — a full internal audit turned
into fixes and a real test safety net. No new screens; the app just lies to you
less and can't silently regress.
- **Fewer false "confirmed camera" alarms.**
  - **Strict `Flock-XXXXXX` naming.** Only the real provisioning-AP name
    ("Flock-" + 6 hex) Confirms now; benign names like `Flock-Guest`, Flock
    Freight or the Flock chat app drop to "Likely" instead of a false Confirmed.
  - **OUI + wildcard probe capped at "Likely."** The Flock OUI list includes
    shared silicon-vendor ranges (Espressif, etc.), so any ESP32 gadget spraying
    a broadcast probe no longer reads as a confirmed camera — Confirmed now needs
    an SSID-name or IE-fingerprint corroboration.
- **Correctness fixes across NFC, GPS and reports.**
  - **NFC deep check no longer mis-grades a swapped card** — presenting a second
    card after checking a first previously showed the first card's "cloneable"
    verdict, UID and logged row for it; the verdict is now bound to the card
    actually checked.
  - **GPS: no stale location, verified sentences, sane counts.** A lost fix (RMC
    void / GGA fix-quality 0) stops tagging detections with the last known spot;
    NMEA sentences are checksum-verified before they're trusted; the satellite
    count is clamped; a garbled `0,0` is rejected.
  - **Reports can't be corrupted by a hostile SSID/BLE name** — CSV / JSON / KML /
    Markdown fields are escaped so a comma, quote, `|`, `<` or newline can't break
    a column, close a JSON string or inject a KML element.
  - Plus: WiFi list rows map to the right AP again; newer WiFi security modes no
    longer sort as "worse than WPA2"; map zoom can't wrap to a garbage scale bar;
    the Net Guardian ramps *through* WATCHFUL instead of snapping to ELEVATED; and
    assorted buffer / overflow / out-of-RAM guards.
- **Reliability & diagnostics.** When the ESP UART is busy (e.g. GPS shares the
  port) scenes now say **"UART busy — check port"** instead of a dead
  "connecting…". The companion advertises a **wire-protocol version** (a mismatch
  is flagged, not silently mis-parsed), overlong serial lines are dropped whole
  and **counted** as a health metric, and the ESP32 companion emits each line
  atomically so fast bursts can't interleave and corrupt the feed.
- **Under the hood: a safety net (no behaviour change).** The app now ships a
  **host unit-test suite (291 checks)** — detection/scoring truth tables, the
  anti-stalking "following" gate, WiFi grading, report-escaping vs injection, and
  the ESP + GPS line parsers — wired into **CI** so these precision rules can't
  silently regress. The wire parsers and decision logic were split into small,
  tested pure modules; the scan-scene UART lifecycle was unified (closing a latent
  link-leak on Back); and the first-party tree is now clang-format clean.

## v0.42
- **Catch cameras that randomize their MAC — with field-updatable "class"
  fingerprints.** Flock's fielded cameras have moved to phoning home as ordinary
  Wi-Fi *probe requests* (their old setup Wi-Fi network is usually off now), and
  they rotate their MAC to dodge OUI matching. The defense is the probe's **IE
  fingerprint** — the shape of its 802.11 info-elements, which is MAC-independent.
  The app has computed this on the companion for a while, but the match table shipped
  empty and could only be seeded by rebuilding. Now:
  - **`signatures.json` accepts an `"ie_fps"` list** (8-hex fingerprints), alongside
    the existing `ouis` / `ssid_confirmed` / `ssid_likely`. Same load-only, offline,
    fail-safe rules; capped for RAM (≤32).
  - **The Flock detail screen shows a detection's `IE-fp:`** so you can read it off a
    *confirmed* camera and drop it into the file to catch its MAC-randomized twins.
  - **Precision preserved:** a user fingerprint only ever scores a candidate
    **"Class?"** — never "Confirmed", even with a Flock OUI — because it's unverified
    community data. A missing/garbled file still falls back cleanly to the built-ins.
- **Performance pass (no behavior change).** Under-the-hood only, all build- and
  host-tested: the Flock live-list now snapshots its data and draws *unlocked*
  (instead of holding the scan lock across the whole screen redraw, which stalled the
  ESP worker every frame); the WiFi-audit sort grades each network once instead of
  O(n²) times; the Net Guardian reads its Flock/Atk/Flip counters from one snapshot
  instead of re-locking three times per frame; and ~400 B of dead map-view buffers
  were removed.

## v0.41
- **Locator: hunt a marked device by live signal strength.** A new top-menu
  **Locator** lists every device you've **marked** (the report star/tag on a Flock,
  BLE, or WiFi detection is now also the Locator pool — one mark, both uses) and
  opens a homing HUD: a hot/cold RSSI meter that climbs as you physically get
  closer, the live dBm, **peak-hold**, a **warmer/colder** trend, and an
  "out of range" state when the target goes quiet. Works **without GPS** (a fix
  only adds a "strongest here" note). No compass arrow — direction-finding a
  transmitter needs a directional antenna; you close in by walking.
- **Net Guardian → OK → Suspicious list.** The Guardian now shows *which* devices
  tripped it — BLE **Flippers**, opt-in **anomaly** (unknown) devices, and WiFi
  **rogue / evil-twin** APs — in a list. Selecting one marks it (so it joins the
  report + Locator pool) and jumps straight into the Locator on it. Deauth/attack
  *sources* are omitted: attackers spoof their MAC, so they aren't reliably
  locatable; the list is the markable, locatable subset.
- **Companion firmware:** new `locate <w|b> <mac> [ch]` command streams a
  `LOC,<rssi>` line for the chosen target (Wi-Fi: matched in promiscuous frames,
  channel-locked; BLE: matched in a repeating scan). Additive — older app builds
  ignore `LOC`, older firmware never sends it. **Reflash required** for the homing.
- Refactored the "anomaly" test into a shared `recon_ble_is_anomaly()` so the
  scorer and the Guardian sus-list flag exactly the same devices.

## v0.40
- **New app icon.** Swapped the abstract circle-slash for a **camera lens /
  aperture** — the optic that watches you — so the menu icon actually reflects what
  FlipDeFlock is for. Still a 10×10 1-bit Flipper icon (a literal CCTV camera was
  tried but turns to mush at that size).

## v0.39
- **Net Guardian: Flipper detection, attack-tool signatures, and an opt-in
  anomaly flag.** The watch face now shows three counters — **`Flock`** (cameras),
  **`Atk`** (active attacks), **`Flip`** (Flipper Zeros nearby).
  - **Flippers** are detected app-side (no reflash) by the standard `Flipper …`
    BLE advertised name; they appear in the BLE list as type `Flipper`, count on the
    Guardian, and raise **WATCHFUL** (a recon tool, not proof of an attack, so it
    needs a second independent radio to reach ELEVATED). Excluded from the BLE
    `trk` tracker count (a Flipper isn't a tracker).
  - **Attack-tool signatures** (companion reflash): the companion now emits an
    `ATK,<kind>,<value>` line for a **probe-request flood**, **beacon-spam** (many
    distinct beaconing BSSIDs/s — Marauder/Pineapple), or a **BLE-spam** advert
    flood (Apple/Samsung/Google pairing spam). The app fuses these into the score
    (BLE-spam counts as an independent BLE radio) and names them in the breakdown.
    Conservative, tunable thresholds.
  - **Anomaly flag** (Settings → *Anomaly flag*, default **off**): flags an
    unnamed, unidentified (no mfg id / no recognized service), strong, repeatedly
    -seen BLE device — "something is on you and won't say what it is." Deliberately
    light (below the WATCHFUL floor on its own) to limit false positives.
  - **Honesty note:** a purely *passive* sniffer transmits nothing, so no device
    can detect it. Only active transmitters are detectable; ELEVATED still requires
    two independent radios in agreement.
- Companion line protocol gains the `ATK` line (and the doc now lists the `DA`
  attribution line and the S-line deauth field). All additive — older app builds
  ignore the new line, older firmware never sends it.

## v0.38
- **Net Guardian now shows two plain-English counters: `Flock` and `Attacks`.**
  The watch face's lower-left number is labelled **`Flock`** (Flock/ALPR cameras
  seen this session — it was `hits`), and a new **`Attacks`** counter sits beside
  it: distinct APs caught in a deauth/disassoc **flood** this session. `Attacks`
  reuses the scorer's flood bar (`WATCH_DEAUTH_FLOOD_MIN`), so a lone benign
  disassoc — normal Wi-Fi churn — never counts. The rotating sweep's mode +
  channel moved to the bottom status line, which still flips to the live
  per-signal breakdown on an alert.

## v0.37
- **"frames" instead of "seen", and a per-session reset.** The Flock/ALPR header
  now labels the 802.11 capture count **`frames`** (the standard term; the old `F`
  was dropped earlier to avoid confusion with "Flock", and "seen" was non-standard).
  The companion reports lifetime totals that only zero on an ESP reboot, so the
  count used to climb forever across sessions — **`frames` and `hits` now rebase to
  0 each time you open a scan screen** (Flock/ALPR and Net Guardian), showing the
  current session. An ESP reboot mid-session re-bases cleanly (no underflow).

## v0.36
- **UI overhaul — "Guardian HUD."** A consistent framed/tactical look across every
  scan screen, with the Guardian mascot kept and made the centerpiece:
  - **Net Guardian:** an inverted "NET GUARDIAN" title bar with a live uptime, the
    mascot face front-and-centre with the state word beside it (it goes loud/inverted
    on ELEVATED), a live **THREAT meter** driven by the fused score, and a clean
    `mode · channel · hits` footer over the per-signal breakdown.
  - **Flock/ALPR, BLE, WiFi:** inverted title bars and **signal-strength bars** on
    every row instead of raw `-33dB` text. BLE and WiFi were converted from plain
    menus to custom HUD list views to match (Save/Rescan are now selectable action
    rows at the top of the list); all scan/rescan/save/detail navigation is unchanged.
  - New shared `ui_widgets` (title bar / signal bars / meter) so the screens share
    one visual language.

## v0.35
- **Net Guardian false-positive fix (deauth).** A *single* deauth/disassoc frame —
  normal Wi-Fi churn (client roaming, idle timeout, an AP reboot) — could raise the
  fused score to WATCHFUL, because the companion emits an attribution line for even
  one frame and the scorer didn't require a flood. The score now gates on the
  per-interval deauth **rate ≥5** (the same threshold the live banner uses), so only
  a genuine flood counts. ELEVATED still requires two independent radios.
- **Evil-twin detection now actually works in Net Guardian.** The rogue/evil-twin
  (same SSID, mismatched security) flag was only computed inside the WiFi Audit
  *screen*, so Guardian never saw it. It's now computed at every scan's completion,
  so Guardian's sweep factors it in. (It's capped below the alarm threshold on its
  own, so this adds no false alarms.)
- **On-screen labels reworked for readability.** The cryptic one-letter codes are
  now mini-words that fit the screen: Flock/ALPR `F:339 H:0 C:6` → `ch6  seen 339
  hits 0`; Net Guardian `watch:…`/`F`/`H` → `scan …`/`seen`/`hits`; BLE
  `trk:9 flw:0` → `trk 9  follow 0`; WiFi `2C 1W 3T` → `2crit 1weak 3twin`. Same
  data, just legible. (Screenshots in the README may still show the old shorthand.)

## v0.34
- **New: Net Guardian — a leave-it-on-the-desk "personal net guardian."** A
  dedicated always-on monitor (top-menu "Net Guardian"). Before this, the fused
  "am I being watched?" score (WATCHSCORE) and the ESP radio never ran at the
  same time: the radio only ran inside a scan screen, and the scorer only ticked
  on the idle menu (where the radio is off), so it just decayed off the tail of a
  scan. Net Guardian unifies them:
  - **Rotating sweep keeps every signal live.** It cycles the companion through
    `flockcombo` (dual-band WiFi+BLE Flock + deauth) → `blescan` (BLE trackers) →
    `wifiscan` (evil-twin APs), so all WATCHSCORE inputs stay fresh — and the
    two-independent-radio coincidence gate can actually reach **ELEVATED**.
  - **Live fused scoring + alerting.** The scorer is evaluated every frame; on the
    rising edge into ELEVATED it fires the discreet haptic (and sound, if enabled)
    and wakes the backlight so it's noticeable across a room.
  - **Pwnagotchi-style display.** A calm face that shifts CLEAR `(-_-)` →
    WATCHFUL `(o_o)` → ELEVATED `(>_<)`, with the per-signal breakdown, the live
    sweep mode + frame/hit counters, channel, and an uptime clock.
  - Needs the FlipDeFlock companion FW (it rotates WiFi+BLE); in Marauder mode it
    explains and points you to Flock/ALPR Detect.

## v0.33
- **Hardening pass (multi-agent code audit).** A correctness/robustness sweep over
  the report writers, the NFC deep check, and the companion line parser. No change
  to normal behaviour or detection logic — these close edge cases that could corrupt
  an export or crash on a low heap.
  - **Reports never emit malformed CSV / JSON / XML.** The WiFi-audit CSV could
    split a column when an issue note contained a comma (e.g. WPA1's "deprecated,
    weak"); the GeoJSON and KML could break if a network SSID or a Bluetooth
    tracker's (user-set) name contained a `"`, `\`, or `< & >`. Every device-derived
    field is now escaped per output format, so exports stay valid for downstream
    tools (deflock.org / OSM, geojson.io, QGIS, WiGLE). Normal SSIDs/names are
    unaffected — the output is byte-identical unless a field actually needs escaping.
  - **NFC deep check fails cleanly when memory is tight.** The MIFARE default-key
    audit allocated four scratch tables without checking the result; on a low heap a
    failed allocation could crash the app. It now aborts the check gracefully and
    keeps scanning.
  - **No partial report files left behind on a failed save.** If a report write
    fails part-way (e.g. the SD card fills), the half-written files are now removed
    rather than left as a corrupt export that looks complete — matching the v0.32
    "fail cleanly" behaviour.
  - **Smaller fixes:** the daily NFC-audit CSV header write is verified before a row
    is appended (no headerless file reported as "saved"); the probe IE-fingerprint
    parser no longer scans the SSID field by mistake; and the settings loader has
    extra buffer headroom so adding keys later can't silently truncate the load.

## v0.32
- **Fix out-of-memory crash when saving a report (BLE/WiFi/Flock).** The report
  writers built the *entire* report in RAM first — three growing strings at once
  (CSV + GeoJSON + WiGLE/KML) — which on a large scan used tens of KB of heap on
  top of the FAP's already tight share of the Flipper's ~256 KB, enough to
  exhaust it and crash. Reports are now **streamed a row at a time straight to
  the SD card**, so peak memory is a single ~1 KB line buffer no matter how many
  detections there are. Output files are byte-for-byte identical.
  - Also guards the low-heap case: if memory is too tight to even start, the save
    fails cleanly instead of crashing, and empty report files are no longer left
    behind when there's nothing to save.

## v0.31
- **Fix "VERIFY FAILED (2)" after a flash.** Error 2 is a *timeout*, not a hash
  mismatch (that's error 4) — the data wasn't proven wrong, the ROM just went
  silent on the MD5 query. Two fixes:
  - **Verify before finalize.** The MD5 verify now runs *before* the FLASH_END
    "leave flash mode" command (matching the esp-serial-flasher examples). The
    ESP32 ROM's FLASH_END quirk (COMMAND_FAILED) was desyncing the link and
    making the later MD5 query time out; verifying first avoids that, so a good
    flash now reports a clean **"Verified OK"**.
  - **A verify timeout is no longer a hard failure.** Some ESP32 ROMs don't
    answer the SPI_FLASH_MD5 query over UART at all. Since every data block was
    already written and acked, the app now reports **"Wrote OK; MD5 n/a — reset
    ESP + test it"** instead of "FAILED". Only a genuine MD5 **mismatch** (error
    4) is treated as a bad flash (with a "turn off Fast, reflash" hint).
- FLASH_END's cosmetic ROM status is now fully ignored (it never gated success).

## v0.30
- **Fix "Finalize failed (9)" at the end of a flash.** The write reached 100%
  and every data block was committed to flash, but the final FLASH_END command
  made the ESP32 *ROM* loader answer `COMMAND_FAILED` — a well-known cosmetic
  quirk of the stubless ROM path that does **not** mean the image is bad. The app
  was wrongly treating that as a hard failure and bailing out **before** the MD5
  verify. Now the FLASH_END error is a soft warning, and the **MD5 verify of the
  actual on-chip flash is the real pass/fail gate** — so a good flash reports
  "Verified OK" instead of a scary "Finalize failed".
  - **If you already saw "Finalize failed (9)":** your firmware almost certainly
    wrote fine. Reset the ESP and test it; re-flash with this build to get the
    explicit "Verified OK" confirmation.

## v0.29
- **Fully stubless flasher (fixes "INVALID_COMMAND / software loader is
  resident").** That error appears when the tool tries to upload the flasher
  stub while a stub is already running on the chip (left over from a previous
  attempt). Now **neither** flashing nor backup ever uploads a stub — both go
  through the ESP32 ROM loader, exactly like the 0xchocolate ESP Flasher — so the
  "resident / overlapping address range" error can't happen at all. Backup reads
  via the ROM's 64-byte path (slower, but stub-free and reliable).
  - **If you already hit this:** power-cycle the ESP once (fully remove power) to
    clear the stuck stub, then re-enter bootloader (hold BOOT, tap RESET).

## v0.28
- **Flash over the ROM loader, like the 0xchocolate ESP Flasher (no stub).**
  Flashing a `.bin` now connects straight to the ESP32 ROM bootloader instead of
  uploading the 12.9 KB esp-serial-flasher stub. This matches the widely used,
  proven 0xchocolate flasher, makes the connect lighter/faster, and sidesteps the
  stub's MD5-checked transfer entirely. The write is still verified afterwards.
- **More reliable "Fast" speed.** Lowered the optional fast flash baud from an
  aggressive 921600 to **230400**, which holds up far better over Flipper↔ESP
  wiring (921600 was a likely source of corruption).
- Backup still uses the stub (the ROM can't read flash back) at Safe speed with
  per-chunk retries, and keeps the 5x connect retries from v0.27.

## v0.27
- **Flasher connect + read reliability.** The connection step now retries **5
  times** with a longer per-SYNC timeout and a pause between tries, so a fiddly
  manual bootloader entry (hold BOOT, tap RESET) has many more chances to latch.
- **Backup is now reliable on noisy links.** A "read failed (4)" is an MD5
  mismatch — corrupted bytes in transit, common at the **Fast (921600)** baud.
  Each backup read chunk now **retries** on a transient error, and a backup
  **forces Safe (115200)** speed regardless of the Fast setting, since reads are
  integrity-checked end to end. Flashing (write) keeps the Fast setting — it
  MD5-verifies and retries each block, so a bad fast write is caught and redone.

## v0.26
- **Fix the ESP flasher running the Flipper out of memory.** The in-app flasher
  (Backup / Flash a .bin) could exhaust the Flipper's heap and abort the app —
  a FAP loads entirely into the ~256 KB RAM it shares with the firmware, and the
  flasher's worker (thread stack + UART buffer + the 12.9 KB esp-serial-flasher
  stub) was the tipping point. This was the flasher's first real on-hardware run.
  Fixes: freed runtime heap by right-sizing the scan tables (Flock 96->64,
  BLE 80->48, WiFi 64->48 — still ample for the threat model), halved the
  flasher's transient buffers (4 KB -> 2 KB RX and read-chunk), and added a
  low-RAM pre-flight that shows a clear message instead of crashing if the heap
  is still too tight. No protocol or detection-logic change.

## v0.25
Roadmap sprint, two "Next" items:
- **Raven vs Falcon split.** A Flock unit is now positively identified as a
  **Raven (acoustic/gunshot sensor)** when the companion firmware sees its
  Raven-specific GATT services (`0x3100`–`0x3500`) — shown as
  **"Flock Raven (audio)"** on the BLE detail screen and in reports. The
  external-battery serial is shared across Falcon (ALPR) and Raven, so a Raven
  is asserted **only** on that GATT match; the absence of it is never treated as
  proof of Falcon (a wrong "audio surveillance" label is worse than a generic
  one). New backward-compatible `rv=1` companion line-protocol flag — reflash
  the companion firmware to emit it; older firmware just reports "Flock device".
- **Updatable signature database.** Load extra Flock OUI prefixes and SSID
  patterns from `apps_data/flipdeflock/signatures.json` on the SD card, **merged
  over** the built-ins, so new signatures don't need a rebuild. **Load-only** (no
  writes, no network) and **fail-safe**: a missing or malformed file leaves the
  built-ins fully intact. User signatures are unverified, so an OUI-only hit
  still scores only "possible" — they can add detections, never over-claim.
  Capped for RAM (≤64 OUIs, ≤32 patterns/list). JSON via vendored jsmn (MIT);
  see [docs/signatures.example.json](docs/signatures.example.json).

## v0.24
- DeFlock moved from deflock.me to deflock.org (the old domain redirects).
  Updated all links, the in-app About text, and the Share-to-DeFlock QR handoff
  URL to the canonical domain.
- Added a README "Support" section pointing at the repo Sponsor button.

## v0.23
- **WATCHSCORE coverage honesty.** In Marauder mode (no companion firmware) the
  "am I being watched?" indicator can only see the WiFi/Flock side; the BLE
  tracker, deauth, and evil-twin signals need the companion. It now shows
  **"watch: WiFi only"** and never lets a CLEAR imply Bluetooth/deauth are clear
  too, so a non-flashing user isn't falsely reassured. Flash the companion for
  full coverage.
- Added a "What's new" section to the README.

## v0.22
Roadmap sprint, three new features:
- **Flock BLE serial decode.** Parses the `0x09C8` external-battery advertisement
  to extract a Flock unit's device serial from its always-on battery telemetry
  (no probe/association needed) and shows it on the BLE detail screen. Serial
  logging to reports is **off by default** (Settings → privacy). Note: this
  advert's serial is shared across Falcon (ALPR) and Raven (audio) units, so
  Raven-vs-Falcon isn't split yet. A validated follow-up via the Raven GATT
  service UUIDs is the path.
- **WATCHSCORE.** A single decaying "am I being watched right now?" indicator on
  the start screen that fuses the existing validated signals (confirmed Flock,
  BLE follower, deauth flood, evil-twin AP). **ELEVATED requires ≥2 independent
  radios coinciding**, with hysteresis, dwell, and a per-signal breakdown: one
  state, not an alert flood.
- **Probe IE-fingerprint pipeline (inert until seeded).** The companion firmware
  now hashes each probe-request's Information-Element skeleton and coalesces
  MAC-cycling bursts by 802.11 sequence number, so Flock detection can survive MAC
  randomization. Ships with an empty fingerprint table (no behaviour change, no
  false positives) until seeded from confirmed-unit captures; reports a device
  *class* match, never a unique device.

## v0.21
Roadmap sprint:
- **NFC default-key audit:** on a MIFARE Classic, a new "Deep" check captures the
  UID and tries the Flipper's on-SD key dictionary against every sector, then
  reports how many open with **factory/default keys** (trivially cloneable). This
  answers the core access-control question, "is this badge using default keys?"
  Reads the stock dictionary (no bundled keys); UID and default-keyed count go
  into the report. (mfkey32 deferred: it requires active card emulation.)
- **On-device Flock map:** a live map that plots detected ALPR cameras by
  bearing/distance around your GPS position, with auto-fit zoom, heading tick,
  confidence-by-dot-size, and a scale bar. Visualization only, no new radio
  activity.
- **Share to DeFlock (phone handoff):** renders a QR per marked camera that opens
  DeFlock on your phone at that location, so you contribute through the official
  app's review flow. The Flipper/ESP never touch a network, so passive-only stays
  literally true. (Direct OSM submission deferred: it needs OAuth2/TLS on the ESP,
  which would break the no-network promise.) QR via vendored Nayuki qrcodegen
  (MIT).

## v0.20
- **Anti-stalking precision model** (Tier-2): a BLE tracker is flagged
  "following" only when seen >=4 times over a >=90 s window at >=3 distinct
  observer waypoints spanning >=150 m. This kills urban false positives (a
  stationary shop tag, a single drive-by) while a real follower still clears it
  easily. Thresholds are tunable `#define`s; the detail view shows the track.
- **CI:** non-failing API-87.1 drift warning on every build; a `release.yml`
  attaches the `.fap` to `v*` tag releases automatically.
- **Docs:** refreshed the GitHub description/topics and README roadmap.

## v0.19
- BLE WiGLE CSV export (Type=BLE): the BLE/Tracker scan now also writes a
  `ble_*.wigle.csv` (geotagged devices only) next to the WiFi one, sharing one
  WigleWifi-1.4 header. (Tier-2 "safe" item from the audit sprint.)

## v0.18
Audit sprint:
- **Flasher:** fast-baud (921600) now works. It was calling the non-stub rate API
  after loading the stub, which always failed.
- **Reports:** GeoJSON now uses OSM/DeFlock tags (`man_made=surveillance`,
  `surveillance:type=ALPR`) so points import; CSV/WiGLE fields are RFC-4180
  escaped (a comma/quote in an SSID no longer corrupts rows); WiGLE omits
  no-GPS-fix rows (no "Null Island"); partial report-write failures now surface.
- **Detection:** deauth alert needs a real flood (>=5/interval), not a single
  frame; added Flock's own registered OUI `B4:1E:52`; ISO15693 graded WEAK
  (UID-only/cloneable).
- **BLE:** detect Google Find My Device trackers (`0xFEAA`:
  Pebblebee/Chipolo/Motorola/Eufy). Geotag hysteresis removes tag jitter.
- **Robustness:** settings baud clamped to valid values; Marauder scraper has a
  bigger line buffer + bounded SSID extraction.

## v0.17
- Clearer support for not flashing (keeping Marauder). Renamed the setting to
  "Board Mode" (Marauder / Companion). In Marauder mode the companion-only
  screens (WiFi Audit, BLE/Tracker Scan) now explain they need the companion
  firmware instead of showing a dead screen, and About shows the active mode and
  what each one does.

## v0.16
- Fix: the ESP board kept scanning after you exited the app. The stop command
  was being cut off because the UART was torn down before it finished
  transmitting; it's now drained first. Works on Marauder and the companion (ships
  in the .fap, no re-flash). The companion firmware also fully idles on stop
  (leaves dual-band mode, parks channel hopping/status).

## v0.15
- Flasher correctness pass (code audit). Backup now reads the final flash chunk
  (off-by-one in the library's read/verify bounds); flashing pads images to
  4-byte alignment so arbitrary .bin sizes work; the write is MD5-**verified**
  and failures are reported (no more "done" on a bad flash); UART is drained
  before the fast-baud switch (prevents desync); robust partial-read handling;
  a partial backup is deleted on abort. Plus minor UI/throughput tweaks.

## v0.14
- Fix "not enough RAM to run app": the flasher bundled flash stubs for ~10 ESP
  chips (a FAP loads fully into RAM). Trimmed to ESP32 only, cutting the .fap
  from ~182 KB back to ~100 KB. (Flasher now supports classic ESP32 boards;
  other chips can be re-added if RAM allows.)

## v0.13
- Flasher "Flash Speed" setting: Safe (115200) or Fast (921600). Fast raises the
  link after connect for much quicker backup/flash; falls back to Safe on failure.

## v0.12
- In-app ESP32 flasher: **back up** the board's current firmware to SD and
  **flash a .bin** (companion / Marauder / a backup) straight from the Flipper -
  switching between Marauder and the FlipDeFlock companion, no computer. Built on
  Espressif's esp-serial-flasher. Manual bootloader entry (hold BOOT, tap RESET).

## v0.11
- Device tagging: mark/untag any WiFi AP or BLE device (Tag button in detail);
  tagged items show `*` in the list and are flagged in the saved reports.

## v0.10
- Dual-band cadence tuned WiFi-biased (9 s WiFi / 3 s BLE).
- GPS is now OFF by default (existing installs keep their saved choice).
- Settings persist across reboots (saved on every change).

## v0.9
- Dual-band Flock detection: the companion firmware now interleaves WiFi (2.4GHz
  promiscuous) with periodic BLE scans, so Flock/Raven is detected over both
  radios; BLE-Flock hits merge into the Flock list and reports.
- Broader BLE Flock signatures (Penguin/FS Ext Battery names, Raven service
  UUIDs, Flock OUIs) beyond the mfg-id check. BLE kept resident (no heap churn).

## v0.8
- Stronger rogue/evil-twin heuristic: same SSID on multiple BSSIDs with
  mismatched security is flagged as a likely evil twin.
- Catalog-readiness: changelog, funding info, submission docs.

## v0.7
- BLE / Tracker Scan: detect Flock/Raven BLE beacons and AirTag/Tile/SmartTag
  trackers; flag a tracker that follows you across GPS waypoints (anti-stalking).

## v0.6
- Extra Flock heuristics (wildcard probe + addr1 silent receiver).
- Capture observer heading (GPS course) in the GeoJSON.
- OUI vendor lookup in the WiFi audit; KML export for Flock reports.

## v0.5
- Deauth attribution (names the attacked BSSID + channel).
- WiGLE CSV export. CI-built ESP32 firmware .bin for the Flipper ESP Flasher app.

## v0.4
- Deauth/disassoc flood detection with a live alert; evil-twin (duplicate SSID).

## v0.3
- WiFi security audit (auth/cipher/WPS grading) via the companion firmware.

## v0.2
- App icon; real ESP32 Marauder backend; RX heartbeat.

## v0.1
- Initial release: Flock/ALPR detection, NFC/RFID audit, GPS geotagging,
  Markdown + DeFlock GeoJSON reports, universal ESP32 companion firmware.
