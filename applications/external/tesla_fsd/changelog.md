## 2.16-beta.18 — black-box records injected (TX) frames + variant AP-status display fix

- **Black-box now records our injected/TX frames, not just the bus (#108).** Until now the recorder captured **RX only**, so on a single-CAN tap our own injected frames never appeared in the capture — which is exactly why @dunckencn couldn't find them, and why the earlier "injection lands ~0.2 s before the abort" reading was actually measuring the car's *native* `0x3EE` broadcast rather than us. Every injected/modified frame (all routed through the single TX chokepoint) is now recorded and tagged: in the `.log` the interface reads `can0TX` / `can1TX` (RX stays `can0` / `can1`, unchanged, so existing parsers are unaffected), and the `.json` summary carries a `tx_frames` count. Same key-ID filter as RX; recording never gates the send. This lets a capture show our injection timing right next to the DAS `3 → 6 → 8` ladder — the tool needed to see whether the single-frame inject is landing on the abort edge. Thanks @dunckencn for pinpointing the RX-only gap (and for self-capturing TX to prove the injection latency in the meantime).
- **AP status no longer stuck on "Waiting" for Signal-Map / variant cars (#122).** When a variant profile or a manual Signal Map reads `DAS_autopilotState` from a non-standard byte, it updated the internal AP state (so the nag killer worked) but not the `ap_active` flag the dashboard's AP-status pill reads — so the pill showed "Waiting" even while genuinely engaged. `ap_active` is now derived from the mapped state (HW4 `>= 2`, HW3 `== 3`), matching the standard parsers. Thanks @ssw0209-sys for the report + screenshots.

## 2.16-beta.17 — experimental "Minimal Inject" (narrow-road steer-jerk)

- **Minimal Inject (experimental, off by default, #108 / #129).** Building on beta.16: @dunckencn confirmed Instant Engage eliminated the steer-jerk on wide/straight roads (67 activations, zero jerk), but it persists on narrow roads/curves — where the car aborts in ~6 ms (WARN→jerk, no lead) and our `0x3EE` injection, being *continuous*, always has a frame right on the abort edge. @dunckencn also discovered that **once FSD is triggered it stays active even after the module loses power** — so continuous injection isn't needed. Minimal Inject injects only a brief burst (`AP_MINIMAL_INJECT_FRAMES` = 5 frames) at the start of each engagement to trigger FSD, then **stops** until the car disengages — moving our injection to the *onset* of engagement (~1.5 s before the abort) and off the abort edge entirely. Re-arms on the next engagement. Stacks with **Instant Engage** (edge = *when* injection starts; minimal = *how long*). Toggle in the ESP32 dashboard and Flipper Settings ("Minimal Inject (exp.)"). **A/B for the narrow-road jerk:** turn on Instant Engage **+** Minimal Inject and see whether the jerk goes and FSD still activates + stays latched. Host-tested.

## 2.16-beta.16 — experimental "Instant Engage" (steer-jerk / activation-lag A/B)

- **Instant Engage (experimental, off by default, #129 / #108).** AP-First normally waits for AP to be engaged (`das_ap_state >= 3`) **and held stable for 1 s** before injecting. That 1 s debounce turned out to pull two ways: @7hf6cfqzkb-png (#129) found it makes FSD activation feel sluggish, while @dunckencn's black-box data (#108) showed the debounce pushes our injection right onto the `3 → 6` transition that correlates with his steer-jerk. **Instant Engage** injects the moment AP is genuinely engaged (skips the 1 s wait) — which is both faster (for #129) and *earlier* than the jerk window (for #108), so it may help both. Still blocks injection while AP is merely available (state 2), so it doesn't reintroduce the AP-off injection. Toggle in the ESP32 dashboard and Flipper Settings ("Instant Engage (exp.)"). **Please A/B it against the default** — the check is simply whether FSD still activates cleanly, and whether the lag (#129) / jerk (#108) improve. Host-tested.

## 2.16-beta.15 — black-box fixes (WiFi, capture window, timestamps) + AP-First off-by-one

Four fixes, all from @dunckencn's on-car testing + black-box captures on #108/#122.

- **AP-First no longer injects `0x3EE` while AP is off (#108).** The AP-First gate treated `das_ap_state >= 2` as "engaged", but the standard `DAS_autopilotState` enum has **2 = AVAILABLE (offered, not engaged)** and **3 = ACTIVE_NOMINAL (first genuinely engaged)**. @dunckencn's black-box capture proved it: with AP off, `das_ap_state` bounced 1↔2 while `0x3EE` was being injected, whereas a real engage went `2 → 3 → 6`. So the gate now fires only at `>= 3` (actual engagement) — no more injecting on the AP-activation frame while merely available. Same class as the fix ev-open-can-tool made in 3.0.2-beta.1 (thanks @dunckencn for the pointer). Also corrected the wrong `das_ap_state` enum comments across the code. **Needs on-car re-validation that FSD activation timing is unaffected.**
- **Black-box no longer boot-loops / drops WiFi from a filesystem scan (#124).** The event-count did a LittleFS directory scan on a timer, which got slower as events accumulated and dropped the WiFi on the S3. Now cached — updated only on save/delete, never on a status poll. (@dunckencn confirmed: keeping events low avoided the drop; this removes the cause entirely.)
- **Black-box captures the full window on a busy bus (#124).** On @dunckencn's Legacy full bus (~3,300 frames/s), recording every frame filled the 6,000-frame ring in ~1.8 s — so the 10 s pre / 5 s post window was truncated and missed the abort lead-up. The recorder now stores only the **key diagnostic IDs** (`0x370`/`0x399`/`0x39B`/`0x488`/`0x129`/`0x3EE`/`0x3FD`/`0x145`/`0x238`/`0x389`/`0x2B9`/`0x118`), dropping the stored rate ~15× so the ring covers ~15–30 s. Define `BLACKBOX_CAPTURE_ALL` at build time to keep every frame.
- **Black-box event timestamps fixed.** They showed `t≈4294964s` (a `millis()` underflow when a capture's first frame landed after the trigger); now clamped to sane values.

## 2.16-beta.14 — dashboard blank-data fix (the real fix for the beta.12/13 S3 reports)

- **Fixed: dashboard connects but shows all blank / flickers on S3 (#124).** This is the actual cause of the beta.12/13 "boot-loop / can't read device info" reports (thanks @ssw0209-sys, @dunckencn — and @ssw0209-sys's screenshot pinned it: the device was *up* and the WebSocket *connected*, but every field was blank, i.e. a data-delivery problem, not a memory reboot). The dashboard rebuilt three heavy status blocks — including a **LittleFS directory scan** for the black-box event count — on *every* ~1 Hz WebSocket push. On the Waveshare S3 (LittleFS backend) that per-second filesystem I/O on the web task stalled the state broadcast, so the dashboard never received data. beta.13's memory / default-off change didn't help because that scan ran **regardless** of the enabled flag. Fixed by moving the black-box / Tap Check / variant-profile blocks **off** the hot WebSocket push onto an on-demand `GET /api/aux` endpoint the dashboard polls every 2.5 s — the core state push is back to the small, known-good beta.11 shape (~1.3 KB). **If beta.12/13 gave you a blank or flickering dashboard, flash this.** Apologies for the two earlier builds that fixed the wrong layer.

## 2.16-beta.13 — black-box boot-loop hotfix + Vehicle-bus reachability (testing build)

- **Hotfix: the black-box recorder no longer boot-loops on no-PSRAM S3 boards (#124).** beta.12's recorder allocated its ~108 KB RAM ring **at boot, before WiFi** — on a Waveshare S3 without PSRAM that starved the WiFi/web stack, so the device rebooted in a loop and the dashboard wouldn't load (thanks @ssw0209-sys for the report). Fixed: the ring is now allocated **lazily** (only when the recorder is enabled), **heap-guarded** (it won't allocate if that would leave WiFi under a 60 KB margin — it stays off and says so instead of starving the link), freed on disable, and **the recorder now defaults OFF**. A device with the recorder persisted on reconciles *after* WiFi is up (guarded), so it can't re-trigger the loop either. Also bumped the dashboard state-JSON buffer (it had been reallocating on every push). **If you were affected, flash this build — it boots clean.**
- **Tap Check: Vehicle-bus body-control reachability (#128).** The capability checker now also reports whether your tap reaches the **Vehicle/body bus** — it detects `0x273` (UI_vehicleControl), `0x102` (VCLEFT_doorStatus), `0x119` (VCSEC_windowRequests), `0x3E9` (DAS_bodyControls) and tells you which are present, so you can see whether body/comfort control is even physically possible on your wiring. **Read-only presence check** — it does *not* prove those frames can be actuated (they're likely counter/CRC/auth-gated); that's a separate on-car step.
- Thanks @ssw0209-sys for the fast, clear boot-loop report.

## 2.16-beta.12 — Field-Readiness: black-box recorder + tap capability checker + variant auto-profiles (testing build)

The theme: make the device understand the bus it's plugged into and auto-record incidents, so the hard 2026.14.x problems stop depending on a tester *catching* the moment. All three are ESP32-first (dashboard), on a shared event-core in `fsd_logic/`. Full initiative + status: #127.

- **Black-box incident recorder (#124, ESP32).** A RAM ring buffer continuously records every CAN frame (all IDs, full rate, all buses). When the shared event-core detects an **abort** (`DAS_autopilotState → 8/9`), a **bus-off**, or a dashboard **Mark**, it freezes ~10 s before + 5 s after the trigger and writes two files: a **pure candump `.log`** (drops straight into `tools/tesla_crc_cracker.py`) and a **decoded `.json` summary** (trigger + transition, HW, firmware-era, buses / dual-CAN, active toggles, a `das_ap_state` timeline). So a steer-jerk / abort auto-captures itself — no more catching it by hand. Storage by board: LittleFS (persistent, keeps the newest 5) where there's a data partition, SD on LILYGO, RAM + one-click dashboard download on the small `min_spiffs` boards. **On by default on persistent-storage boards** (incl. Waveshare S3), off on the volatile RAM boards. PSRAM-aware ring (the S3 internal fallback covers the full window without PSRAM). Dashboard "Black-box" card: event list, download, delete, ⚑ Mark now, new-event badge. Host-tested. *(On-car item: heap headroom for the S3 ring under live WiFi — fails safe, disables itself if it can't allocate.)*
- **Tap capability checker (#125, ESP32).** Listens a few seconds and reports, per feature, whether **this tap carries what it needs** — not a guessed bus name (the X179 pin↔bus map varies by harness). Nag killer needs `0x370` **+** DAS state (`0x39B`/`0x399`) → it tells you "works here" or "**dual-CAN recommended**"; AP-First needs DAS state; Soft Engage needs `0x129`. Handles the `0x399` trap (DAS state on HW3/Legacy, but the ISA chime on HW4). Best-guess bus name is shown only as a hint ("confirm in Service Mode → CAN Port"). Directly targets the #1 "nag killer does nothing on HW4" cause. Pure RX — never transmits. Host-tested.
- **Built-in `0x39B`/`0x399` variant auto-suggest (#126, ESP32).** Extends the Signal Map: when the standard parser can't read AP-state on a variant layout (e.g. @ssw0209-sys's byte0 hi-nibble `0x39B`), the device matches against built-in profiles and **suggests** one — one tap pre-fills the Signal Map. Strict single-match, only when the standard parser is failing, and **never silent auto-apply** (a false-positive is worse than no profile). Doesn't touch the existing Highland byte0 auto-latch (#116). Host-tested.
- **Security: gate black-box downloads behind admin auth when an AP password is set.** `/blackbox/list` and `/blackbox/get` serve recorded CAN (VIN, drive data), so they now require HTTP Basic Auth **when an AP password is set**, and stay open otherwise (the no-password one-click download workflow is unchanged). Path-traversal-safe (`?name=` restricted to `[A-Za-z0-9_]`).
- **CI + docs.** Build the `m5stack-atom-matrix` env so its `.bin` ships with releases (was missing from the build matrix). README now documents the 14.x experimental toggles (Soft Engage / Abort Guard / Signal Map / Nag Burst / EPAS-faithful) in all three languages, and the running-14.x-tracker link points to #122.
- Thanks **@dunckencn** and **@ssw0209-sys** — the black-box, capability, and variant work all came straight out of their on-car captures and testing.

## 2.16-beta.11 — Abort Guard + configurable signal mapping + Nag Burst (testing build)

- **Abort Guard — steer-jerk mitigation (#108, experimental, off by default).** Root-caused from @dunckencn's own CAN logs (his 6/22 `ids_3EE_399_488_118_145` set: two steer-jerk logs + a normal AP+FSD reference). Decoding `0x399 DAS_autopilotState`: his clean run reaches `ACTIVE (6)` and **holds ~9 s**; both jerk runs reach `6` then immediately go `8 ABORTING → 9 ABORTED` within ~0.5 s — exactly at the jerk — with a large `0x488` steering-command excursion at that instant. The abort *is* the jerk. Critically, the injected `0x3EE` is **byte-for-byte identical** in the jerk vs clean runs, so we send nothing different — the abort is the car's own decision (it matches his AP-only finding: that narrow road triggers the car's corrective-steering; AP-only stays a soft correction, AP+injection escalates to abort+jerk). Abort Guard, when on, cuts all activation injection (`0x3FD`/`0x3EE`) the instant the car enters an abort state and latches off until a clean disengage (`das_ap_state < 2`), so we never feed or repeat an in-progress abort. Shared core (Flipper + ESP32); ESP32 dashboard toggle + NVS. Host-tested (296). **Needs on-car validation — this is a probe, not a confirmed fix.** Thanks @dunckencn for the methodical no-injection vs injection captures that made the mechanism visible.

- **Configurable signal mapping + freshness gate for the nag killer (#122).** Borrowed from the same in-the-wild Mode-C port. The nag killer reads its context — AP-state, hands-on state, steering angle — to decide whether to inject; on cars whose `0x39B`/`0x399` byte layout differs (e.g. @ssw0209-sys's 3rd `0x39B` variant where AP-state was unreadable, leaving the EPAS-faithful path inert) our auto-parsers can't find those signals and the killer never engages or never gates. **Signal Map** (ESP32 dashboard → advanced) lets you override exactly where each signal lives: DAS frame id + byte/shift/mask for AP-state and hands-on, and the steering frame id + hi/lo bytes. Leave DAS id `0` for auto-detect (unchanged default behaviour). Paired with a **freshness gate**: when a custom DAS id is set, the killer only injects while a matching context frame has arrived within the last second — so a stale/misconfigured map fails closed (no blind echo) rather than injecting against unknown state. Persisted in NVS; auto-parsers stay fully active whenever the matching id is `0`. Host-tested (286).

- **Nag Burst (burst/pause injection) + ±1.8 Nm torque cap (#122).** From reviewing the in-the-wild `06066060606060/nag-killer` port (nicolozak's algorithm, tested on Model Y HW4 2026.20 Party CAN, surfaced by @ssw0209-sys): (1) **Nag Burst** — a new opt-in that echoes `0x370` in bursts (~1 s on / ~1.5 s off) instead of continuously. The rest period is the believed reason a TSL6P-style device evades the stricter 14.x detector, where our continuous echo trips it. Off by default; toggle in Flipper Settings and the ESP32 dashboard. (2) **±1.8 Nm torque hard-cap** on every nag path — the in-the-wild devices clamp here, and >1.8 Nm is reported to trigger FSD disengagements during turns. This drops our legacy grip pulses (were ~3 Nm) and caps the EPAS-faithful ramp (was 2.1 Nm) to 1.8 Nm. Credit: nicolozak (algorithm), the 06066060606060 ESP32 port, @ssw0209-sys / @DrStrangeglovebox (surfacing it). Host-tested (276).

## 2.16-beta.10 — Soft Engage (steer-jerk) + EPAS-faithful handsOnLevel fix (testing build)

- **EPAS-faithful: stop setting handsOnLevel (#122).** Torque-model analysis of ground-truth-labelled HW3 14.6 captures (@LonelyCheese09, @jim608) + the HW4 Feifan dump shows real EPAS keeps `0x370` handsOnLevel (byte4[7:6]) at **0 even when hands are genuinely on** (clean-nag 142/142 = 0). The nag escalation lives in `0x399`/`0x39B` `das_hands_on_state` (ladder 1→…→7), not handsOnLevel. So Mode-C deriving handsOnLevel from torque was non-EPAS-like and a likely 14.x preflight tell (explains why it made @ssw0209-sys's car worse). EPAS-faithful now leaves handsOnLevel untouched; it still does the demand-state torque. (Legacy killer's handsOnLevel-flip is unchanged — it works pre-14.x.)

- **Soft Engage — steer-jerk mitigation (#108, experimental, off by default).** New opt-in that holds the activation-edge injection until the steering wheel is near-centred (within ±5°), so the DAS's steering-path recompute when FSD enables is small. The steer-jerk @dunckencn reports is the car's own steering correction at the injection moment, and it's worse on curves (bigger path delta) — a plain injection delay doesn't help (confirmed on-car), but engaging only when straight should. Latches once per engagement (so mid-drive turns are fine) and resets when AP drops. Needs `0x129 SCCM_steeringAngle` on the tapped bus to be effective; if absent it degrades to AP-First-only. Now wired on the ESP32 too (the steering parser was Flipper-only). Toggle in Flipper Settings → and the ESP32 dashboard. Host-tested (272). **Needs on-car validation.**

- **M5Stack ATOM Matrix support + GPIO 39 front-button fix (@maslyankov, from #46).** New `m5stack-atom-matrix` PlatformIO env: drives the 5×5 SK6812 grid (`LED_COUNT`, dimmer brightness so 25 LEDs stay within the USB regulator budget) and maps the button to **GPIO 39** — the ATOM's front-face button, which the firmware previously ignored (it only read GPIO 0, the side BOOT button). ATOM Lite is unchanged (single LED, GPIO 0). Re-applied to current `main` from @maslyankov's PR #46 (the original PR had drifted ~95 commits behind the dual-CAN refactor and couldn't merge as-is).
- **Continuous AP robustness + brake-state fix (#120, @vrs11).** Continuous AP (HW3/Legacy) gear-sequence hardening: counter-freshness latch, send-result tracking, and split retry/delay timing so the 0x229 double-press path recovers cleanly. Also fixes `ESP_driverBrakeApply` (0x145) to treat only states ≥2 (`Driver_applying` / `Faulty-SNA`) as braking — state 1 (`Not_Applied`) was wrongly counting as a brake-applied, and folding SNA into "applied" fails the interlock closed.
- **HW4 Highland byte0 AP-state auto-fallback (#116, @0xAccretion).** On some HW4 Highland (China MIC, 2026.20) builds, `0x39B` carries `DAS_autopilotState` in byte0 low nibble while byte1[7:4] stays pinned at 1, so the HW4 parser read AP as "off" the whole drive — pill stuck on "Waiting", AP-First never opened. The parser now auto-detects it: while byte1[7:4] holds at 1, byte0 reaching an active state across 3 frames latches that car to the byte0 reading for the session; the instant byte1[7:4] is ever seen ≠1 the fallback is disqualified, so standard HW4 is byte-for-byte unchanged. Both Flipper + ESP32; host-tested + on-car confirmed on a 2026.20 Highland HW4.
- **Docs: tap Party CAN (X179 pins 2/3) for the nag killer (#100).** `0x370` is on Party CAN — not Vehicle CAN (9/10), and the Chassis copy (13/14) trips the 14.x preflight. Confirmed working on HW4 2026.20 by tapping 2/3 (@SkyRaax). A single-CAN board on the wrong pair has nothing to echo — the usual "nag killer does nothing on HW4" cause. (README + HARDWARE.md.)
- **14.x status (honest):** on 2026.14.x / 2026.20, the **nag killer works when tapped on Party CAN (2/3)** with the normal killer. The **EPAS-faithful (Mode-C) toggle** from beta.8 is still experimental and **not yet confirmed on-car** — the wins so far came from the right bus, not that mode. **FSD activation** on 14.x and the **region/tier lock** on FSD-entitled HW4 2026.20 remain unsolved (the tier is re-derived server-side and can't be spoofed via CAN — @weigibbor, #117). Pre-14.x FSD unlock is unchanged.
- Thanks: @0xAccretion (HW4 Highland AP-state fix + on-car), @vrs11 (Continuous AP + brake fix), @maslyankov (ATOM Matrix + GPIO 39), @SkyRaax (Party-CAN nag confirmation), @weigibbor (2026.20 region-lock TX-test), @jewelrylin / @DrStrangeglovebox / @dunckencn / @ssw0209-sys (on-car data).

## 2.16-beta.8 — EPAS-faithful nag rewritten as a demand-state (Mode-C) machine + capture during Active (testing build)

- **EPAS-faithful nag mode rewritten (#100).** beta.7's first attempt (a flat torque walk with handsOnLevel left at 0) was too naive and @ssw0209-sys confirmed it didn't hold. The mode is now a demand-state machine ported from the in-the-wild "Mode C" reference (surfaced by @ssw0209-sys), which works on 2026.14.x:
  - **handsOnLevel is derived from the synthetic torque magnitude** (|t|≥2 Nm → 2, ≥1 Nm → 1, else 0) so the frame is internally consistent — instead of forcing it (legacy killer) or pinning it to 0 (beta.7).
  - **Torque escalates with `DAS_autopilotHandsOnState`:** nothing in state 0/1, a 2 s idle delay then a mild ±0.5–2.0 Nm random walk in state 2, and a 1 s pause then a 0→2.1 Nm ramp-and-hold (1.5 s cycle) in states 3/4/5.
  - **Torque is directed opposite the steering angle** (`0x129`, where parsed), and the whole thing is gated to AP being active.
  - Still behind the **"Nag EPAS-faithful"** toggle, **off by default**, both builds. Host-tested (248 assertions: AP-state gate, state-2 delay+band, state-3 ramp, handsOnLevel-from-magnitude, counter+1, checksum). **Experimental — needs on-car validation.** Note: the nag killer only works on the bus that carries `0x370` (Chassis CAN on most harnesses, *not* Vehicle CAN — see HARDWARE.md); tapping the wrong pair means there's nothing to echo.
  - ⚠️ Same safety note as below: test only in a safe, empty area, hands ready.
- **Web-stream CAN capture now works in Active mode (#108).** The capture previously recorded only in Listen-Only, so it stopped the instant you hit Activate — making the steer-jerk transient (which only happens during injection) impossible to record (@dunckencn). Multi-ID captures now record in both modes; the single-ID hardware filter stays Listen-Only-only so injection's RX path is never starved.
- **HARDWARE.md: X179 13/14 relabelled Chassis CAN (#100/#114).** Per @jewelrylin's Service Mode CAN Port map (harness `1933903-XX`), pin 13/14 is Chassis CAN, not "Bus 6" — which is why `0x370 EPAS3P` is there (EPAS lives on Chassis). `0x370` is **not** on Vehicle CAN (9/10), so the nag killer only works on the pair your car's Service Mode page labels Chassis. The pin→bus map varies by harness (≥4 configs) — Service Mode is the deterministic check.
- **Safety note corrected (@DrStrangeglovebox).** The forced-disengagement / tripped-efuse behaviour was seen only on FSD v13, not v14, and the fuses reset on their own after the car sleeps ~20 min — no service required. Less severe than beta.7's note said; still test only in a safe, empty area, hands ready.
- Thanks: @ssw0209-sys (the Mode-C reference + the honest negative beta.7 result), @dunckencn (the capture-during-Active need), @jewelrylin / @DrStrangeglovebox / @0xAccretion (the bus + DAS-layout data underpinning all of this).

## 2.16-beta.7 — TWAI bus-off recovery + HW-detect guard + experimental EPAS-faithful nag (testing build)

- **EPAS-faithful nag mode (experimental, #100)** — a new approach to the 2026.14.x nag, decoded from a 22,598-frame capture of a working in-the-wild commander (V4.1.00) that suppresses the nag on Bus 6 *without* tripping the 14.x preflight (capture by @DrStrangeglovebox). The key finding: the working scheme **never flips handsOnLevel** — it sends a realistic torsion-bar torque centred at ~0 Nm with large *live variance* (measured mean 0.31 Nm, stdev 1.48 Nm), with the genuine rolling counter and checksum, so the frame reads as real EPAS and passes the content check. Our existing killer flips handsOnLevel and pushes a steady ~1.8 Nm — almost certainly the tell the 14.x preflight catches. The new mode mirrors the wild scheme: no handsOnLevel flip, torque centred at 0 Nm with a mean-reverting random walk, valid counter + checksum (`sum(b0..b6)+0x73`, validated 22,598/22,598). New toggle on both builds (**"Nag EPAS-faithful"** in Flipper Settings → Beta, and in the ESP32 dashboard), **off by default**. Reference encoder/validator added at `tools/feifan_0x370.py`. **Experimental — needs on-car validation; the suppression mechanism is inferred, not yet confirmed with our own frames.** Host-tested (handsOnLevel never forced, counter+1, checksum valid, torque centred ~0 with live variance).
  - ⚠️ **Safety:** torque injection during FSD carries risk — still test only in a safe, empty area with hands ready. Severity update from @DrStrangeglovebox: the forced-disengagement / tripped-efuse behaviour was seen **only on FSD v13** (an error the car itself triggers), not on v14; and the fuses **reset on their own after the car sleeps ~20 minutes — no service required**. So less severe than first reported, but still treat any disengagement as potentially leaving the car briefly unpowered.
- **TWAI bus-off auto-recovery (#108)** — when injecting on a bus where TX errors accumulate, the ESP32 TWAI controller could enter bus-off and stop delivering frames entirely (dashboard CAN frames/s → 0 after a few minutes), recoverable only by a manual Deactivate/Activate toggle (reported by @dunckencn on M5Stack ATOM). The driver now detects bus-off and runs the ESP-IDF recovery sequence automatically, so RX resumes on its own. Acts only in the bus-off state; a healthy bus is unaffected. (Note: this is separate from the web-stream "capture stops on Activate" behaviour, which is by design — the stream records in Listen-Only only; allowing multi-ID capture during Active is tracked for the steer-jerk work, #108.)
- **All-zero 0x398 detection guard (#110)** — an all-zero `GTW_carConfig` stub (forwarded onto Bus 6 on some HW4 trims) is no longer read as `das_hw=0 → Legacy`, which had mislabelled HW4 cars and routed them through the 0x3EE path. An empty payload now yields Unknown so detection falls through to live markers.
- **HARDWARE.md: post-SOP10 connector change** — documented that on newer Model Y the third CAN pair moved from pins 13–14 to 4–5 (now Body CAN) and Chassis CAN relocated to a new left port (X177); corrects where to tap for Chassis/EPAS work on post-SOP10 builds (writeup by @mamixsystem).
- Thanks: @DrStrangeglovebox (the Feifan reference capture + the efuse safety warning), @jewelrylin (the frame-content preflight test), @dunckencn (the bus-off report), @mamixsystem (the SOP10 connector reference).

## 2.16-beta.6 — ESP32 AP-First (gates 0x3FD / 0x3EE / 0x370) (testing build)

- **AP-First on the ESP32 (#108 / #52 / #100)** — the AP-First gate is now on the ESP32, not just the Flipper. New **"AP-First (14.x)"** toggle in the web dashboard (off by default). When on, AP/FSD/nag injection is held until Autopilot is engaged and has been stable for 1 second, instead of firing on the activation edge. It gates the FSD-enable frames (`0x3FD` HW3/HW4, `0x3EE` Legacy) AND the nag-killer `0x370` echo — the latter because on 2026.14.x, injecting `0x370` before AP is engaged can itself block AP from becoming ready (on-car finding by @jewelrylin, #100). This is the "start-after-AP" behaviour @dunckencn measured working ~95% on China 2026.8.3.6 HW3 (#108). Strictly conservative: it can only withhold injection until AP is up, never inject more; default-off preserves prior behaviour.
  - **Usage on 14.x:** turn on AP-First, then engage Autopilot from the stalk first — the device starts injecting once AP is up and steady.
  - **Honest limit (per #100):** on HW4 Juniper over Bus 6, AP-First lets AP become engageable again, but the `0x370` echo can still trip the preflight *after* engagement on that bus — the full fix there is Vehicle-CAN injection (tracked separately). On other buses / Legacy / HW3, AP-First should help directly.
- Thanks: @jewelrylin, @DrStrangeglovebox, @dunckencn, @kristopf007 — the on-car testing that pinned down the `0x370` preflight surface and the start-after-AP requirement.

## 2.16-beta.5 — HW4 nag killer: escalation-edge grip pulse + honest AP-First banner (testing build)

- **HW4 Juniper nag killer fix (#100)** — beta.4 suppressed the first hands-on nag wave but the second still escalated to yellow on HW4 Juniper trims. Root cause: on those trims EPAS handsOnLevel (`0x370` byte4) is frozen at 0 the whole hands-off window (measured constant by @jewelrylin / @DrStrangeglovebox), so the strong grip pulse — which edge-detected only handsOnLevel — fired once and never re-armed. The signal that *does* move is `das_hands_on_state` stepping 0→2→3. The grip pulse now re-arms on a DAS escalation rising edge too, tracking das every frame so a satisfied (0) wave resets the baseline. It only changes *when* the pulse fires, not the gate or the echo, so HW3 and steady-das HW4 cars are unchanged. The ESP32 nag path had no on-demand pulse at all — added it with the same DAS-edge re-arm so both builds match. Host-tested; needs on-car confirmation across two consecutive nag waves.
- **Honest AP-First banner on the ESP32 dashboard (#52/#108)** — the 14.x warning told users to "enable AP-First in settings", but AP-First is **Flipper-only** — there is no such control on the ESP32. Reworded to say so and give the manual workaround (engage AP from the stalk first). Porting the AP-First gate to the ESP32 is tracked separately.
- Thanks: @jewelrylin and @DrStrangeglovebox (the dual-unit HW4 nag captures that diagnosed this), @kristopf007, @dunckencn.

## 2.16-beta.4 — Continuous AP (HW3), HW4 nag fallback, 0x229 cracked + gated (testing build)

- **Continuous AP (HW3/Legacy, #107, @vrs11)** — enhAuto-style auto-re-engage: while Autopilot is active it watches the disengage conditions, and if AP drops with a turn signal on it waits for low steering torque + signal off + AP ready, then re-engages (max 3 attempts). Brake pedal or a full right-stalk-up is the kill switch, re-checked in every state. **Opt-in (off by default), HW3/Legacy only** — the HW4 engage sequence isn't identified yet so the HW4 path is stubbed inert. The brake interlock fails closed: it won't engage until a `0x145` (ESP_status) frame has been seen.
- **HW4 nag fallback (#100)** — on HW4 trims that never broadcast `0x39B` (e.g. Juniper RWD on Bus 6), the nag gate now reads `DAS_handsOnState` from `0x399` (same byte5[5:2] field) so the hands-on echo isn't starved. Verified against @jewelrylin's captured nag run (the field steps 1→2→3 as the visual nag escalates); gated so cars that do send `0x39B` are unaffected.
- **`0x229` SCCM_rightStalk checksum cracked + tool** — `tools/crack_0x229.py` recovers the AUTOSAR-E2E-style checksum (CRC8 poly 0x2F over data + a counter-indexed Data-ID, collapsed to a per-counter constant). Reproduces 224/224 real frames across two independent full-rate captures. **But injection is a dead end on a shared bus** — the genuine SCCM never stops sending `0x229`, so an injected pull collides bit-for-bit and breaks the rolling-counter sequence (this is why `0x3C2` ScrollPress works and `0x229` doesn't). The cracked checksum is documented; it is not a working engage path.
- **`0x229` blocked from loadable profiles (safety)** — a pulled-down `0x229` is a request to shift into DRIVE, which the parked/stationary interlock can't catch (it gates timing, not semantics), so the `.cantest` runner now refuses `0x229` lines on load.
- Thanks: @vrs11 (Continuous AP), @DmitroPanteliuk (`0x229` full-rate captures + validation), @jewelrylin (HW4 Juniper Bus-6 captures + full-rate confirmation), @se7en7777777 (`0x485`/Highland/checksum analysis), @JakNo (`0x3C2` scroll decode), @BenjaminFaal, @deftdawg, @jangshik, and everyone testing on-car.

## 2.16-beta.3 — full-rate single-ID capture + AP-First stability debounce (testing build)

- **Full-rate single-ID CAN capture (#104)** — the port-82 `?ids=` stream now installs a hardware acceptance filter on the CAN controller when you request exactly one ID, so that ID is captured at true full rate instead of being decimated. The RX queue was overflowing on a busy Vehicle CAN (a 10-frame TWAI queue against thousands of frames/s), which aliased the rolling counter+CRC to noise and hid sub-0.5s stalk/button presses — the reason single-ID counter+CRC cracks kept failing. Now: filter to one ID, the queue can't overflow, every consecutive frame lands. The TWAI RX queue was also raised 10→64 for multi-ID captures, and the same single-ID filter is implemented for MCP2515 (only two RX buffers, so it gains the most). Validated on-car: @DmitroPanteliuk's `0x229` capture shows the counter incrementing 0→F with zero skips where it was previously a uniform ~0.5s sample, and @jewelrylin independently confirmed full-rate (`0x129` at 100 Hz) on a Waveshare S3.
- **AP-First stability debounce** (ev-open-can-tools v3.0.2-beta.2 parity) — AP-First now requires AP to hold stable for 1 second before `0x3FD`/`0x3EE` injection, not just be momentarily active, to avoid injecting on the activation edge. Off by default; no change unless AP-First is enabled.
- **`0x229` SCCM_rightStalk** (pre-Highland stalk, #95) — full-rate captures decode the frame as byte 1 = `[stalk position | rolling counter]` (idle 0, pull-down 4) with byte 0 a checksum. The checksum provably is not a CRC8 over the visible bytes, so a full-DLC capture is still needed to finish it. `0x229` only exists on pre-Highland cars — Highland/Juniper dropped the physical stalk (thanks @se7en7777777, @jewelrylin). `0x485 PM_shiftState` is the accepted/current gear state, not a shift request, so replaying an edited `0x485` will not command a shift.
- **HW4 nag (#100, under investigation)** — @jewelrylin's HW4 Juniper captures show `0x39B` absent on Bus 6 while `0x399` carries the hands-on escalation; the HW4 nag gate reads `0x39B`, so it is starved on that trim. A `0x399` fallback is being scoped.
- Thanks: @DmitroPanteliuk (`0x229` full-rate validation on #104/#95), @jewelrylin (HW4 Juniper Bus-6 captures + full-rate confirmation), @se7en7777777 (`0x485` / Highland stalk correction), @BenjaminFaal, @vrs11, @deftdawg, @jangshik, and everyone testing on-car.

## 2.16-beta.2 — user-loadable SEND test profiles + Legacy AP-First fix (testing build)

- **Send Test (user-loadable `.cantest` profiles)** — the closing half of the test loop. Edit a plain text file on any computer (`ID#DATA [repeat=N] [delay=Nms]` — the same candump format CAN Capture writes, so a captured line can be tweaked and replayed), drop it on the SD card under `apps_data/tesla_mod/tests/`, and run it from the Flipper menu (**Send Test [BETA]**). Defaults to dry-run; transmitting is hard-gated and **fail-closed** to a **parked + stationary** car (a fresh `0x257` DI_speed frame must show speed ~0), re-checked before every frame so motion aborts the burst, and each run is logged to `tests/results/` for a bug report. Format + workflow: `docs/cantest-format.md`; example: `examples/example.cantest`.
- **Cracker now sweeps covered-byte subsets** — `tools/tesla_crc_cracker.py` recovers CRC8s that cover only part of a frame (real Tesla frames often exclude a rolling counter), via an affine collapse that keeps the search fast. Confirmed on a real `0x229` capture from #95 that no CRC8 matches it — that dump is from the gateway-forwarded Bus 6 (X179 pin 13/14), where the original SCCM counter/CRC isn't preserved; capture from the direct vehicle bus (pin 9/10 or OBD-II) instead.
- **Legacy AP-First fix** (ev-open-can-tools#66) — the Legacy `0x3EE` path was missing the AP-First timing gate, so it could inject FSD-enable before AP was stable. Upstream links that early-injection timing to a sharp steer-jerk at activation on some Legacy/HW3 cars (China FW 2026.8.3.6). Now gated on AP active, matching the `0x3FD` path.
- **ESP32 STA WiFi + dashboard config sections** (PR #102, @vrs11) — the ESP32 can join an existing network / phone hotspot instead of only hosting its own AP, with a serial `ip` command and collapsible dashboard sections.
- **212-assertion host test suite** now covers every protocol-core handler (additive checksum, mux dispatch, HW3/HW4 branching, the scroll-press and GTW Config Replay state machines, every parser) plus the new `.cantest` parser and the send interlock — and gates both platform builds in CI, so a one-byte regression goes red instead of reaching a car.
- README: corrected the upstream GitLab status (the `Tesla-OPEN-CAN-MOD` group was renamed to `ev-open-can-tools`, not removed).
- Thanks: @vrs11 (STA WiFi PR #102 + the candump format the capture mirrors), @DmitroPanteliuk (`0x229` captures on #95), @BenjaminFaal (`0x485` finding), @jangshik, @deftdawg (TTGO test report), and everyone testing the beta on-car.

## 2.16-beta — Flipper CAN capture + shared protocol core (testing build)

- **Flipper CAN Capture** — new "CAN Capture" toggle in Settings. When on, the running scene appends every received frame to `/ext/apps_data/tesla_mod/captures/cap_<tick>.log` in SocketCAN candump-ASCII (`(sec.usec) can0 ID#DATA`), and the running screen shows `REC <n>`. Read-only — it never transmits, so the intended use is Listen-Only and it is safe to run on a live car. This is the Flipper side of the v2.16 baseline-capture work; the ESP32 already logs via the SD dump + the port-82 stream. A capture drops straight into the checksum cracker (`python3 tools/tesla_crc_cracker.py --id 0xNNN cap.log`) and a `car_compatibility` report — no laptop tap required.
- **Shared protocol core** — the Flipper and ESP32 builds now share one definition of the CAN frame type, the `TeslaHWVersion` / `OpMode` enums, the full `FSDState`, the Tesla additive checksum, the candump formatter, and the stateless frame helpers (bit set, mux read, FSD-selected). These were previously hand-maintained copies that had drifted. The additive checksum in particular — which the car validates and rejects a frame on mismatch — is now a single implementation, closing the class of "works on one platform, silently rejected on the other" bug. `OpMode` was renumbered to match the ESP32's persisted NVS values, so no settings migration is needed and behavior is unchanged on both sides.
- **Host test suite + CI gate** — 186 host assertions with independent oracles now cover every handler in the protocol core: checksums, bit-packing, mux dispatch, HW3/HW4 branching, the `0x3C2` scroll-press timed state machine, the `0x7FF` GTW Config Replay learn/arm/replay state machine, and every parser. CI runs them on every push and PR and gates both platform builds, so a one-byte regression in the injection logic turns the build red instead of reaching a car.
- **Please test the capture** — this is a testing build. If you have a Flipper + CAN add-on on a car: enable CAN Capture, run in Listen-Only, and confirm a `cap_*.log` appears under `apps_data/tesla_mod/captures/` containing `(t) can0 ID#DATA` lines. Captures of counter+checksum frames such as `0x485` (Highland gear shift) and `0x229` (pre-Highland stalk) are exactly what the cracker needs to recover their checksum — attach them to a `car_compatibility` issue.
- Thanks: @vrs11 (the ESP32 HTTP CAN log + candump format this Flipper capture mirrors), @BenjaminFaal (`0x485` PM_locState finding that drives the capture→cracker workflow), and everyone running the beta on-car.

## 2.15 — First 2026.14.x bypass + HW3 DAS_status fix

- **Scroll-Press AP Engage (`0x3C2`)** — first confirmed 2026.14.x bypass. Runs a time-based, human-like scroll-wheel engage gesture on `VCLEFT_switchStatus` mux=1 (hold `swcRightPressed` ~250ms → `swcRightScrollTicks` up ~150ms → `swcRightPressed` ~250ms → final scroll-up), engaging AP without touching `0x3FD` (the path Tesla's 14.x preflight check actively detects). HW4-only in v2.15; Service mode required, default OFF. Rising-edge trigger on `das_ap_state` UNAVAIL→AVAIL with one-fire-per-drive-cycle cooldown. Timing/value constants flagged pending on-car bench confirmation by @JakNo. Hardware prerequisite: bus must carry `VCLEFT_switchStatus` — X179 pin 9/10 (Vehicle CAN Bus 2 direct) or OBD-II; NOT X179 pin 13/14 (Bus 6 selectively forwarded subset). HARDWARE.md updated with the Bus 6 forwarding caveat.
- **Pre-Highland HW3 `0x399` DAS_status fix** — pre-Highland HW3 cars (Intel Atom, MCU2 generation) use the legacy Tesla CAN map where `0x399` is `DAS_status`, not the HW4 ISA chime. v2.14 read DAS from `0x39B` universally, silently breaking AP-First mode and DAS-aware nag gating for this entire cohort since v2.9. ESP32 fix landed via vrs11's PRs #92 / #93 / #94 (split from the original #78 architectural rewrite). Flipper-side mirror via PR #97. Also fixes a latent corruption bug: Suppress Chime on HW3 was overwriting `0x399` byte 1 bit 5 + recomputing an HW4-format checksum on what is actually `DAS_status` — now gated on `hw_version == HW4`.
- **On-demand grip pulse** — nag killer now fires an immediate grip excursion (3-5 frames at 3.10-3.30 Nm) when `handsOnLevel` rises into a nag-demand state (0 imminent / 3 escalated), then resets the periodic cooldown. v2.14 only emitted pulses on a fixed 5-9 s schedule, which let the yellow 2-second escalation get there first when a nag arrived between pulses. Random-walk torque between pulses is unchanged.
- **2026.14.x firmware warning banner** — default-on warning that reaches users *before* they enable any TX feature on 14.x firmware. Flipper running scene shows `!14.x: TX may stop AP` on line 4 (priority over BMS / extras flags) when the toggle is ON. ESP32 web dashboard shows a dismissible yellow banner with the dash-side UX symptom (`"Autopilot turning off"` appearing ~1 s after stalk engagement) so users encountering it recognise what they're seeing. Opt-out via Settings → `On 14.x?` (Flipper) or banner Dismiss button (ESP32, NVS-persistent).
- **GTW Config Replay** — renamed from "Ban Shield." The old name overpromised. Feature is a CAN-broadcast-layer mask that replays a learned-healthy `GTW_carConfig (0x7FF)` snapshot when the gateway emits modified frames. Does not undo NVRAM, undo backend records, or prevent bans. Six weeks of v2.9-v2.14 deployment with no empirical ban-prevention case ([#60](https://github.com/hypery11/flipper-tesla-fsd/issues/60), [#67](https://github.com/hypery11/flipper-tesla-fsd/issues/67)). NVS key preserved (`shield_enabled`) so user settings survive the rename.
- **ESP32 HTTP CAN log stream** (vrs11, PR #94) — phone-friendly CAN frame dump on port 82. 3072-frame ring buffer, candump-compatible output, optional CAN ID filter (up to 32 IDs). Intended for v2.16 baseline-capture work on banned cars — no laptop required.
- **ESP32 Ignore OTA toggle** (vrs11, PR #93) — explicit override of the auto TX-pause during Tesla OTA detection. Default OFF (safe behavior preserved); when ON, the OTA banner updates to read "TX ALLOWED BY IGNORE OTA" so the user knows the override is active.
- **HARDWARE.md addendum** — Bus 6 is a *selectively forwarded* subset of Vehicle CAN, not the full bus. Notably `0x3C2` is NOT in the forwarded list. Documented the correct tap points (X179 pin 9/10, OBD-II) and noted LILYGO T-2CAN as a planned v2.16 dual-CAN board variant.
- **Internal sweep protocol** — fixed a maintainer-side date-arithmetic bug that silently filtered out 24 h of community comments on a "check everything" sweep. Sweeps now cross-validate against unread-since-cutoff notification counts.
- Thanks: @JakNo (`0x3C2` discovery + bench verification on Highland HW4 2026.14.2, HW3 parity input, pin 9/10 capture confirmation), @vrs11 (HW3 `0x399` architectural fix + `can_signals.h` refactor + HTTP CAN log + Ignore OTA — real-car tested on M3 2020 HW3 EU), @deftdawg (on-demand grip pulse design source + tester/integrator), @bruvv (Ban Shield rename advocacy on #67, ev-open-can-tools cross-project collaboration), @jewelrylin (Bus 6 vs Bus 2 forwarded-subset diagnostic on HW4 2026.14.3 with full reproduction logs), @DmitroPanteliuk (HW3 2026.14.6 negative test driving the v2.15 HW4-only scope), @Tikernel + @ViPiMP (positive compat data: Model Y Juniper HW4 2026.2.11 China, HW4 2026.8.3 Germany).

## 2.14 — AP-First mode: 2026.14.x firmware compatibility

- **AP-First mode** — Tesla 2026.14.x added a preflight check that blocks AP/TACC engagement if CAN injection is already active on 0x3FD. When AP-First is enabled, the app monitors `DAS_autopilotState` from `0x39B` and only starts injecting after AP is confirmed active. Nag killer, TLSSC Restore, and Ban Shield are unaffected (different CAN IDs).
- **Telemetry Disable (beta)** — `UI_enableTripTelemetry=0` on `0x3F8` bit43 to disable trip data collection. Warning: may itself be a ban signal — use only with SIM pulled.
- Thanks: @cquanu (first 2026.14.2 incompatibility report), @TzCoMe (telemetry disable research), ev-open-can-tools community (confirmed 14.x preflight behavior)

## 2.13.1 — Bugfixes + Settings reorg

- **Nag killer self-disabling on startup** — `das_hands_on_state` was zeroed by memset, causing DAS-aware gate to skip echoing entirely. Now initialized to 0xFF sentinel so nag killer echoes conservatively until `0x39B` arrives.
- **Legacy→HW3 auto-upgrade was dead code** — MCP2515 hardware filters in Legacy mode blocked `0x3FD` at the chip level, so the upgrade trigger could never fire. Fixed: wide-open RXB1 for all modes + reprogram RXB0 from `0x3EE`→`0x3FD` after upgrade + mutex-guarded HW version update.
- **Ban Shield + Tier Override double-send** — both could fire on the same `0x7FF` frame, sending two conflicting copies. Now mutually exclusive (shield takes priority).
- **TLSSC bit38 fired without FSD gate** — applied on every mux-0 frame before `fsd_enabled` check. Added gate.
- **ESP32 WiFi password overwrite** — web dashboard sent masked password `***` back to NVS, bricking WiFi AP on restart. Now skips password update if value is masked.
- **Settings menu reorg** — Mode → Stable features → `-- Beta (report!) --` separator → Beta features → Hardware.

## 2.13 — Competitive parity: region unlock, nav FSD, hands-off, lane graph, NVS persistence

- **7 new CAN features**: Nav FSD Route (`0x3F8` bits 13/48/49), Hands-Off UI (`0x3F8` bit14), Dev Mode (`0x3F8` bit5), Force LHD (`0x3F8` bits 40-41), Lane Graph (`0x3FD` mux1 bit45), TLSSC bit38 (`0x3FD` mux0 bit38), Tier Override (`0x7FF` mux2 byte5 bits 4:2). All default OFF with Settings toggles.
- **Energy consumption** (`0x33A` Wh/km) added to BMS dashboard.
- **ESP32 major upgrade** (PR #40 by @dmagyar): NVS persistence for all toggles, WiFi SSID/password config via web dashboard, deep sleep on Lilygo T-CAN485, factory reset (5s button hold), improved NAG echo, WiFi password masked in JSON/Serial.
- **README complete rewrite** — CAN IDs table (13 IDs), expanded FAQ, confirmed compatibility table, ESP32 board comparison.
- Thanks: @dmagyar (ESP32 NVS/WiFi/deep-sleep PR), @THER4iN, @MiniCS, @kp43h8, @gauner1986, @nagotti — platform testing and ban research

## 2.12.1 — Palladium S/X Legacy→HW3 auto-upgrade

- **Palladium Model S/X auto-upgrade** — cars reporting `das_hw=0` (MCU2/HW3 retrofit) were stuck in Legacy mode, silently disabling FSD injection. Now auto-upgrades to HW3 when `0x3FD` frames appear on the bus, reprograms MCP2515 RXB0 filter from `0x3EE`→`0x3FD`, and preserves all user toggles across the upgrade.

## 2.12 — Security hardening + Ban Shield fix

- **RX/TX DLC buffer overflow** — MCP2515 driver trusted raw DLC values up to 15 (4-bit mask). SPI noise or a hostile CAN device could overflow the 8-byte frame buffer. Now clamped to 8.
- **HW4 speed-profile bit offset** — both Flipper and ESP32 wrote speed profile to bits 4-6 of byte 7 on `0x3FD` mux=2. Upstream DBC reference uses bits 5-7. Fixed.
- **OTA false positive** — now only suspends TX when `GTW_updateInProgress` raw value is 2 (installing), not any non-zero. Fixes false "OTA TX paused" on Model X/S and some firmware builds.
- **HW auto-detect passive mode** — detection now uses `MCP_LISTENONLY` (was `MCP_NORMAL` which ACKed live bus frames) and respects user's crystal frequency setting.
- **Ban Shield learning fix** — shield was immediately armed on scene entry since v2.9, skipping learning phase entirely. No healthy snapshots were captured. Fixed: starts unarmed, learns all 8 GTW_carConfig mux frames, then auto-arms.
- **8 additional fixes** from adversarial code review: Track Mode safety gate, ESP32 HW4 misclassification race (50-frame threshold), `0x370` EPAS DLC validation, malloc NULL checks, sizeof(pointer) cleanup, SPI handle init.
- Thanks: @ViPiMP (sizeof BusFault root cause), @dmagyar (Legacy HW + NAG fixes), @Symness (Ban Shield learning approach), @nagotti (OTA false positive testing)

## 2.11 — Legacy HW detection + NAG killer fixes

- **Legacy HW detection fix** — `fsd_detect_hw_version()` fell through to `TeslaHW_Unknown` for `das_hw=0` and `das_hw=1`. MCU2/HW3 retrofit Model S/X reports `das_hw=0`, silently disabling FSD injection for this entire class of vehicle. Now correctly maps to `TeslaHW_Legacy`.
- **NAG killer level 3 fix** — the `hands_on != 0` guard skipped level 3 (escalated alarm), the state where suppression matters most. Changed to `hands_on == 1` so both level 0 (nag imminent) and level 3 (escalated) get echoed.
- **NAG killer echo byte fix** — OR-ing `0x40` without clearing bits 7:6 left level=3 state on escalated frames. Fixed to `(data[4] & ~0xC0u) | 0x40u`.
- **ESP32: Lilygo T-CAN485 board** — new build target with onboard SN65HVD230 transceiver and SD card.
- **ESP32: CAN dump to SD card** — all frames in candump ASCII format + per-session debug.log. Auto-rotation at 1M entries, 15-min auto-stop.
- **ESP32: OTA false positive hardening** — assert 3 frames / clear 6 frames debounce on 0x318.
- **ESP32: fallback HW detection** — when 0x398 is absent, falls back to 0x3EE (Legacy), 0x399 (HW4), or 0x3FD (HW3).

## 2.10 — TLSSC Restore

- **TLSSC Restore (0x331)** — recover Traffic Light and Stop Sign Control on VIN-banned vehicles via DAS config spoofing. Read-modify-retransmit on CAN ID 0x331 at ~1 Hz: overwrites byte[0] lower 6 bits to 0x1B, setting `DAS_autopilot` and `DAS_autopilotBase` to `SELF_DRIVING`. Triggers MCU reboot and restores the TLSSC toggle.
  - **Confirmed working on:** Palladium (Model S Plait 2023), HW4 Highland (Model 3 Performance 2024), Intel HW3 (Model 3, with AP-first workaround).
  - **Known limitation:** Intel HW3 banned cars must activate AP before using TLSSC (enabling the toggle in UI breaks AP on Intel HW3 specifically).
  - **Does NOT restore full FSD** — only TLSSC (stop signs / traffic lights). FSD UI elements (Navigate on AP, Summon, Autopark) remain locked behind server-side Ethernet entitlement.
  - New Settings toggle: "TLSSC Restore" (OFF by default). Also ported to ESP32 web dashboard.
- **Ban research findings** (issue #18):
  - Byte-diff of banned vs unbanned 0x7FF mux=2 confirms tier downgrade (SELF_DRIVING→ENHANCED).
  - 0x3FD mux=0 byte[4] bit 7 is an independent "TLSSC UI visible" flag, cleared on ban.
  - Ban enforcement is platform-specific: Palladium/HW4 less aggressive than Intel HW3.
  - New ban indicator candidate: `0x259 APP_fsdSuspendState` (SUSPENDED=1 on banned car).

## 2.9 — Ban Shield

- **Ban Shield** — a CAN-layer immune system that freezes `GTW_carConfig (0x7FF)` in its healthy state. When Tesla pushes a server-side VIN ban, the Gateway changes specific bits in 0x7FF to disable TLSSC. The Ban Shield detects these changes in real-time and immediately retransmits the healthy snapshot, blocking the ban at the CAN frame level before the AP ECU processes it.
  - **Phase 1 (learning):** Enable "Ban Shield" in Settings. During normal driving, the shield automatically captures all 8 GTW_carConfig mux frames as the "healthy" baseline. No user action needed.
  - **Phase 2 (armed):** Once the baseline is captured, any incoming 0x7FF frame that differs from the snapshot is instantly overwritten and retransmitted with the healthy data. The `gtw_shield_blocks` counter tracks how many frames were blocked.
  - All GTW_carConfig signals are static hardware configuration (dasHw, country, drivetrainType, seatHeaters, autopilot tier, etc.) — they never change during normal driving. A change means either Tesla pushed a ban or a service center modified the config.
  - **Note:** Whether the AP ECU reads 0x7FF from CAN (where our shield works) or from Ethernet (where it doesn't) is still unverified. Community testing needed.

## 2.8 — DAS-aware nag killer + anti-detection

- **DAS-aware nag suppression** — the nag killer now gates on `DAS_autopilotHandsOnState` (from `0x39B`). Only echoes when DAS is actively demanding hands-on (states 2-7, 9-10). States 0 (NOT_REQD) and 8 (SUSPENDED) suppress the echo entirely. Reduces spurious bus traffic from ~25 frames/sec to near-zero during normal AP driving. Ported from ev-open-can-tools PR #5 (@zdenekbouresh).
- **Organic torque variation** — replaces the fixed 1.80 Nm echo with a xorshift32 random walk in [1.00-2.40 Nm] plus brief grip pulses [3.10-3.30 Nm] every 5-9 seconds. A flat torque signal is a telemetry detection vector for VIN-level bans (issue #18).
- **MCP2515 12 MHz crystal support** — Settings → MCP Crystal now has 16 / 8 / 12 MHz. Fixes Waveshare RS485 CAN HAT compatibility. CNF values: CFG1=0x00, CFG2=0xA2, CFG3=0x02 (from arduino-CAN library).
- **MCP2515 8 MHz crystal toggle** — same Settings menu, for generic AliExpress modules.
- **VIN-level ban warning** — README and SECURITY.md now document Tesla's VIN-level FSD bans (confirmed April 2026 by @THER4iN in issue #18). Bans persist across account transfers and re-subscriptions. CAN injection cannot override.
- **MCP2515 SPI NULL crash fix** (v2.7.1 hotfix) — `mcp_alloc()` now properly initializes the SPI bus handle.
- **SPI callback const fix** — compiles on Momentum and Xtreme firmware.
- **fsdcanmod.com badge restored** — community tracker site back online with accurate project tracking.

## 2.7 — Upstream parity + Momentum fix + X179 guide

- **Ported 5 features from upstream** ([ev-open-can-tools](https://github.com/ev-open-can-tools/ev-open-can-tools)):
  - GTW autopilot tier readback (`0x7FF` mux=2): shows the vehicle's actual AP entitlement — NONE/HIGHWAY/ENHANCED/SELF_DRIVING/BASIC. If it reads NONE or BASIC, FSD features won't work regardless of CAN injection.
  - Track Mode inject (`0x313`): sets track mode request ON with checksummed retransmit. Service mode only.
  - Enhanced Autopilot flag: mux=1 now also sets bit 46 when enabled — required for EAP auto lane change and summon on HW3/HW4.
  - HW4 speed offset runtime: mux=2 byte[1] lower 6 bits can be overridden at runtime.
  - Speed profile lock: follow distance stalk no longer overrides the speed profile when locked.
- **Fix: SPI callback const mismatch** — `Spi_lib.c` now compiles on Momentum and Xtreme firmware in addition to official. The `FuriHalSpiBusHandleEventCallback` typedef differs between firmware builds; fixed with a portable cast. Reported by @LeeSSXX in issue #17.
- **HARDWARE.md complete rewrite:**
  - X179 connector is now the recommended connection point (4-wire: CAN-H + CAN-L + 12V + GND).
  - Full X179 20-pin and 26-pin pinout tables with all 4 CAN bus pairs documented.
  - Explains why Pin 13/14 (bus 6) is a Gateway-forwarded mixed bus that carries both Party CAN and Vehicle CAN signals — one connection for nearly all features.
  - Added X052 connector for 2019 Model 3 (pre-facelift): Pin 44/45 CAN + Pin 20/22 12V/GND, confirmed by @THER4iN.
  - Added LILYGO T-2CAN ESP32-S3 (~$24, dual isolated CAN) as recommended future-proof board.
  - All hardware prices corrected from verified official store listings.
  - Deep sleep guidance for permanent vehicle installation.
- **Upstream link updated**: the upstream project moved from GitLab (`slxslx/tesla-open-can-mod-slx-repo`, archiving) to GitHub (`ev-open-can-tools/ev-open-can-tools`, vehicle-agnostic naming).
- **37 total CAN handlers** (14 TX write + 23 RX read-only).

## 2.6 — Full Party CAN coverage

- **32 CAN handlers** (12 TX write + 20 RX read-only), up from 19 in v2.5. Every useful signal on Tesla Model 3/Y Party CAN is now parsed or injectable.
- **New write handlers (Service mode only):**
  - High Beam Strobe — rapid PULL/IDLE toggle on `SCCM_leftStalk (0x249)` at 200ms. Same Party CAN OBD-II tap.
  - Turn Signal Left/Right — inject turn indicator via `SCCM_leftStalk (0x249)` UP_1/DOWN_1.
  - Wiper Wash — inject wiper wash button press via `SCCM_leftStalk (0x249)`.
  - Steering Tune — `GTW_epasTuneRequest (0x101)` COMFORT/STANDARD/SPORT (Chassis CAN tap required).
  - Hazard Lights — `VCFRONT_hazardLightRequest (0x3F5)`.
  - Wiper Off — force `DAS_wiperSpeed (0x3F5)` to 0.
  - Park Inject — `SCCM_parkButtonStatus (0x229)` PRESSED (Vehicle CAN tap required).
- **New read-only parsers (Party CAN, mode-independent):**
  - `DAS_control (0x2B9)`: ACC state (ACC_ON=4), set cruise speed.
  - `DAS_status (0x39B)`: AP hands-on state (4-bit nag), auto lane change, blind spot warning, blind spot avoidance, FCW, vision speed limit.
  - `DAS_status2 (0x389)`: ACC report, AP activation failure reason.
  - `DAS_settings (0x293)`: autosteer enabled readback.
  - `DI_state (0x286)`: cruise state (enabled/standby/standstill), park brake, autopark, digital speed.
  - `DI_torque (0x108)`: motor torque (Nm).
  - `DI_speed (0x257)`: vehicle speed (kph), UI speed.
  - `UI_warning (0x311)`: left/right blinker, any door open, seatbelt, high beam status.
  - `SCCM_steeringAngleSensor (0x129)`: steering wheel angle (deg).
  - `DAS_steeringControl (0x488)`: DAS steering request type + angle.
  - `EPAS3S_currentTuneMode (0x370)`: current steering mode + torsion bar torque.
  - `ESP_driverBrakeApply (0x145)`: brake pedal state.
  - `DI_systemStatus (0x118)`: track mode state, traction control mode.
  - `VCRIGHT_rearDefrostState (0x343)`: rear window defrost.
- **Extras scene expanded** to 10 toggles: Hazard, Rear Window Heat, Auto Wipers Off, Fold Mirrors, Rear Fog, Steering [ChassisCAN], High Beam Strobe, Turn Left, Turn Right.
- **New research docs:**
  - `enhauto-re/FEIFAN_CAN_ANALYSIS.md` — technical analysis of the 非凡指揮官 (Feifan Commander, 69K+ sales in China) CAN injection techniques: continuous AP, stalk simulation, strobe, checksum formulas.
  - `enhauto-re/COMMANDER_VS_TESLAMOD.md` — three-way feature comparison between enhauto S3XY Commander, Feifan Commander, and Tesla Mod.

## 2.5 — Tesla Mod

- **Rebrand: Tesla FSD Unlock → Tesla Mod.** The app name in the Flipper menu changes from "Tesla FSD" to "Tesla Mod". The repo URL stays the same (`hypery11/flipper-tesla-fsd`) for link stability. This reflects the project's evolution from a single-purpose FSD tool to a general Tesla CAN bus toolkit.
- **Extras scene [BETA]** — a new submenu accessible from the main menu with toggles for CAN features beyond FSD: Hazard Lights, Rear Window Heat, Auto Wipers Off, Fold Mirrors, Rear Fog Light. These are marked BETA — the CAN IDs and bit positions come from public sources but need on-vehicle verification. Only active when Mode = Service. Adding a new extra is intentionally cheap (one bool + one toggle + one handler + one dispatch line). PRs welcome — see `CONTRIBUTING.md`.
- **Multi-hardware welcome**: this project now explicitly welcomes ports to any hardware platform. The Flipper Zero version lives in the root, the ESP32 port in `esp32/`. If you want to port to RP2040, STM32, nRF, or anything else with a CAN transceiver, open a PR. See `HARDWARE.md` for the current hardware matrix.
- Housekeeping: removed dead FSD CAN Mod Hub badge, fixed Starmixcraft dead links, filled SECURITY.md takedown chain.
- Added Prerequisites section to README — FSD entitlement is required for FSD features, this is a region-gate bypass not a purchase bypass.

## 2.4

- **Listen-Only is now the first-boot default.** The MCP2515 starts in hardware listen-only mode (physically incapable of TX) and the user must explicitly switch to Active in Settings → Mode. Safer for new users; matches the default of the ESP32 port from PR #6.
- **`HARDWARE.md`** — three-way comparison of supported hardware: Flipper Zero + Electronic Cats CAN Add-On, Flipper Zero + generic MCP2515 module, M5Stack ATOM Lite ESP32 port (PR #6), and Waveshare ESP32-S3-RS485-CAN. Wiring tables and termination-resistor diagnostics for each.
- **README compatibility matrix expanded** with FSD v14 (`2026.2.9.x`) classification, China MIC reports, Highland reports, and a "tested by community" table sourced from issues #1/#2/#7/#9.
- **README termination-resistor section rewritten** — Electronic Cats v0.1 vs v0.2 default differs; documented how to verify with a multimeter without opening the board.
- **`CONTRIBUTING.md`** — what to verify in Listen-Only before opening a PR, code style, branching, what to avoid (AI-generated PR bodies, feature flags as a substitute for safety).
- **`SECURITY.md`** — explicit list of every CAN ID class the TX path can write to, what it pointedly does NOT touch (brakes, steering, ESP, BMS, anything on Chassis CAN), security disclosure email, and a recommended pre-flight checklist. Hardened disclaimer wording.
- **`.github/ISSUE_TEMPLATE/`** — three structured templates: `car_compatibility.yml` (collects HW / firmware / region / mode / result automatically), `bug_report.yml`, `feature_request.yml`. Plus a `config.yml` that links HARDWARE / ROADMAP / SECURITY before issue creation, cutting down on duplicate "what hardware do I need" questions.
- **README "Related projects" section** — links the broader Tesla CAN modding ecosystem (slxslx upstream, ESP32 port PR #6, tumik/S3XY-candump, dzid26 Dorky Commander, original CanFeather, tuncasoftbildik) so users find the right hardware variant without bouncing across forks.

## 2.3

- Live BMS dashboard: parses `0x132` `BMS_hvBusStatus` (pack voltage / current), `0x292` `BMS_socStatus` (state of charge), and `0x312` `BMS_thermalStatus` (battery temp). Once any BMS frame is seen the running screen swaps the feature flag line for live SoC%, instantaneous kW, and battery temp range.
- New "Precondition" toggle in Settings: when ON, periodically injects `UI_tripPlanning` (`0x082` byte[0]=0x05) every 500ms to trigger BMS battery preheat — same trick Tesla uses for Supercharger preconditioning, but you can fire it from anywhere. Goes through the OTA / listen-only TX gate.
- New `enhauto-re/CAN_DICTIONARY.md` references the cross-source Tesla CAN signal dictionary work (mikegapinski 40k signal dump, talas9 wire format, tuncasoftbildik handler templates).

## 2.2

- OTA detection: monitors GTW_carState for `GTW_updateInProgress` and auto-suspends CAN TX when Tesla is pushing a firmware update
- Operation modes: Active / Listen-Only / Service. Listen-Only switches the MCP2515 to its hardware listen-only register (no TX even on bus error frames)
- CRC error counter sampled from MCP2515 EFLG register, surfaced on screen
- TX / RX / Err counters live on the running screen
- Wiring sanity check: shows a clear "no CAN traffic — check wiring" warning after 5s with zero RX
- Background-research notes on the enhauto S3XY Commander — derived from observing the unencrypted signals on its BLE and CAN interfaces — live in `enhauto-re/`

## 2.1

- Nag Killer: CAN 880 counter+1 echo method — spoofs EPAS handsOnLevel to suppress nag at the sensor layer (ported from upstream MR !44)
- New Settings toggle: "Nag Killer" (runtime, no recompile)

## 2.0

- Legacy mode for HW1/HW2 (Model S/X 2016-2019, CAN ID 0x3EE)
- Force FSD toggle — bypass "Traffic Light" UI requirement for unsupported regions
- ISA speed chime suppression (HW4, CAN ID 0x399)
- Emergency vehicle detection flag (HW4, bit59)
- Settings menu with runtime toggles (no recompile needed)
- DLC validation on all frame handlers
- setBit bounds check to prevent buffer overrun
- Firmware compatibility warning for 2026.2.9.x and 2026.8.6

## 1.0

- Initial release
- FSD unlock for HW3 and HW4 (FSD V14)
- Auto-detect hardware version via GTW_carConfig
- Manual HW3/HW4 override
- Nag suppression
- Speed profile control, defaults to fastest
- Live status display
