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
