# Changelog

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
