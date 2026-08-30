# Changelog

## v0.77

**Net Guardian can now tell you what an attack IS, not just that it happened.**
The HUD only ever showed a count ("Atk 1"), which answers the wrong question --
the one you actually have is "is someone attacking my network right now, or did
the router just reboot?" A count cannot tell those apart. Everything here is
passive: it reads only what the scan already saw. No transmit.

### Added

- **The app now says what just beeped.** An alert told you something was found
  and could not tell you what, and new rows land at the bottom of the list, so
  the answer cost a scroll every time. A card now rises on exactly the same
  event as the beep, carrying the rung, the device and its name, and clears
  itself after three seconds. **Pressing OK jumps straight to that device and
  opens it**, so the row you would have hunted for is simply arrived at.
  Requested by [@h00die](https://github.com/h00die) in discussion #7.

  Sorting newest-first was the other option and was deliberately not taken: the
  selection is a position in the list and Left is Delete, so a row arriving
  while you reach for one would quietly retarget the delete at a different
  camera.

- **A vendor field, so the app stops calling everything a Flock camera.** Every
  ALPR-class detection used to render as *"Flock / ALPR camera"* — including
  competitor hardware, and including MACs that matched no table at all and were
  scored purely on probe behaviour. The detection screen and both report exports
  now carry a **vendor** alongside the class, re-derived on the Flipper from the
  MAC and SSID rather than asserted by the companion. An unattributed detection
  reads *"ALPR (unattributed)"*; only real Flock evidence prints the word Flock.

- **Five competitor vendors detected: Ubicquia, Motorola Solutions, Verkada,
  Genetec and Avigilon.** Cities dropping Flock are switching vendors, not
  dropping ALPR — Axon's replacement products are the clearest case, and **Axon
  Lightpost is Ubicquia hardware underneath**, a streetlight node that is a
  tri-band Wi-Fi 6 access point and so should beacon continuously. Every prefix
  was read out of the IEEE registry one at a time and the organisation string is
  quoted in `flock_db.c`.

  These score **Possible on any frame type, including a bare beacon** — unlike
  the Flock table, where bare-OUI scoring was removed for reporting a T-Mobile
  gateway. The reasoning does not transfer: `flock_ouis[]` is mostly Liteon and
  Espressif, while `94:7b:be` is Ubicquia's own and Ubicquia makes nothing else.
  They are never promoted above Possible — registry-verified is not
  field-observed, and none of this hardware has been captured on the air.

- **A `Gear` device class: vendor known, kind not.** One Motorola Solutions OUI
  covers plate readers, body cameras *and* APX/MOTOTRBO hand-held radios; a
  Ubicquia AP6 has no camera while an AP/AI carries the plate reader. Nothing on
  the air separates them, so the app names the vendor and declines to name the
  product.

- **A misattribution denylist in the CI parity gate.** Grep the IEEE registry for
  "motorola" and Motorola *Mobility* (Lenovo — consumer phones) comes back beside
  Motorola *Solutions*; grep "genetec" and an unrelated Japanese company comes
  back beside Genetec Inc. Nine such look-alike prefixes are now blocked by
  `tools/check_oui_parity.py` and asserted absent by `test_flock_db.c`. All four
  gate checks were verified against cases they must **block**, in a throwaway
  tree — including a drift applied identically to both files, the shape that hid
  `f8:a2:d6` for five releases.

### Changed

- **The Axon label no longer promises the device moves.** It read *"Axon
  body/in-car kit"*, chosen so it could not be mistaken for a camera on a pole —
  correct while Axon made only body-worn and in-car equipment. In 2026 Axon
  launched **Outpost** and **Lightpost**, both fixed ALPR, on the *same single
  OUI*. The label is now **"Axon: body or fixed"**, because a MAC cannot tell you
  which. `docs/signatures.md` and `README.md` carried the old claim too and were
  corrected.

- **Reports gained a `Vendor` column** in both the sightings and evidence tables.

### Fixed

- **The bench emitter was broadcasting its own SSID under a spoofed MAC.**
  `tools/flock_emitter` runs in APSTA mode, and beacon identities also pointed
  the ESP32's own SoftAP at the address being impersonated, so the board emitted
  a second, unintended beacon carrying its default `ESP_xxxxxx` name from the
  spoofed MAC. The app keeps the first SSID it sees for a MAC, so an identity
  could be recorded under the board's name instead of its own. The rung was
  never wrong (the OUI is what fires it), but the rig misreported the exact
  field an SSID identity exists to test. Setting that MAC was never needed:
  an injected beacon carries its source in the frame we build by hand.

- **Nightly builds now include the companion firmware.** They were `.fap` only,
  on the reasoning that the protocol handshake would catch a mismatch. That does
  not cover a detection-table change like this one: the protocol is unchanged,
  so the handshake stays silent while an old companion simply never reports the
  new vendors. A nightly would have shipped an app whose headline feature was
  inert with nothing telling you.

### Notes

- **New Zealand coverage is deliberately absent, not missing.** Police reach there
  runs through **Auror** and **SaferCities vGRID/VIBE**, which are *software
  platforms that ingest existing retail and council CCTV* — there is no Auror
  device to detect. Covering them would mean shipping Hikvision, Dahua, Uniview
  and Axis OUIs, which would flag every doorbell and shop camera in the country.
  Reasoning written down in `docs/signatures.md` so it is not relitigated.

- **Motorola Solutions' Bluetooth SIG company id `0x04EC`** is recorded and
  verified in `flock_ble.h` but **not yet consumed** — wiring it up needs a new
  BLE category in the companion sketch. It is *not* `0x0008`, which is the legacy
  consumer-handset registration.

- **This half needs a companion reflash to take effect.** The new tables score on
  the ESP; an un-reflashed companion never reports these vendors at all.

- **Attack detail screen** -- press **Left** on Net Guardian. For each live
  attack it shows what it is (deauth flood / beacon-probe-BLE spam / evil twin),
  the target BSSID and channel, how many frames over how long, a verdict
  (**brief** vs **ACTIVE**), and what to actually do about it.

  The verdict is the point. A router reboot or one congested moment is a *blip*:
  a short burst that stops. A real attack *persists* -- frames keep arriving
  across many status intervals, so the span grows while it stays fresh. That
  timing is what separates them, and it is host-tested in `test_attack_triage.c`.

- **Actionable advice, per attack kind.** Deauth is the one with a real fix, and
  it gets the direct instruction: enable **WPA3 / PMF (802.11w)** on your router,
  after which a spoofed deauth is simply ignored. The flood/spam kinds are
  nuisances a passive tool cannot stop and does not pretend to -- the advice says
  so and where they are coming from.

- **Evidence log.** Confirmed (sustained, not blip) attacks are appended to
  `apps_data/flipdeflock/attacks.csv` with a timestamp, kind, target, frame count
  and duration -- a record you can keep or hand to someone. Unlike `hits.csv`
  this defaults **ON**: it is a log of attacks *against* you, not a movement
  trail. Toggle **Settings -> Attack log**.

- **Escalated alerting.** **Settings -> Attack alert**: *Off*, *Once* (one buzz on
  a new active attack, the default), or *Repeat* (re-buzz every 20 s while it
  stays active). The HUD also shows `<atk!N` whenever a triaged attack is live, so
  the Left drill-down is discoverable.

**Passive by design, on purpose.** This does not transmit, jam, or spin up a
decoy -- the Flock/ALPR passive pledge is unaffected. A decoy SSID would also not
work against a deauth flood (the attacker spoofs your real BSSID; a same-named AP
does not intercept that), so the honest defence is detection, evidence, and
pointing you at PMF, which actually stops it.

**Hardware verification.** The vendor work, the detection card and the row and
Help changes were all checked on a real device against `tools/flock_emitter`,
including the before/after that proves the new vendor rung only fires with the
v0.77 companion firmware. Net Guardian's HUD and the attack screen's "no active
attacks" state were checked too. The one thing still host-tested only is the
attack screen with a LIVE attack on it: that needs a real deauth flood or advert
spam to render, and none was generated. `test_attack_triage.c` covers the
triage logic behind it.

## v0.76

**The ESP32 flasher would not open on RogueMaster** ([#23](https://github.com/ReconGrunt/FlipDeFlock/issues/23),
reported by [@h00die](https://github.com/h00die)). It failed with *"Flasher
unavailable"* the moment the screen was entered.

### Fixed

- **Plugins are found under any of the filenames this project ships as.** The
  flasher and the QR encoder ship as `.fal` files embedded in the `.fap` and are
  mapped in only while their screen is open. The loader looked in exactly one
  place: `/assets/plugins/`.

  `/assets` is a **virtual** path. The firmware rewrites it to
  `/ext/apps_assets/<appid>` — and for an external `.fap` that appid is **not**
  the manifest appid, it is the *filename*:

  ```c
  path_extract_filename_no_ext(path, app_name);
  furi_thread_set_appid(loader->app.thread, ...);
  ```

  Asset extraction derives its directory the same way, so on stock firmware the
  two agree and one path was enough. That assumption broke the moment the same
  app began shipping under a second filename — `deflock.fap` for RogueMaster,
  added in [#21](https://github.com/ReconGrunt/FlipDeFlock/pull/21). Anything
  that makes the running name and the extracted name disagree leaves a perfectly
  good `.fal` sitting on the card that the app cannot see.

  The loader now tries the virtual path first, then the concrete directory for
  every artifact name the project releases. The QR handoff used the same loader
  and had the identical latent bug; it is fixed by the same change.

- **The failure now says what it checked, instead of guessing.** The old message
  was *"Reinstall the .fap so its assets are extracted"* — one possible cause,
  stated as if it were the only one, and simply wrong for #23: those assets
  **were** extracted, just under a different directory name. Being told to
  reinstall something already installed correctly costs the reporter time and
  tells the maintainer nothing.

  Every attempted path is now logged individually, and the on-screen text points
  at that log, because a photo of this screen is what actually arrives on an
  issue.

**Honest status:** this is a fix for a real defect found by reading the firmware's
own path resolution, and it covers the case #23 describes. It has **not** been
confirmed against a RogueMaster device — none was available — so if the flasher
still fails there, the CLI log will now name every path tried, which is the thing
that was missing the first time.

## v0.75

**The probe-rate gate is removed.** It shipped in v0.74 and should not have.

v0.74 required 3 probe requests from one transmitter inside 2000 ms before a
Flock OUI plus probe behaviour could score *Likely*. That threshold was not
strict, it was **impossible**. The companion watches any single channel for
300 ms out of every 3900 ms, so a camera probing every ~125 ms can put at most 2
probes into one window, and consecutive windows are 3900 ms apart. The condition
could never be met by anything, including a real camera, and it sat on the rung
upstream says finds most fielded cameras.

Widening the window to span sweeps was tried next. On the bench that still
produced **zero** probe-sourced detections, while beacons from the same board
detected normally -- so the receive path was fine and the probe rung was not.
Whether the remaining cause was the gate or the test rig's own MAC spoofing was
never established, and a filter on the primary detection path that cannot be
shown to pass a true positive is worse than the false positive it prevents.

So the gate is gone and the behaviour returns to what upstream runs and field
tested at 11 of 12 cameras with 2 false positives. **The reported T-Mobile false
positive stays fixed**, because its actual cause was never the cadence: it was
`48:27:ea` (Samsung Electronics) and `a4:cf:12` (Espressif) sitting in the
built-in OUI table. Those remain demoted to the seed file.

What survives is the **measurement**. The per-transmitter probe counter is still
there and still reports `pr=<n>` on the wire -- as an observation, never a
confidence input. A future threshold should come from that data rather than from
reasoning about cadence, which is now 0 for 2.

**Requires a companion reflash**, like the change it reverses.

### Fixed

- **The bench emitter never transmitted Wi-Fi.** `tools/flock_emitter` passed
  `en_sys_seq = false` to `esp_wifi_80211_tx()`, while its own comment 200 lines
  above states the IDF refuses foreign-MAC injection unless that flag is true. The
  driver logged the objection for every beacon and nothing reached the air. On
  hardware the Flipper saw 0 Wi-Fi detections across minutes at 36 frames/s while
  reporting BLE identities from the same board.

  That is why "the emitter has never been run" mattered: it compiles either way,
  and the only symptom is a detector that stays empty while the serial log
  narrates identities it never sent. Fixed, and the rig immediately earned its
  keep -- it is what caught the impossible gate above.

- **Probe identities in the emitter now model a camera.** They held for 3 seconds
  and fired a single scan, so they were usually off the air when the detector was
  listening. A fielded camera probes continuously in station mode; the identity
  now holds across several detector sweeps and re-arms its scan throughout,
  pinned to the bench channel.

- **The emitter never spoofed a probe MAC.** `esp_wifi_set_mac()` was called on a
  started interface and with the same address already assigned to the AP, which
  the ESP32 rejects as `ESP_ERR_WIFI_MAC` (0x3009). The return code was never
  read, so every probe identity went out from the factory Espressif MAC while the
  serial log announced a Flock OUI. The detector was right to ignore them: they
  were not Flock frames.

  The symptom is worth recognising -- **beacons detect, probes never do**. Beacon
  source addresses are a field in a hand-built frame, so they were always correct;
  only the interface-level spoof was broken. The rig now stops the interface to
  set the MAC, leaves the AP address alone for probe identities, and **reads the
  return code**, warning loudly if it ever fails again.

### Verified on hardware

Run against the emitter on a second ESP32:

| Identity | Expected | Observed |
|---|---|---|
| `Flock-A1B2C3` | CONFIRMED | **CONFIRMED** |
| `test_flck` | CONFIRMED | **CONFIRMED** |
| `Flock-Guest` | Likely, never CONFIRMED | **Likely** |
| Flock OUI + wildcard probe | Likely | **Likely**, `Method: OUI + probe req` |

The last row is the one this release is about. It is the rung upstream says finds
most fielded cameras, it was dead under v0.74's gate, and it had never once been
checked against a radio. `Flock-Guest` is the v0.46 over-claim, also confirmed on
the air rather than argued from a unit test.

## v0.74

**A false positive users actually hit, and a Net Guardian you can point at one
network.**

**Partly hardware-verified.** Run on a Flipper Zero (Momentum `mntm-dev`, API
87.1) with the ESP32 companion attached and reflashed to this build. Net
Guardian's network targeting was exercised end to end, the Axon device class was
confirmed rendering on a real screen, and the v0.70 Detail-round-trip fix was
verified on hardware for the first time (a tagged tracker survived Back, and the
device table grew 25 -> 28 rather than resetting). **The probe-rate gate's
thresholds are still unmeasured** -- no Flock hardware was present to exercise a
true positive, so the gate is known to stop the reported false positive and is
NOT known to pass a real camera. See the note under that entry.

### Added

- **Net Guardian can guard ONE network instead of everything in range.** Press
  **Right** on the Guardian screen to pick an access point; the bottom line then
  names it (`> MyNetwork`) instead of showing the `OK=sus` hint. The choice
  persists, so a Flipper left next to a router comes back guarding the same
  network.

  Untargeted, the Guardian answers *"is anything around me under attack?"* — and
  in a flat, an office or a hotel that is mostly somebody else's traffic. A
  Guardian that lights up for the neighbours is one you learn to ignore, which is
  precisely the alert fatigue the fused score was built to remove.

  **Only the network-shaped inputs are filtered:** a deauth flood must be aimed at
  the guarded BSSID, and an evil twin must clone the guarded SSID. Flock
  detections, BLE trackers, a Flipper nearby and attack-tool signatures are about
  the **operator**, not the network, so they keep contributing whatever is
  targeted — filtering those on a BSSID would be meaningless.

  The BSSID and the SSID are both stored because they answer different questions:
  a deauth is attributed by address, while an evil twin *by definition* announces
  the same name from a **different** address. Matching a twin on BSSID could never
  fire. A target with no name (a hidden AP) simply never contributes the
  evil-twin signal; deauth attribution still works for it. Changing the target
  resets the score rather than carrying one earned against a different question.

  **You can run the scan from inside the picker.** If no networks are listed yet,
  a **Scan for networks** row runs one on the link the Guardian already holds and
  the list fills in place — no leaving the screen and coming back. The row reads
  `Scanning...` while the sweep is out and `Scan again` afterwards, and it says
  `No ESP32 - check wiring` when there is no companion to ask.

  The first cut showed `(no APs seen yet - run a scan)` instead, which was wrong
  twice over: the submenu clipped it to `(no APs seen yet - run a s...` so the
  instruction was cut off mid-word, and selecting the row did nothing anyway. Every
  label on this screen is now short enough to render whole — a half-read
  instruction is worse than none.

  **The row under the cursor is the row you get.** Rebuilding the list to show new
  scan results reset the submenu selection to the top, so while a scan was
  delivering, the list shifted between reading a row and pressing OK — and a
  different network got guarded than the one that was highlighted. Caught on
  hardware: `Kestral` was chosen and the neighbouring `WiFi` ended up targeted,
  with nothing on screen to say so. The cursor is now carried across the rebuild.
  Silently guarding the wrong network is worse than not offering the feature.

  **Verified on hardware**, not just in CI: the picker opens on Right, an empty AP
  list renders safely, the in-place scan was run and repopulated the list live, a
  real AP was selected, the HUD showed `>Kestral` in place of the `OK=sus` hint,
  the active target is marked with `*` on re-entry, hidden APs list as `(hidden)`,
  and the choice round-tripped through `settings.txt` as `guard_bssid` +
  `guard_ssid` and reloaded on restart.

### Changed

- **RogueMaster gets its own release artifact, `deflock.fap`.** RogueMaster names
  the app that way, so anyone installing there had been renaming the Unleashed
  file by hand. It now builds as a fourth CI target and ships under the name that
  firmware expects. Contributed by [@h00die](https://github.com/h00die) in
  [#21](https://github.com/ReconGrunt/FlipDeFlock/pull/21); it uses the Unleashed
  SDK, since RogueMaster tracks it and reports the same API.

### Fixed

- **A T-Mobile hotspot was being reported as a likely ALPR camera.** Reported from
  the field, and the cause is two-part.

  First, the built-in OUI table is mostly **chip vendors, not Flock**. Checked
  against the IEEE registry, 21 of its entries are registered to **Liteon**, and
  only `b4:1e:52` belongs to Flock Safety itself. Two of them were worse: `48:27:ea`
  is **Samsung Electronics** and `a4:cf:12` is **Espressif**, and upstream rates
  both *"low confidence, WiGLE crowdsource"* — its weakest tier. Both are now
  **demoted to `docs/signatures.seed.json`**, where you can opt back in. They are
  not retracted; nothing says they are wrong, only that nobody corroborated them.

  Second, and the general fix: the companion scored *Flock OUI + wildcard probe
  request* as **Likely**, and a wildcard probe is the single most ordinary frame a
  Wi-Fi client emits — it is what scanning for a network looks like. So any device
  on shared silicon scored Likely for doing nothing at all. The companion now
  requires a **sustained probe rate** before that rung: a fielded Flock camera runs
  in station mode and probes roughly every **125 ms**, while a phone or hotspot
  emits a short burst and then goes quiet for tens of seconds. Same frame, very
  different cadence — the rate is what separates them.

  The counter is keyed on the transmitter and updated *before* the sequence-run
  coalescer, which deliberately suppresses repeats; counting after it would always
  see one probe and the gate would reject real cameras too. The **silent-receiver**
  path is deliberately left ungated — there the frame was sent *to* the Flock-OUI
  device by someone else, so the cadence is the sender's and says nothing about the
  receiver, and that path is upstream's key technique for catching a dormant camera.

  **The thresholds are not field-tuned.** 125 ms is upstream's figure; the
  client-side distribution has never been measured here, so they are set loosely to
  clear the reported false positive without risking a real camera. The observed
  count now rides the wire as `pr=<n>` and reaches the app as an observation —
  never a confidence input — precisely so it can be tuned from real captures
  instead of guessed at twice.

  **BOTH HALVES NEED A COMPANION REFLASH**, and the demotion is the less obvious
  one. The OUI table is compiled into the companion as well, so a board running
  older firmware still matches `48:27:ea` and still reports it -- and the app
  trusts the companion for every rung below Confirmed, because those depend on
  probe behaviour it cannot re-derive. Updating the `.fap` alone will not clear
  this false positive. Flash `flipdeflock_companion_esp32wroom.bin` from the same
  release.

  **Not verified on hardware.** The gate is companion-side and the board attached
  during testing was on older firmware, so nothing exercised it. The thresholds
  remain unmeasured guesses until someone runs a camera and a phone past it.

## v0.73

The v0.72 RogueMaster load failure is fixed. **Not run against a radio.**

### Added

- **Axon Enterprise detection, as its own device class.** Axon makes body-worn and
  in-car police equipment — Axon Body, Axon Fleet — and rows for it carry an `AX`
  tag next to the confidence character, alongside the existing `ST` for
  SoundThinking. Two identifiers, both taken from the issuing registry rather than
  from anyone's list: the IEEE OUI **`00:25:df`** (*Axon Enterprise, Inc.*, their
  only registration) and the Bluetooth SIG company id **`0x034D`**, filed under
  their former name TASER International — the same pattern as Flock's `0x09C8`
  sitting under the battery vendor XUNTONG.

  **It is deliberately not folded into the ALPR class.** An Axon unit is not fixed
  infrastructure: it moves with a person or a vehicle. The detail screen says "Axon
  body/in-car kit" and the label is asserted by test to contain no word *camera*,
  because the one thing this must never do is read like a camera on a pole.

  **Registry-verified, never field-observed.** Nobody has captured an Axon device
  using these on the air, and embedded products routinely expose the Wi-Fi *module*
  vendor's OUI instead of the brand owner's — which is why most Flock hardware
  appears as Liteon or Espressif rather than `b4:1e:52`. This may match every Axon
  radio or none of them. An OUI-only hit caps at *Possible* exactly like every
  other OUI; the BLE company id reaches *Confirmed*, as `0x09C8` does. There is
  also a real duty-cycle limit: Axon's own docs say a body camera checks for a
  known network every **15 minutes**, versus roughly every 125 ms for a Flock
  camera, so you have to be listening in the right window.

  Two traps are recorded in `docs/signatures.md` and pinned by tests. Searching a
  vendor database for "axon" also returns *Axon Networks Inc* — an unrelated
  networking company — plus Axona, Axonne, Interaxon, Maxon, Praxon, Paxonet and
  Yaxon. And a curated "law enforcement OUI" list in circulation turned out to have
  **11 of 15 prefixes wrong** when checked against IEEE: Apple filed as Digital
  Ally, Nintendo as WatchGuard, General Motors and Samsung as Panasonic i-PRO,
  Xiaomi and Dell as Getac, and Axis Communications as Flock Safety. Importing it
  would have reported phones, consoles, cars and laptops as police equipment. Every
  one of those prefixes is now asserted absent, so re-importing that list breaks
  the build.

### Fixed

- **RogueMaster no longer fails at launch with `Missing Imports`.** The optional
  phone-GPS calls are resolved only when the Phone source starts, so firmware
  variants that share an API version without exporting that optional service can
  load the app and report Phone GPS as unsupported instead.

- **A stored Axon detection would not have survived a restart.** The `hits.csv`
  parser bounded the device-class column at the old maximum, so a line carrying the
  new class was rejected outright — the sighting would not have come back
  mislabelled, it would not have come back at all. The bound is now a named
  constant beside the field it guards, and every value the enum can hold is
  round-tripped by test. Verified against the failing case: restoring the old bound
  turns 16 checks red.

- **API drift is now checked on the nightly, not only on push.** Unleashed shipped
  `unlshd-091` (API 88.2 → 88.3) and nothing reported it, because the check lived
  in the push-triggered workflow while the nightly — the job that actually rebuilds
  against each firmware's live release channel — had no such step. Both now share
  one composite action rather than two copies of the same 50 lines of shell.

  **Nothing was broken by that bump.** The loader compares only the *major* API
  version; the minor comparison is commented out in the firmware source, identically
  on official firmware and Unleashed. README claimed the firmware refuses a `.fap`
  "built against a different one", which overstated it and could have sent someone
  hunting a problem that did not exist.

### Changed

- **Raven firmware 1.1.7 cannot be positively identified, and the README now says
  so.** The Raven-specific Bluetooth services (`0x3100`–`0x3500`) that back the
  "Flock Raven (audio)" label arrived in Raven firmware **1.2.0**. A unit still on
  1.1.7 publishes only generic services — device information, health thermometer,
  location and navigation — which millions of ordinary devices also publish.
  Matching those would flag half the consumer electronics in range, so an older
  Raven simply does not get the label.

- **Troubleshooting explains why an OUI emulator shows two "missing" detections.**
  Bench tools that replay Flock prefixes include `cc:cc:cc` and `f8:a2:d6`, both
  retracted upstream and both deliberately unmatched here, so those two identities
  producing nothing is the correct result rather than a miss.

## v0.72
**A retracted false-positive OUI came back and shipped in five releases. It is out
again, and there is now a guard that would have caught it.**

If you are running **v0.67 through v0.71**, that build scores an unrelated consumer
device as a *Likely* ALPR camera on one prefix. Updating is worth it for that alone.

**Not run on hardware.** As with everything since v0.20, this is verified by host
tests, both compilers and the CI gates — not against a radio.

### Added

- **Phone GPS, on Unleashed firmware.** **Settings → GPS From → `Phone`** takes the
  position from a paired phone over the firmware's RPC location service, for people
  who have no GPS module. Needs Unleashed and the qUnleashed companion app paired
  over BLE or USB. Requested in discussion #20.

  **A phone is not the recommended source and is not the default.** A GPS module
  only receives; a phone is a second radio-connected device tied to a subscriber
  account and logged by the network, which is a poor trade for anyone using this
  tool to avoid being tracked. README states the tradeoff plainly.

  Fixes coarser than **100 m are rejected**, which has no equivalent on the NMEA
  paths: a receiver without a lock reports no fix, while a phone answers with a
  cell-tower estimate that looks identical to a real one. Pinning a camera two
  kilometres out is worse than leaving it unpinned — precision over recall, applied
  to position. A heading is kept only when the fix passed and the phone was actually
  moving, since a stationary phone reports no bearing and 0 is indistinguishable
  from due north.

  Six new badge states, because the phone source has six ways to fail and one
  hollow "searching" badge standing in for all of them is what made issue #5 take
  four rounds: `!FW` no location service on this firmware · `!APP` nothing paired ·
  `!PERM` permission denied · `!LOC` phone location off, or no receiver on the
  paired device · `!ACC` answering but too coarse · `!ERR` companion fault.

  Gated at compile time on `__has_include(<gps/gps.h>)`, so the official-firmware
  and Momentum builds compile it to a no-op and keep linking — CI builds all three
  from one tree and the service exists only in Unleashed's API 88.2. Verified by
  building against all three SDKs and confirming the `gps_*` API imports are present
  in the Unleashed image and absent from the other two.

- **A redacted false-positive export.** **Reports → False Positive Report** writes
  a Markdown file you can attach to an issue without publishing where you were.
  Reporting a wrong detection previously meant handing over the detection log,
  and that log is a record of your movements and the networks around you.

  Kept: the confidence rung, the indicator that actually fired, the OUI, frame
  type, channel, RSSI, sighting count and IE fingerprint. Removed: GPS
  coordinates and heading, the low three octets of every MAC, the sighting
  timestamp, and any SSID that did not itself match a Flock naming rule.

  An SSID that *did* match is kept as-is, because that is a camera's own name.
  Anything else becomes a shape (`A`=upper `a`=lower `d`=digit), since an SSID is
  routinely a surname or a street address and is independently geolocatable
  through public wardriving databases. The shape still answers the question that
  matters — was this a MAC-shaped name, or a person's network?

- **Issue templates** for false positives, bugs, and detection signatures. The
  signature form states plainly that field data needs no DCO sign-off.

- **A size report in CI**, against the ~256 KB budget. It measures the **loaded
  image** (`.text+.rodata+.data+.bss`), currently **65,444 bytes, 24%** — not the
  `.fap` on disk, which includes embedded plugins that are extracted to the SD
  card rather than loaded. Measuring the file would have called the change below
  a regression.

### Changed

- **The ESP32 flasher loads on demand instead of shipping inside the app.** It is
  reached from one screen, so it now ships as an embedded `.fal` mapped in only
  while the Firmware screen is open. Measured on the linked ELF, this took
  **10,230 bytes (13.5%)** out of the contiguous allocation the loader has to find
  at launch — the allocation that fails with *"Not enough RAM to run the app"*.

  Install is unchanged: still one file, with the plugin embedded inside it.

  This replaces the idea of splitting Net Guardian and the Wi-Fi audit into
  separate apps. Measured, those two are ~3.8 KB and ~2.5 KB, so that split would
  have cost a lot of restructuring to recover under a quarter of what this did.
  Both stay in the app.

### Fixed

- **The retracted OUI `f8:a2:d6` is out of the detection tables again.** Upstream
  withdrew it — *"low confidence; hit on a Sony Media Player"* — and it was dropped
  for v0.44. It came back silently in `93beede` (2026-08-05), a commit about
  *tightening* precision, which reflowed both OUI tables and reinstated the prefix in
  the process. It shipped in **v0.67, v0.68, v0.69, v0.70 and v0.71**.

  It was not harmless while it was there. A camera is scored *Likely* on a Flock OUI
  plus a wildcard probe request — and every ordinary Wi-Fi client sends wildcard
  probes while looking for networks. So a device on this prefix was reported as a
  **Likely Flock/ALPR camera** for simply scanning for Wi-Fi. That is the same class
  of false positive as the T-Mobile gateway that `93beede` was written to eliminate.

  The tables now hold **31** prefixes, which is what both files' comments, the v0.44
  changelog entry and `docs/signatures.md` had been claiming throughout.

- **The OUI parity gate now checks more than parity, because parity was not enough.**
  `tools/check_oui_parity.py` compared the app's table against the companion's and
  nothing else. `93beede` changed **both files identically**, so the gate stayed green
  at 32-vs-32 for five releases while both count comments still said 31 — the script
  printed the contradicting number and had no rule to act on it. It now also fails on:

  - a **count comment that disagrees with its own array**, and
  - any **upstream-retracted prefix** reappearing in a built-in table — `f8:a2:d6`,
    `6c:cd:d6`, `94:2a:6f`, `f4:e2:c6`, `cc:cc:cc`, `00:0c:e7`, each carrying its
    retraction reason so it cannot be "rediscovered" from the older flat OUI list.

  `test/test_flock_db.c` asserts the same denylist from the matcher side and pins the
  table size. Both guards were verified against the failing case — the regression was
  re-seeded in a throwaway copy outside the repo, and each guard was confirmed to fail
  on its own, including with the count comments "corrected" to hide the first one.

- **`signatures.example.json` no longer teaches users to reintroduce the
  `Flock-Guest` bug.** The template shipped `"ssid_confirmed": ["flock-"]`, and
  `docs/signatures.md` tells you to copy the file and edit it. User SSID needles are
  matched as **unanchored substrings**, so that value marks `Flock-Guest`,
  `Flock-Freight-WiFi` and similar benign networks **CONFIRMED** — precisely the
  over-claim that the built-in `Flock-` rule was anchored to prevent in v0.47, reached
  through a supported path instead. The app's re-derivation guard cannot catch it,
  because it re-checks against the same matcher that is consulting your needle.

  Both example values are now inert placeholders, matching how `aa:bb:cc` and
  `deadbeef` already behaved, and `docs/signatures.md` carries an explicit warning
  that `ssid_confirmed` is the one key where a user file can manufacture a false
  CONFIRMED. Anything doubtful belongs in `ssid_likely`.

- **Superseded CI runs are cancelled rather than queued.**

### Changed

- **The built-in OUI table now records what each prefix is actually resting on.** It
  describes itself as prefixes *observed in fielded Flock Safety deployments*, which
  is true of most but not all of it. `docs/signatures.md` gains a provenance table
  separating Flock's own registered OUI, field-corroborated prefixes, four
  contract-manufacturer prefixes that WatchFlock files separately as false-positive
  prone (Liteon/USI), four inherited from the superseded flat list with no status in
  the curated source at all, and three the curated source rates as weak. **No scoring
  changes** — an OUI-only match still caps at *Possible* regardless of grade — this
  only stops the table's own claim reading as stronger than the evidence.

- **Signature sources re-checked against upstream.** No new Flock or SoundThinking
  prefixes exist that this app lacks; coverage of published research is current. BLE
  tells are unchanged and confirmed: XUNTONG company id `0x09C8`, the Raven GATT range
  `0x3100`–`0x3500`, and the `Penguin-` / `FS Ext` naming. `e0:0a:f6` stays in
  `docs/signatures.seed.json` rather than being promoted — its second source rates it
  *lower*, not higher.

## v0.71
**Nightly builds, so a fix can be tested before it is frozen into a tag.**
No app behaviour changes in this release; it is release plumbing and docs.

### Added

- **A nightly channel.** `main` is now built and published daily to a rolling
  `nightly` prerelease whenever something has changed, carrying the same three
  `.fap` files under the same names as a tagged build. This exists because v0.69
  shipped a fix that did not hold: between it and the correction in v0.70 the only
  published build was the broken one, and "wait for the next tag" was the only
  advice available. It also runs the other way, letting whoever reported a bug
  confirm the fix before it is tagged.

  Nightlies are marked as prereleases, so `/releases/latest` still resolves to the
  newest tag and the README install link is unaffected. The companion firmware is
  not rebuilt for a nightly; pair it with the `.bin` from the newest tag. Skipped
  entirely on a day when `main` has not moved, so the publish date always means
  something changed.

### Fixed

- **Superseded CI runs are cancelled instead of queued.** A second push to a
  branch left the first ESP32 run sitting in the queue behind its own replacement
  — one on PR #18 sat there for 25 hours, on a result nobody was waiting for.
  Tag builds are exempt, because a cancelled tag run is a release missing its
  companion firmware with nothing red to show for it.

### Changed

- **`CONTRIBUTING.md` and `SECURITY.md` moved into `.github/`**, where GitHub
  surfaces them in the PR and advisory flows. Every link that pointed at them was
  updated; the licensing grant's original publication date still governs, since a
  relocation is not a republication.
- **The asset pack download link and filename** were corrected.
- **[@nickk02](https://github.com/nickk02) is credited in `CONTRIBUTORS.md`** for
  the v0.70 release-engineering work.

## v0.70
**Opening a detail screen stopped the scan and cleared the table behind it.**
Every scan screen, not just BLE. v0.69 aimed at this and missed; the entry below
is corrected.

### Fixed

- **Looking at a detection no longer destroys it.** `scene_manager_next_scene()`
  calls the outgoing screen's `on_exit` before it opens the child, so pressing OK
  on any device tore the ESP link down on the way IN. Wi-Fi scanning, BLE
  scanning, deauth detection and the GPS relay were all offline app-wide for as
  long as a detail screen was open, and the Back that followed came back to no
  link at all. Every "is this a fresh scan?" test read that as a new session and
  cleared the table. Measured on hardware: BLE/Tracker lost 36 devices and the
  tag just set on one of them, and Net Guardian lost its Wi-Fi count and reset
  its threat score, every single time. The board answers the next request from
  its own cache within a fraction of a second, so the counts looked healthy again
  moments later, which is how this survived two releases.

  The link is now owned by the app rather than by a screen. It is released when
  you genuinely return to the Main Menu, or when the app exits, and it stays up
  across a detail round-trip. Net Guardian's baseline reset was gated on nothing
  at all and is now gated too. Flock Map was never affected: it opens no child
  screen.
- **Returning from the Locator no longer leaves the board idle.** The Locator
  tells the companion to stop, and the scan screens used to be restarted by the
  link being rebuilt underneath them. With the link surviving, each screen
  re-sends its own start command on entry instead.
- **A scan you never started can no longer overwrite saved hits** with an empty
  table.
- **The Flock list keeps your place.** It reset to the top of the list every time
  you looked at a detection and came back.

### Added

- **A `.fap` for each firmware, not just official.** A `.fap` records the API it
  was built against and the loader refuses one that does not match, so the single
  file shipped until now only ever worked on API 87.1. Unleashed and RogueMaster
  are on 88.2 and were being told the app was old. It was not; the API was.
  Releases now carry `flipdeflock.fap` (official), `flipdeflock-unleashed.fap`
  (Unleashed and RogueMaster) and `flipdeflock-momentum.fap`. README and
  [Troubleshooting](docs/TROUBLESHOOTING.md) have a table telling you which one
  is yours. Thanks to [@nickk02](https://github.com/nickk02).

### Changed

- **Label formatting is explicitly bounded**, so the app builds on SDKs that
  enable `-Wformat-truncation`. The evil-twin row now keeps the `[OPEN] AABBCC`
  tail that names which access point is the open one, instead of a long network
  name eating it. Thanks to [@nickk02](https://github.com/nickk02).
- **The release checksum job waits for every companion image** before hashing,
  rather than only the WROOM one. Two images are attached by two independent
  jobs, so a `SHA256SUMS.txt` that looked complete could have omitted the C5
  build. **The API drift check reads the right column** of the SDK's
  `api_symbols.csv`; it had been comparing a status field to a version number and
  so could never have reported real drift. Thanks to
  [@nickk02](https://github.com/nickk02).

## v0.69
> **Correction (v0.70):** the BLE fix below did not hold. It gated the table
> clear on a "fresh session" signal that was always true, because the screen's
> own `on_exit` tore the link down on the way into the detail view. Tagging a
> device and pressing Back still cleared the table. Fixed properly in v0.70.

**Marking a device sent it to the Locator, and the Locator had nothing to home
on.** The tag was being thrown away one keypress after it was made.

### Fixed

- **A tagged device survives a Back press.** *(This did not work; see the
  correction above.)* The BLE screen cleared its whole table in `on_enter`, and
  scene re-entry re-runs `on_enter` -- which is exactly what a plain Back from the
  detail screen does. So tagging a device and stepping back reset the table before
  the mark was visible to anything else. The Locator reads that table, so it never
  saw a tag, because making one always costs a Back press. It now clears only on a
  genuinely fresh scan session, the same signal the Flock, Flock Map and Guardian
  screens already gated on. Same root cause as the Net Guardian data loss in
  issue #5.

### Added

- **Ping and Ring for a validated tracker.** On a tracker you have already
  selected, **Ping** does a one-shot reachability check and **Ring** asks an
  Apple/Find My tag in separated state to make a sound. These are the only
  actions in the app that transmit, they are explicit, and nothing goes out
  unless you press them. Ring is deliberately limited to the Apple/Find My
  non-owner path rather than guessed at for Tile or SmartTag.
- **`SHA256SUMS.txt` on every release**, covering all published assets. Flock
  detection is a trust decision, so being able to prove the file you hold is the
  one this repo built matters. See [TRADEMARK.md](TRADEMARK.md).
- **[TRADEMARK.md](TRADEMARK.md) and [LICENSING.md](LICENSING.md).** The first
  draws the line between a legitimate fork (welcome) and a repackage passing
  itself off as official (not). The second states the dual offer plainly:
  GPL-3.0-or-later free for everyone, plus a commercial licence for anyone
  shipping FlipDeFlock inside a closed product. **Nothing is gated, and nothing
  changes for existing users** -- see [SUPPORTERS.md](SUPPORTERS.md).

### Changed

- **The README's "it never transmits" line was inaccurate** once Ping and Ring
  existed, and now says what is actually true: detection is listen-only, those
  two explicit actions are the exception.

## v0.68
**The Locator gave no usable sense of closer or farther.** The cause was
arithmetic, not a broken feature.

### Fixed

- **The meter shows the smoothed level, not the last raw sample.** A single
  frame's RSSI swings hard with multipath and body position, so the big number
  jumped around with no relation to whether you were walking closer. The smoothed
  value already existed and was only being used for the warmer/colder word.
- **Smoothing retuned 70/30 to 50/50.** Readings on the primary target arrive
  roughly once every 1.6 s, so at the old weight a new sample took five readings
  (~8 s) to move the needle. When samples are scarce each one has to count more.
- **Companion LOC window 120 ms to 400 ms.** A Flock camera sweeps the channels
  with probe requests while the companion listens on one, so it is heard about
  once every 1.6 s. A 120 ms window that hard-resets its peak spent most of its
  life expiring empty and threw away the readings it *had* caught. This invents no
  data; it stops discarding it.
- **Freshness 2500 ms to 4000 ms**, which sat barely above the gap between
  readings and flipped the display to "quiet" on ordinary jitter.

Worth stating plainly: about one reading every 1.6 s is a physical limit for
Wi-Fi homing on a target that hops 13 channels while you sit on one. This makes
the feedback usable and truthful, not fast. BLE targets update far more often.

## v0.67
**A bare OUI match is no longer a detection.**

### Fixed

- **OUI-only sightings are dropped.** A T-Mobile gateway broadcasting
  `tmobile-5416` was reported as a possible ALPR camera. The OUI table is mostly
  shared silicon-vendor ranges -- Espressif, Liteon and friends -- so "beacons,
  and has one of these prefixes" describes an enormous number of ordinary
  consumer devices.
  It is also no longer evidence of anything: Flock's management AP was
  deactivated around **December 2025** and the cameras moved to station mode,
  emitting wildcard **probe requests** roughly every 125 ms rather than
  beaconing. So an OUI hit on a beacon is, by construction, not a camera. The
  upstream research this OUI list comes from reached the same conclusion and
  disabled every detection path except probe-request + OUI + IE fingerprint.
  Nothing catchable is lost: a real camera still reaches Likely via the
  probe-request branches and Confirmed via an SSID name or IE fingerprint.
- **The Marauder backend got the same treatment.** It has no frame type to work
  with, so it now requires a Flock-shaped SSID on the same line to corroborate an
  OUI.

### Added

- **OUI `f8:a2:d6`**, which was missing from our copy of the upstream list. Both
  the app and the companion carry it, enforced by the existing CI parity gate.

## v0.66
**The evil-twin rule was too loose, and its alert was unverifiable.**

### Fixed

- **"Evil twin" now requires a security DOWNGRADE**, not merely a difference. It
  flagged any two APs sharing an SSID whose auth modes differed at all -- which
  fires on **WPA2/WPA3 transition mode**, where one radio or mesh node advertises
  `WPA2_PSK` and another `WPA2_WPA3_PSK`. That is an ordinary modern network, and
  the app was telling its owner they were under attack. It now requires one side
  to be open or WEP while the other is secured: a clone you would join without a
  password, standing in for a network you trust. A clone using the *same*
  security was never detectable this way regardless, so nothing catchable is
  lost. "A false positive is worse than a missed detection" applies hardest to
  the scariest thing this app can say.
- **The Suspicious entry shows the evidence.** It read `Rogue AP <ssid>` -- an
  accusation with no way to check it, since you could not see the two BSSIDs or
  which one was open. It now reads `Twin <ssid> [OPEN] AABBCC`, naming the
  security and the BSSID, so the dangerous half is identified rather than implied.

## v0.65
### Added

- **The asset pack ships with every release**, as `flipdeflock_asset_pack.zip`.
  It was previously repo-only, which meant the animations existed but nobody
  could install them without cloning the source. Unzip into `asset_packs/` on the
  SD card and pick it in the firmware's Desktop settings.
- **`tools/pack_assets.py`** turns the committed `.png` frames into the `.bm`
  files the firmware reads. The repo keeps PNGs because those are what a human
  can open and diff; the device wants `.bm`, and this is the step between.
  Validated by re-packing the existing frames and diffing against what the
  firmware SDK's own packer produces: 16 of 16 byte-identical.

## v0.64
### Added

- **Asset pack: a hooded figure kicking a camera pole down**, 24 frames at
  128x64, alongside the existing scanner animation. A tribute to
  [@h00die](https://github.com/h00die), who has field-tested this project harder
  than anyone and found most of the bugs worth finding.

### Fixed

- **The on-screen version can no longer disagree with the build.** It was a
  hand-maintained `#define` kept in step with `fap_version` by hand: two edits in
  two files with nothing checking they matched. It is derived from a single
  `FAP_VERSION` in `application.fam` now and reaches the C code as a build
  define. The old arrangement survived seven bumps in a day, and its failure mode
  was silent -- showing a confidently wrong version to precisely the person who
  asked for it so he could report bugs against a known build.

## v0.63
### Fixed

- **Help & Warnings was unreadable on the device.** It shipped as flowing prose
  hand-broken at a column limit, which put line breaks mid-sentence ("GPS and the
  ESP are / on the SAME UART. They"), plus doubled section gaps. Rewritten so
  every line is a short complete phrase that stands on its own -- the same shape
  the About page uses, which is why that one always read cleanly. It now opens
  with a summary table of all five GPS badge states, visible without scrolling,
  before the per-fault detail.

## v0.62
**A warning that cannot be looked up is not a warning.**

### Added

- **Help & Warnings**, a new main-menu page explaining every mark this app can put
  on a screen: the GPS badge states and what to do about each, the scan header
  fields, the row confidence letters and tags, and a common-fixes section. A user
  hit `!PORT` on his own device and said "I don't know what it means and have no
  way of finding out" -- naming a fault precisely is only half the job.
- **A fault explains itself where you meet it.** When GPS cannot work, the scan
  screen shows what is wrong, what to change, and where -- e.g. `!PORT UART clash
  / GPS and ESP share a port. / Settings > GPS Port`. OK dismisses it, and it only
  re-arms on a fresh scan session, so it never becomes something to swat away.

## v0.61
### Fixed

- **A GPS fault badge no longer starts with the word "GPS".** `GPS!` and `GPS?`
  were both read as the GPS being on and working -- the reporter of the original
  GPS bug looked at a filled `GPS!` and described his board as "showing a gps
  lock", which is the exact opposite of what it meant. That is not a misreading:
  a filled badge is how this header says "locked, n satellites", so a filled badge
  whose first three characters are G-P-S reads as a lock at a glance, and the
  punctuation was carrying the entire meaning alone. Each fault now names the
  thing to fix: **`!PORT`** (GPS and the ESP are on the same UART), **`!PIN`** (the
  companion refused that ESP GPS Pin), **`!FW`** (the companion never answered --
  reflash it).
- **The Wi-Fi glyph's arcs sit adjacent.** With a blank row between both arcs and
  the dot it read as three loose fragments rather than one mark. Only the dot
  keeps its gap now -- it is the device, the arcs are the signal leaving it.

## v0.60
### Changed

- **The Flock header uses the Wi-Fi and Bluetooth glyphs instead of the letters
  `rx` and `b`**, suggested by [@h00die](https://github.com/h00die) on
  [#5](https://github.com/ReconGrunt/FlipDeFlock/issues/5). The same two marks
  already label every row below, so the header stops needing a vocabulary of its
  own. Reads `ESP [wifi]55/s [bt]490 a2`.
- **The sub-line is measured against the GPS badge rather than guessed.** It is
  drawn as placed segments now, each positioned from the measured width of the
  one before, and nothing is drawn past the badge's left edge -- so no counter can
  grow into it however large it gets. Previously the only check on that was a
  user counting the remaining gap by hand off a photograph.

## v0.59
### Added

- **Settings gains `Test alert`.** Press Left/Right and the app fires the real
  detection alert with your real settings, immediately.
  "No beep or vibrate" has been reported three times on
  [#5](https://github.com/ReconGrunt/FlipDeFlock/issues/5) and every structural
  part of the path audits clean, because the app firing and the Flipper's own
  notification settings swallowing it are indistinguishable from outside. One
  press separates them. Silent means the fault is the Flipper's **Notifications**
  (volume / vibro) or **Alert on hit** being OFF, and no detection tuning will
  ever produce a sound. A buzz means the notification path works and the question
  moves to whether a detection actually qualified -- which the new `a<n>` counter
  on the Flock header then answers.
  It calls the same `recon_alert_fire()` the detection path calls, deliberately:
  a test that exercises different code than the thing it tests is worth nothing.

## v0.58
**Version on screen, and two invisible failures made visible.** All from
[@h00die](https://github.com/h00die) on
[#5](https://github.com/ReconGrunt/FlipDeFlock/issues/5).

### Added

- **The version is on the main menu** (`FlipDeFlock v0.58`) and on About, so a bug
  report can name the build without digging.
- **`a<n>`: alerts actually DELIVERED this session.** "No beep or vibrate" has been
  reported three times, and there was no way to tell the app not firing from the
  Flipper's own notification settings swallowing it. Those are different faults
  with different fixes. If `a` climbs and nothing buzzes, the app did its part and
  the fault is the Flipper's Notification settings (volume / vibro) or the alert
  level being set above the rung being detected.
- **`b-` versus `b0`.** A BLE count of 0 could not distinguish "BLE ran and heard
  nothing" from "BLE never ran at all" -- and a user seeing `b0` beside a healthy
  Wi-Fi rate had no way to know which. `b-` now means no BLE scan phase has
  completed yet; `b0` means one has, and found nothing.
- Spaces dropped after each header tag, freeing width before the GPS badge.
- **About credits @h00die**, who has found more real bugs in this project than
  anyone else using it.

## v0.57
**Screen icons in the title bars, and a small memory reclaim.**

### Added

- **Title bars carry a screen glyph**: a camera on Flock, a shield on Net
  Guardian, a crosshair on the Locator. `FLOCK/ALPR` also shortens to `FDF`,
  which hands roughly 45 px back to a header that has to fit channel, hits, the
  frame rate, the BLE count and the GPS badge.
  Hand-placed pixels and every form is axis-aligned. Deriving them from the
  project logo was tried and abandoned: its camera sits at 45 degrees and was
  already an unreadable blob at 16 px, because a rotated edge is entirely
  staircase and leaves nothing for the shape. All three were checked on a real
  panel, inverted, before shipping.

### Fixed

- **156 bytes reclaimed from the Settings GPS pin picker.** It stored a
  formatted string for every selectable pin inside the single contiguous
  allocation the app loader has to place, and only one item is ever dynamic.

## v0.56
**The Flock header now shows live activity instead of a number that only grows.**
From a suggestion by [@h00die](https://github.com/h00die) on
[#5](https://github.com/ReconGrunt/FlipDeFlock/issues/5).

### Added

- **`rx<n>/s` replaces the cumulative frame count.** A lifetime total tells you
  the link is up; it does not tell you whether the radio is hearing anything right
  now, which is the question you have while parked next to a camera that is not
  showing up.
- **`b<n>`: BLE adverts this session.** The Flock screen previously showed nothing
  at all about BLE, so in `flockcombo` mode a working BLE half and one that never
  ran looked identical. BLE is usually the easy detection on these cameras.
- **`!r<n>`: the companion restarted.** A lifetime counter can only fall if the
  board rebooted, and that was being silently absorbed by the per-session rebase
  -- so it just looked like the count sliding back toward zero. Reported as a
  cosmetic oddity on a long drive; it actually meant the ESP was resetting and
  dropping detections in between.

## v0.55
**ESP32-C5 correctness.** The one person field-testing this runs a C5, and three
separate things in the app assumed every board was a classic ESP32.

### Fixed

- **The GPS pin picker no longer offers pins that can brick the link.** It was a
  hardcoded classic-ESP32 list. On a C5, four of its ten pins do not exist
  (GPIO stops at 28), two are the flash/PSRAM bus, and one is UART0 itself -- so
  the picker could hand you the pin carrying the Flipper link, which needs a
  recovery flash to undo. The board now reports its own usable pins and the app
  offers those. Until it does, a conservative fallback is used, plus whatever pin
  you already had, so an existing working setup is never silently changed.
- **The companion's pin guard asks the chip instead of assuming.** It was
  `rx != 1 && rx != 3 && rx < 48`, which is the classic ESP32's pinout written as
  if universal. It is now derived from this target's own IDF headers, so it
  refuses the real UART0 pins, the real flash bus and out-of-range pins on
  whatever part it was built for. A refusal now also says why.

### Added

- **Band is a Settings item, and defaults to 2.4 GHz.** The companion defaulted a
  C5 to sweeping both bands: 41 channels instead of 13, which at the same dwell
  is ~12.3 s per sweep instead of ~3.9 s, so **any given camera is revisited a
  third as often**. A user parked beside three known Flock cameras and detected
  none while the radio spent two thirds of its time on 5 GHz. Covering a band we
  cannot yet confirm anything uses must not cost two thirds of the dwell on the
  band every signature we hold actually lives on. 5 GHz and Both remain available.

## v0.54
**Three bugs from [@h00die](https://github.com/h00die) in
[#5](https://github.com/ReconGrunt/FlipDeFlock/issues/5), one of them a data-loss
regression this project shipped in v0.53.**

### Fixed

- **Alerts never fired for a camera you had already saved.** `hits.csv` restores
  each entry with its alert latch already set, so a reboot cannot buzz at you --
  but nothing ever cleared it, and the restored confidence also failed the
  "crossing" test. The result: turning on **Save Hits** silently disabled alerts
  for every device you had ever driven past, permanently and across reboots.
  Meeting a stored device on the air is now treated as the new event it is. This
  was reported twice as "alerts don't work"; the v0.52 fix addressed a different
  cause and this one survived it.
- **Net Guardian destroyed your Flock detections.** Opening it wiped the entire
  detection table, including entries restored from `hits.csv`, and leaving it then
  wrote that empty table back over the file -- so a reboot could not bring them
  back either. A user lost a drive's worth of hits in the field to this. The clear
  was never needed for scoring: the watchscore already skips archived entries and
  already gates live ones on freshness, so it bought nothing and cost the data.
- **`hits.csv` is no longer removed as a side effect of an empty table.** v0.53
  made an empty table delete the store, meaning ANY code path that cleared it in
  memory became permanent loss on disk -- which is what turned the Net Guardian
  bug above from "the list looks empty" into "the file is gone". Removal now
  happens only where the operator explicitly deletes the last entry.

### Added

- **The GPS badge can now tell you WHICH thing is wrong.** The companion echoes
  `GPSCFG,<on>,<pin>,<baud>` for every relay command and the app was discarding
  it, so with the ESP32 as GPS source every failure looked identical: a hollow
  "searching" badge, forever. Now `GPS?` means the board never answered (wrong or
  old firmware -- reflash) and `GPS!` means it answered and refused the pin
  (change the pin). Those need opposite fixes, which is why the distinction
  matters.
- **The relay config is re-sent when the companion announces itself.** It was sent
  once when a scan started, which a board still booting simply missed -- and a
  silently dropped config is indistinguishable from a dead GPS module. A banner
  also arrives on every ESP reboot, so a power blip now re-arms the relay by
  itself.

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
