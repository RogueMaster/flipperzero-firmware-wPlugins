# Roadmap

Derived from `enhauto-re/COMMANDS.md` table 2a — the set of Commander
actions whose CAN frame templates are already documented in public sources
and can be implemented on our existing MCP2515 + Flipper stack without any
new hardware or vendor firmware.

## v2.15 — shipped (first 2026.14.x bypass + HW3 DAS_status fix)

Tagged and released. The whole PR set landed:

- **PR #82** — `0x3C2` Scroll-Press AP Engage (HW4-only, first confirmed 2026.14.x bypass)
- **PR #83** — On-demand grip pulse nag killer enhancement
- **PR #84** — 14.x firmware warning banner (Flipper + ESP32, default ON, dismissible)
- **PR #81** — Ban Shield → GTW Config Replay rename (honest framing, NVS-key preserved, #67)
- **PR #97** — Flipper HW3 `0x399` DAS_status parser fix (mirror of ESP32 #92)
- ESP32 side (vrs11): HW3 `0x399` DAS_status fix + `can_signals.h` refactor (#92), HTTP CAN log stream on port 82 (#94), Ignore OTA toggle (#93)

## v2.16 — shipped (beta line, currently beta.26)

The v2.16 beta line shipped and is on **beta.26**. What started as the
baseline-capture tooling grew into the Field-Readiness initiative (#127)
and a long run of on-car 14.x nag / steer-jerk fixes. Highlights across the
line (see `changelog.md` for the full per-beta history):

- **Shared protocol core + host test suite / CI gate** (beta, beta.2) — one definition of the frame type, checksums, enums and parsers shared by the Flipper and ESP32 builds, gating both.
- **Capture tooling** — Flipper CAN Capture (beta), full-rate single-ID capture (beta.3), user-loadable `.cantest` SEND profiles (beta.2), ESP32 STA WiFi + dashboard config (beta.2).
- **Field-Readiness (#127)** — black-box incident recorder (#124), tap capability checker (#125), `0x39B`/`0x399` variant auto-profiles (#126), from beta.12 on.
- **14.x nag / steer-jerk work** — ESP32 AP-First (beta.6), EPAS-faithful / Mode-C nag (beta.7/8/10), Nag Burst + ±1.8 Nm cap + Signal Map (beta.11), Abort Guard (beta.11), Soft Engage (beta.10), Instant Engage / Minimal Inject (beta.16/17/19), plus HW4-detect and dashboard fixes (beta.20–24).
- **`0x229 SCCM_rightStalk` — resolved (#95).** The AUTOSAR-E2E checksum was cracked and documented (`tools/crack_0x229.py`, 224/224 real frames across two full-rate captures, beta.4). Outcome: injection is a dead end on a shared bus — the genuine SCCM never stops sending `0x229`, so an injected pull collides bit-for-bit and breaks the rolling-counter sequence (this is why `0x3C2` ScrollPress works and `0x229` doesn't). `0x229` is therefore blocked from loadable `.cantest` profiles for safety (a pulled-down stalk is a shift-to-DRIVE request the parked/stationary interlock can't catch). Credit @DmitroPanteliuk (full-rate captures), @se7en7777777, @jewelrylin.
- **LILYGO T-2CAN dual-CAN — shipped (#96).** `lilygo-t2can` is a live PlatformIO env (onboard MCP2515/SPI as Vehicle CAN + native TWAI), carrying Bus 6 plus Vehicle CAN Bus 2 direct on one board. T-2CAN firmware / bus / wiring reference merged into `esp32/README.md` via #137 (beta.25). Credit @ssw0209-sys.
- **EU / AP feature toggles (beta.25, all opt-in, default OFF)** — Summon EU Unlock (`0x3FD` mux1, clears bit19 + sets bit47, closing the HW3 gap; #111/#139, PR #144); Continue on Green (`0x3FD` mux0 bit39; PR #145); Right-Hand Drive override (`0x3F8` bit41; #66, PR #146); Telemetry Off (experimental — clears reachable telemetry-enable flags, not a ban guarantee; PR #147); AP branch/tier selector (`UI_apmv3Branch`, experimental, non-persistent; PR #148).
- **Adjustable Track Mode (beta.26, PR #150)** — `0x313 UI_trackModeSettings`: Track Mode ON + Handling Balance (byte1) + Stability Assist (byte2) + post-drive cooling (byte3), additive checksum recomputed and counter preserved. Opt-in, default OFF, works on non-Performance trims. (See Tier 2 / Tier 3 below.)

### Now / next (post beta.26)

Genuinely-open, contributions welcome:

- **HW3 `0x3C2` retest with current code** — @DmitroPanteliuk's earlier HW3 negative test (emergency brake on 2026.14.6) may have been caused by the `0x399`-vs-`0x39B` DAS readback failure since fixed in #92. Retest with current ESP32 code before deciding whether to expose `0x3C2` on HW3.
- **L2 nag trigger investigation** — @deftdawg flagged that residual 2-second yellow nags still appear on the on-demand grip pulse path. L2 (transitional / "marginal hands") may be the missing trigger. Needs CAN capture of L1→L2 transitions on a banned car before deciding to add to the trigger set — acting on L1 directly is a fingerprint risk.
- **OpenWRT spoofing AP** — @vadimpelau raised the question of DNS-spoofing Tesla domains to keep maps/multimedia alive while reducing ban risk. Marginal improvement for ban prevention given Tesla's mutual-TLS-pinning on telemetry paths, but useful for UX. If anyone has a working OpenWRT writeup that handles cert pinning, drop in [#80](https://github.com/hypery11/flipper-tesla-fsd/issues/80).

## Already shipped (v2.3.0)

- FSD unlock HW3/HW4/Legacy (`0x3FD` / `0x3EE`)
- Force FSD (bypass Traffic Light requirement)
- Speed profile control + fastest default
- Nag killer (EPAS `0x370` counter+1 echo)
- ISA speed chime suppression (`0x399`)
- Emergency vehicle flag (HW4 bit59)
- OTA detection + auto TX pause (monitor `0x318`)
- Operation modes: Active / Listen-Only / Service
- CRC / TX / RX counters + wiring warning
- Battery preconditioning (`0x082` byte[0]=0x05 @ 500ms)
- Live BMS dashboard (`0x132` / `0x292` / `0x312`)

## Tier 1 — trivial ports (one handler each, shared infra already in place)

Each of these is structurally identical to what we already do for the FSD
frame: read an incoming frame by CAN ID, flip a bit or overwrite a byte,
retransmit. Estimated ~30 LOC each.

- [ ] `HazardLights` toggle via `0x273 VCFRONT_lighting`
- [ ] `FrontFogLights` / `RearFogLights` / `AllFogLights` (`0x273` bits)
- [ ] `HighBeamStrobe` (`0x273` bit, timed burst)
- [ ] `RearDRL` (`0x273` bit)
- [ ] `ForceManualHighBeam` (`0x273`)
- [ ] `AutoHighBeamOff` (`0x273` disable bit)
- [ ] `MuteSpeedLimitWarning` (block `0x399` more aggressively than current chime suppression)
- [ ] `AutoWipersOff` / `KeepWipersOff` (`0x3E2 VCFRONT_wipers`)
- [ ] `BackWindowHeater` toggle (`0x3B3 HVAC_status`)
- [ ] `RearVentAlwaysOn` / `RearVentAlwaysOff` (`0x2E1 VCRIGHT_hvacStatus`)
- [ ] `Recirc` toggle (`0x2E5 HVAC_Command`)

## Tier 2 — stateful handlers (need small state machine)

- [ ] `FollowDistance` full mapping to speed profile (partial today, expose as first-class toggle)
- [ ] `GearShift` / `SimulatePARK` via `0x229 SCCM_rightStalk` — needs stalk emulation
- [ ] `ChargePort` open/close via `0x102 VCLEFT_chargingHandleStatus`
- [ ] `FoldMirrors` / `MirrorsDip` / `MirrorsDim` (`0x273` bits) — read current state, toggle
- [ ] `StoppingMode` select (`0x293`)
- [ ] `TractionControl` off (`0x2A1 ESP_status`)
- [x] `TrackMode` enter/exit — shipped via `0x313 UI_trackModeSettings` (beta.26, PR #150; the real frame, not the guessed `0x293`/`0x2B9`)
- [ ] `WiperMode` cycle / `WipersWasher` pulse (`0x3E2`)
- [ ] **Cybertruck Homelink bridge** — requested by @JoshuaSpain on [slxslx/tesla-open-can-mod-slx-repo#2](https://gitlab.com/slxslx/tesla-open-can-mod-slx-repo/-/issues/2). Tesla removed the Homelink module from Cybertruck and replaced it with a MyQ subscription. The CT's firmware almost certainly still runs the Homelink code path — Tesla is known to leave code for removed hardware (rain sensor, ultrasonic, etc.) — so the car is plausibly sending a "Homelink requested" frame every time the UI button is pressed, it just has no physical module to act on it.
  
  **Approach (credit @JoshuaSpain for the direction):** instead of emitting RF from a Flipper sub-GHz radio, sniff the frame the car sends when the Homelink button is pressed, translate it to an HTTP webhook / MQTT publish, and let Home Assistant + [RATGDO](https://github.com/paul-wieland/ratgdo) (or any other HA-compatible garage opener) do the actual door-opening. No RF rolling-code reproduction, no hardware emitter, no subscription.
  
  **Bus layer:** likely LIN, not CAN. Homelink on factory-equipped Teslas sits as a LIN slave off the front body controller (VCFRONT on Model 3/Y, BCM-equivalent on S/X), which matches how Tesla wires other low-speed body actuators. There may also be a shadow copy of the "Homelink requested" frame on body CAN — if so, the Flipper + MCP2515 rig already in this project can read it directly; otherwise a Flipper-compatible LIN transceiver is needed.
  
  **Blocked on:** a Model Y LIN-bus capture around a working Homelink button press. @JoshuaSpain owns a Model Y with the factory module and has offered to do the dump. Waiting on the raw capture (candump / savvycan / csv) before writing the handler — once we have the frame ID and payload, the listener is ~20 lines.
  
  **Implementation skeleton once unblocked:**
  1. Add a new CAN (and/or LIN) ID constant for the Homelink-request frame
  2. Register a read-only handler in the dispatch loop that matches the frame, optionally checks a payload flag, and fires a webhook to a configurable URL (WiFi-enabled ESP32 port or external companion)
  3. Add a Settings toggle for "Homelink bridge" with a URL text entry
  4. Document the Home Assistant automation receiver in `HARDWARE.md` (`service: rest_command.ratgdo_door_open` pattern)
  
  **Nice-to-have:** also sniff the frame on an actual Cybertruck to confirm the CT still emits it, rather than assuming from Tesla's firmware patterns.

## Tier 3 — needs read-parse-decide (already have BMS pattern to copy)

- [ ] `DriveModeAccel` / `DriveModeRegen` / `DriveModeSteering` (parse `0x118 DI_vehicleStatus`, write `0x293`)
- [ ] `HVAC_Blower` / `HVAC_On` / `HVAC_DefogDefrost` (`0x2E5`) — need current HVAC state
- [ ] `CabinTemp` / `CabinTempLeft` / `CabinTempRight` — current temp read, setpoint write
- [ ] Seat heat family: `FrontSeatHeatLeft/Right`, `RearSeatHeatLeft/Right/Central/All`, `AllSeatHeat`, `SteeringWheelHeat` (`0x2E1` bitfield)
- [ ] `FrontSeatVentLeft` / `FrontSeatVentRight` (`0x2E1`)
- [x] `TrackModeStability` / `TrackModeHandling` sliders — shipped via `0x313` Stability Assist (byte2) + Handling Balance (byte1) (beta.26, PR #150; the real frame, not `0x2B9`)

## Tier 4 — multi-frame / safety-gated

- [ ] `Suspension` / `RideHandling` via `0x204 VCFRONT_airSuspension` — guard against run-while-driving
- [ ] Window family: `VentWindows`, `VentLeft`, `VentRight`, `FrontLeftWindow`, `FrontRightWindow`, `RearLeftWindow`, `RearRightWindow` (`0x3E3`)
- [ ] `MediaControl` / `Sounds` / `PlaidLightsControl` / `LightStripBrightness` (`0x2F1 UI_audioStatus` + `0x264` + `0x273`)
- [ ] `DynamicBrakeLights` (`0x273`)

## Out of scope (until we get a real Commander to sniff)

Table 2b in `enhauto-re/COMMANDS.md`. These need byte-level templates that
only exist in Commander firmware: `DisableMotor`, `ParkBrake`,
`DriverMoveSeat` / `PassengerMoveSeat`, seat profile save/restore,
`PresentingDoors*`, `PresentingTrunk*`, `HandWash`, `SuspensionByGear`,
`AutoWindowDrop`, `CameraOnFoldMirrors`, `Grok`, `KickdownSport`,
`RainbowMode*`, `SplitLedStrip`, etc.

## Out of scope (Tesla Fleet HTTP API, not CAN)

Table 1 in `enhauto-re/COMMANDS.md`. These are proper Tesla Fleet API
commands (Frunk/Trunk, DoorLock/Unlock, HonkHorn, FlashLights, Bioweapon,
DogMode, CampMode, KeepClimateOn, SetTemps, ChargeOpen/Close,
GarageDoorOpenClose, SpeedLimit, Wake, RemoteBoombox, etc.). They belong in
a companion web console, not on the Flipper — the Flipper core has no
native WiFi.

## Shape of the work

All Tier 1 and most of Tier 2 can land as a single PR that introduces a
generic **"Extras" scene** — a scrollable list of one-shot toggles that
share the existing MCP2515 / OpMode / OTA-pause gating infrastructure.
Adding a new toggle becomes: drop a handler in `fsd_logic/extras/`, add an
entry to the scene list. That's a cheap architectural change that unlocks
~20 of the items above in one go.

Tier 3 needs a small "vehicle state cache" so we can show current values
(HVAC temps, seat heat levels) before a user flips them. This is an
extension of the BMS pattern we already have — same `state->*_seen` flag
pattern, same mutex discipline.

Tier 4 should be gated behind an explicit safety confirm prompt because
they affect vehicle dynamics or exterior lighting.
