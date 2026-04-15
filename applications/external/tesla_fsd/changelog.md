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
