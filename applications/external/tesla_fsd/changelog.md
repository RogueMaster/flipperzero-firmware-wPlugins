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
