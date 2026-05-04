[English](README.md) | [繁體中文](README_zh-TW.md) | [简体中文](README_zh-CN.md)

# Tesla Mod for Flipper Zero

[![GitHub stars](https://img.shields.io/github/stars/hypery11/flipper-tesla-fsd?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/hypery11/flipper-tesla-fsd?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/network)
[![GitHub release](https://img.shields.io/github/v/release/hypery11/flipper-tesla-fsd?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/releases)
[![Downloads](https://img.shields.io/github/downloads/hypery11/flipper-tesla-fsd/total?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/releases)
[![Last commit](https://img.shields.io/github/last-commit/hypery11/flipper-tesla-fsd?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/commits/main)
[![Open issues](https://img.shields.io/github/issues/hypery11/flipper-tesla-fsd?style=flat-square&logo=github)](https://github.com/hypery11/flipper-tesla-fsd/issues)
[![PRs welcome](https://img.shields.io/badge/PRs-welcome-brightgreen?style=flat-square)](CONTRIBUTING.md)
[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-blue?style=flat-square)](LICENSE)
[![Build](https://img.shields.io/badge/build-ufbt-brightgreen?style=flat-square)](https://github.com/flipperdevices/flipperzero-ufbt)
[![Flipper target](https://img.shields.io/badge/Flipper%20target-7%20%2F%20API%2087.1-orange?style=flat-square)](https://github.com/flipperdevices/flipperzero-firmware)
[![Tracked on FSD CAN Mod Hub](https://img.shields.io/badge/tracked%20on-FSD%20CAN%20Mod%20Hub-orange?style=flat-square)](https://fsdcanmod.com/project/hypery11-flipper-zero)

> **Open-source Tesla CAN bus toolkit for Flipper Zero and ESP32.** FSD region-gate bypass, TLSSC Restore for VIN-banned cars, nag killer with organic torque variation, Ban Shield, live BMS dashboard, and 30+ CAN handlers across Model 3, Model Y, Model S, and Model X. Supports HW3, HW4, and Legacy HW1/HW2. Free alternative to the $200+ S3XY Commander — total cost from **$14** with the [ESP32 port](https://github.com/hypery11/flipper-tesla-fsd/tree/main/esp32).

> [!IMPORTANT]
> **An active FSD package is required for FSD features** — either purchased or subscribed. This tool enables FSD functionality at the CAN bus level, but the vehicle still needs a valid FSD entitlement from Tesla. Non-FSD features (nag killer, BMS dashboard, diagnostics) work without any subscription.

> [!CAUTION]
> **Tesla has begun issuing VIN-level bans** (April 2026). Affected vehicles lose the TLSSC toggle silently — no OTA, no warning, persists across account transfers and re-subscriptions. The **TLSSC Restore** feature (v2.10+) can recover stop sign / traffic light control on banned Palladium and HW4 cars via 0x331 DAS config spoofing. See [SECURITY.md](SECURITY.md) and [issue #18](https://github.com/hypery11/flipper-tesla-fsd/issues/18) for the full ban research.

<p align="center">
  <img src="assets/demo.gif" alt="Tesla FSD unlock running on Flipper Zero — main menu, HW detect, and live BMS dashboard" width="600">
</p>

<p align="center">
  <img src="screenshots/main_menu.png" alt="Flipper Zero Tesla FSD main menu" width="256">&nbsp;&nbsp;&nbsp;
  <img src="screenshots/fsd_running.png" alt="Tesla FSD unlock running on Flipper Zero" width="256">
</p>

<p align="center">
  <a href="https://star-history.com/#hypery11/flipper-tesla-fsd&Date">
    <img src="https://api.star-history.com/svg?repos=hypery11/flipper-tesla-fsd&type=Date" alt="Star history" width="600">
  </a>
</p>

<p align="center">
  <a href="https://github.com/hypery11/flipper-tesla-fsd/graphs/contributors">
    <img src="https://contrib.rocks/image?repo=hypery11/flipper-tesla-fsd" alt="Contributors">
  </a>
</p>

---

## Features

### Core FSD
- Auto-detect HW3/HW4 from `GTW_carConfig` (`0x398`), with fallback detection via `0x3FD`/`0x399`/`0x3EE` when `0x398` isn't on the tapped bus
- **Legacy→HW3 auto-upgrade** for Palladium Model S/X — detects `das_hw=0` then upgrades when `0x3FD` appears on the bus
- FSD unlock via bit manipulation on `UI_autopilotControl` (`0x3FD` / `0x3EE`)
- **Legacy mode** for HW1/HW2 (Model S/X 2016-2019)
- Speed profile defaults to fastest, syncs from follow-distance stalk

### TLSSC Restore (v2.10+)
- Recover Traffic Light and Stop Sign Control on **VIN-banned** vehicles
- Read-modify-retransmit on CAN ID `0x331` — sets `DAS_autopilot` to SELF_DRIVING
- Confirmed working on Palladium (Model S Plaid 2023), HW4 Highland (Model 3 Performance 2024), and Intel HW3 (with AP-first workaround)
- Does NOT restore full FSD visualization — only TLSSC (stop signs / traffic lights)

### Ban Shield (v2.9+)
- Freezes `GTW_carConfig` (`0x7FF`) in its healthy state
- Learns all 8 mux frames on first run, then auto-arms
- Any server-side ban push is blocked at the CAN frame level in real-time

### Nag Killer (v2.1+)
- DAS-aware gating — only echoes when DAS is actually demanding hands-on, zero bus traffic when DAS is satisfied
- Organic torque variation — xorshift32 PRNG random walk in 1.00-2.40 Nm with grip pulse excursions to 3.10-3.30 Nm every 5-9 seconds
- EPAS counter+1 echo on `0x370` with level 0 (nag imminent) and level 3 (escalated alarm) suppression

### AP-First mode (v2.14+, for 2026.14.x firmware)
- Tesla 2026.14.x added a preflight check that blocks AP/TACC engagement if CAN injection is already active
- When **AP-First** is enabled, the app monitors `DAS_autopilotState` from `0x39B` and only starts injecting `0x3FD` after AP is engaged
- Nag killer, TLSSC Restore, and Ban Shield are unaffected (they target different CAN IDs)

### Diagnostics (read-only, no FSD required)
- Live BMS dashboard: pack voltage, current, SoC, temperature range, **energy consumption (Wh/km)**
- Vehicle speed, steering angle, motor torque, brake state
- DAS status: autopilot state, hands-on nag level, lane change state, blind spot warning, FCW, vision speed limit
- GTW autopilot tier readback (NONE/HIGHWAY/ENHANCED/SELF_DRIVING/BASIC)
- OTA detection with debounce — auto-suspends TX during firmware updates

### Settings (runtime toggles)

**Stable (car-tested):**

| Setting | Description |
|---------|-------------|
| **Mode** | `Active` / `Listen-Only` / `Service`. Listen-Only is the **first-boot default** — MCP2515 is in hardware listen-only mode and physically cannot TX. |
| **Nag Killer** | DAS-aware EPAS counter+1 echo with organic torque variation. |
| **Force FSD** | Bypass the `isFSDSelectedInUI` check. Does not bypass Tesla's server-side entitlement — only affects local CAN frame flow. |
| **TLSSC Restore** | 0x331 DAS config spoof to recover TLSSC on banned vehicles. Triggers MCU reboot. |
| **AP-First (14.x)** | Delay 0x3FD injection until AP is engaged. Required for Tesla firmware 2026.14.x. |
| **Ban Shield** | Freeze `GTW_carConfig` (0x7FF) to block server-side VIN bans. Auto-learns healthy state, then arms. |
| **Suppress Chime** | Kill the ISA speed warning chime (HW4 only, `0x399`). |
| **Emerg. Vehicle** | Enable emergency vehicle detection flag (HW4 only, bit59). |
| **Precondition** | Battery preheat trigger via `0x082`. |

**Beta (untested, please report results):**

| Setting | CAN ID | Description |
|---------|--------|-------------|
| **Nav FSD Route** | `0x3F8` bits 13/48/49 | Enable nav-based FSD routing (EU/restricted regions) |
| **TLSSC bit38** | `0x3FD` mux0 bit38 | Explicit TLSSC enable, complementary to 0x331 |
| **Lane Graph** | `0x3FD` mux1 bit45 | UI_showLaneGraph — lane visualization on non-FSD tier |
| **Tier Override** | `0x7FF` mux=2 | Force GTW_autopilot to SELF_DRIVING (more aggressive than Ban Shield) |
| **Dev Mode** | `0x3F8` bit5 | UI_dasDeveloper flag |
| **Force LHD** | `0x3F8` bits 40-41 | Override driving side for RHD markets |
| **Hands-Off** | `0x3F8` bit14 | UI-level hands-on disable (second nag vector) |
| **Telemetry Off** | `0x3F8` bit43 | Disable trip telemetry — may itself be a ban signal, use only with SIM pulled |

**Hardware:**

| Setting | Description |
|---------|-------------|
| **MCP Crystal** | 16 / 8 / 12 MHz — match your CAN module's crystal frequency. |

### HW Support

| Tesla HW | Bits Modified | Speed Profile |
|----------|---------------|---------------|
| Legacy (HW1/HW2) | bit46 | 3 levels (0-2) |
| HW3 | bit46 | 3 levels (0-2) |
| HW4 (FSD V14+) | bit46 + bit60, bit47 | 5 levels (0-4) |

---

## Hardware

### Flipper Zero

| Component | Description | Price |
|-----------|-------------|-------|
| [Flipper Zero](https://flipper.net/) | The multi-tool device | ~$170 |
| [Electronic Cats CAN Bus Add-On](https://electroniccats.com/store/flipper-addon-canbus/) | MCP2515-based CAN transceiver (v1.2 supported) | ~$30 |
| OBD-II cable or X179 pigtail | Connect to Tesla's CAN bus | ~$5-10 |

### ESP32 (from $14)

Full-featured ESP32 port with WiFi web dashboard, NVS settings persistence, deep sleep, and factory reset. Same CAN logic as the Flipper app.

| Board | Cost | Build target |
|-------|------|-------------|
| M5Stack ATOM Lite + ATOMIC CAN | ~$14 | `m5stack-atom` |
| Lilygo T-CAN485 | ~$15 | `esp32-lilygo` |
| Waveshare ESP32-S3-RS485-CAN | ~$18 | `waveshare-s3-can` |
| Generic ESP32 + MCP2515 | ~$6 | `esp32-mcp2515` |

See [`esp32/README.md`](https://github.com/hypery11/flipper-tesla-fsd/tree/main/esp32) for setup, and [`HARDWARE.md`](HARDWARE.md) for the full comparison + wiring diagrams + X179 pinouts.

### Connection points

- **OBD-II** (under steering column) — Party CAN. May go silent in Drive on some Model 3/Y builds.
- **X179** (behind passenger kick panel) — recommended. Pin 13/14 = Bus 6 (mixed forwarding, stays active in all modes). See [`HARDWARE.md`](HARDWARE.md) for 20-pin and 26-pin pinouts.

<p align="center">
  <img src="images/wiring_diagram.png" alt="Wiring Diagram" width="700">
</p>

---

## Installation

### Option 1: Download Pre-built FAP

1. Go to [Releases](https://github.com/hypery11/flipper-tesla-fsd/releases)
2. Download `tesla_mod.fap`
3. Copy to your Flipper's SD card: `SD Card/apps/GPIO/tesla_mod.fap`

### Option 2: Build from Source

```bash
git clone https://github.com/hypery11/flipper-tesla-fsd.git
cd flipper-tesla-fsd
ufbt
# Output: dist/tesla_mod.fap
```

### ESP32

```bash
git clone https://github.com/hypery11/flipper-tesla-fsd.git
cd flipper-tesla-fsd/esp32
pio run -e m5stack-atom    # or: esp32-lilygo, waveshare-s3-can, esp32-mcp2515
```

---

## Usage

1. Plug the CAN Add-On into your Flipper Zero (or flash the ESP32)
2. Connect CAN-H/CAN-L to the vehicle via OBD-II or X179 pin 13/14
3. Open the app: `Apps > GPIO > Tesla Mod`
4. Select **"Auto Detect & Start"** (or Force HW3/HW4)
5. Wait for detection (up to 8 seconds) — Palladium S/X will auto-upgrade from Legacy to HW3
6. The app starts modifying frames automatically when the TLSSC toggle is enabled in the car

---

## Compatibility

### Confirmed working (community-tested)

| Vehicle | HW | Firmware | Tester | Feature |
|---------|----|----------|--------|---------|
| Model S Plaid 2023 (Palladium) | HW3/MCU3 | 2026.2.9.3 | @MiniCS, @nagotti | TLSSC Restore, FSD |
| Model 3 Highland Perf 2024 | HW4 | 2026.8.6 | @kp43h8 | TLSSC Restore, persists after disconnect |
| Model 3 2019-2023 | HW3 | Various | @THER4iN, multiple | FSD, nag killer |
| Model X Raven 2017 (HW3 retrofit) | HW3/MCU2 | 2026.8.3 | @dmagyar | Nag killer, EAP |
| Model Y 2023 (China, MIC) | HW3 | 2026.2.11 | Community | FSD (Force FSD mode) |
| Model 3/Y 2023+ | HW4 | < 2026.2.9 | @vbarrier, @kwangseok73-sudo | FSD |

### Known limitations

| Firmware | Issue | Workaround |
|----------|-------|------------|
| 2026.8.6+ | Region lock — FSD neural net refuses to run in some regions | Pull SIM, use Force FSD |
| 2026.8.6 HW4 | HW4 injection path broken on this specific build | Use Force HW3 mode |
| Intel HW3 (banned) | TLSSC toggle restored via 0x331, but enabling it breaks AP | Activate AP first, then inject TLSSC via 0x3FD |

File your own test report via the [Car compatibility report](https://github.com/hypery11/flipper-tesla-fsd/issues/new?template=car_compatibility.yml) template.

---

## How It Works

Single-bus read-modify-retransmit on Party CAN. No MITM, no second bus tap.

1. Gateway/ECU sends a frame on the CAN bus
2. Flipper/ESP32 catches it, modifies the target bits
3. Retransmits — the receiver uses the latest frame

### CAN IDs

| CAN ID | Name | Direction | Role |
|--------|------|-----------|------|
| `0x331` | `DAS_autopilotConfig` | TX | TLSSC Restore — set tier to SELF_DRIVING |
| `0x370` | `EPAS3P_sysStatus` | TX | Nag killer — counter+1 echo with organic torque |
| `0x399` | `ISA_speedLimit` | TX | Speed chime suppression (HW4) |
| `0x3FD` | `UI_autopilotControl` | TX | FSD unlock — bit46/60 (HW3/HW4), TLSSC bit38, lane graph bit45 |
| `0x3F8` | `UI_driverAssistControl` | TX | Nav FSD route, hands-off, dev mode, LHD, telemetry (beta) |
| `0x3EE` | `UI_autopilotControl` | TX | FSD unlock — Legacy HW1/HW2 |
| `0x7FF` | `GTW_carConfig` | TX | Ban Shield freeze + active tier override |
| `0x082` | `UI_tripPlanning` | TX | Battery preconditioning trigger |
| `0x398` | `GTW_carConfig` | RX | HW version detection |
| `0x318` | `GTW_carState` | RX | OTA detection (auto-suspend TX) |
| `0x39B` | `DAS_status` | RX | AP state (for AP-First), nag level, lane change, blind spot |
| `0x132` | `BMS_hvBusStatus` | RX | Pack voltage / current |
| `0x292` | `BMS_socStatus` | RX | State of charge |
| `0x312` | `BMS_thermalStatus` | RX | Battery temperature |
| `0x33A` | `UI_ratedConsumption` | RX | Energy consumption (Wh/km) |

Full list of 37 handlers (14 TX, 23 RX) in [`fsd_logic/fsd_handler.h`](fsd_logic/fsd_handler.h).

---

## FAQ

**Does FSD stay unlocked after I unplug?**
No. Real-time frame modification. Unplug = back to stock.

**Can this work without an FSD subscription?**
FSD features (TLSSC, traffic light/stop sign control) require the FSD entitlement from Tesla. Without it, the AP ECU has no neural network weights loaded. Non-FSD features (nag killer, BMS dashboard, speed chime suppress, diagnostics) work on any AP-capable car.

**What about VIN-level bans?**
Tesla has been banning VINs server-side since April 2026. The ban downgrades `GTW_autopilot` tier from SELF_DRIVING to ENHANCED and removes the TLSSC toggle. The **TLSSC Restore** feature (0x331) can recover stop sign/traffic light control on Palladium and HW4. See [issue #18](https://github.com/hypery11/flipper-tesla-fsd/issues/18) for the full research. The **Ban Shield** (0x7FF) can block ban pushes if the AP ECU reads 0x7FF from CAN.

**Flipper Zero vs ESP32 — which should I get?**
ESP32 is cheaper ($14 vs $200+), has WiFi dashboard, NVS persistence, and deep sleep. Flipper is more portable and has a built-in screen. Both run the same CAN logic. If you don't already own a Flipper, get the ESP32.

**Does this support Model S / Model X?**
Yes. Palladium S/X (2021+) is confirmed working with TLSSC Restore. Pre-2021 S/X with HW3 retrofit works via Legacy→HW3 auto-upgrade. HW1/HW2 Model S/X uses Legacy mode (`0x3EE`). Model S/X uses different BMS CAN IDs — BMS dashboard may show incorrect values.

**Can this brick my car?**
Only UI config frames are touched. No writes to brakes, steering, or powertrain. The app boots in Listen-Only mode by default. See [SECURITY.md](SECURITY.md) for the full TX surface list.

**Do I need a Flipper CAN Add-On?**
For the Flipper: yes, any MCP2515-based module (Electronic Cats, generic boards). For ESP32: built-in CAN transceiver on most supported boards (M5Stack ATOMIC CAN, Lilygo T-CAN485, Waveshare S3).

---

## Related projects

| Project | What it is | Hardware |
|---------|------------|----------|
| [ev-open-can-tools](https://github.com/ev-open-can-tools/ev-open-can-tools) | The upstream community project, now on GitHub. Formerly `Tesla-OPEN-CAN-MOD` on GitLab (group removed April 2026). | RP2040 CAN, Feather M4, ESP32 |
| [dzid26/ESP32-DualCAN](https://github.com/dzid26/ESP32-DualCAN) | "Dorky Commander" — open-source hardware alternative to the S3XY Commander | ESP32 + dual CAN |
| [tuncasoftbildik/tesla-can-mod](https://github.com/tuncasoftbildik/tesla-can-mod) | Arduino reference implementation with frame templates | Arduino + MCP2515 |
| [tumik/S3XY-candump](https://github.com/tumik/S3XY-candump) | Python CAN dump tool via S3XY Commander (Panda protocol) | Commander dongle |

## Credits

- [commaai/opendbc](https://github.com/commaai/opendbc) — Tesla CAN signal database
- [ElectronicCats/flipper-MCP2515-CANBUS](https://github.com/ElectronicCats/flipper-MCP2515-CANBUS) — MCP2515 driver for Flipper
- Community contributors: @THER4iN, @MiniCS, @kp43h8, @gauner1986, @dmagyar, @ViPiMP, @marcobellinoroci-source, @danpadure, @bruvv, @Symness, @hkloudou, @nagotti, @patatman, @JordanzhaoD — ban research, platform testing, ESP32 improvements, bug fixes
- `Starmixcraft/tesla-fsd-can-mod` — original CanFeather FSD research (GitLab repo removed; mirror at [Karolynaz/waymo-fsd-can-mod](https://github.com/Karolynaz/waymo-fsd-can-mod))

## Support the research

If this project saved you money on an aftermarket dongle, helped you understand Tesla's CAN bus, or kept your TLSSC working through a ban, consider supporting the ongoing research and testing.

[![PayPal](https://img.shields.io/badge/PayPal-Donate-00457C?style=for-the-badge&logo=paypal&logoColor=white)](https://www.paypal.com/cgi-bin/webscr?cmd=_xclick&business=hypery11@gmail.com&item_name=Tesla+FSD+Open+Source+Research&currency_code=USD) [![GitHub Sponsors](https://img.shields.io/badge/Sponsor-hypery11-EA4AAA?style=for-the-badge&logo=github&logoColor=white)](https://github.com/sponsors/hypery11)

Funds go toward Tesla parts for testing (banned VINs to recover, different MCU/HW combos), ESP32 hardware variants, and time spent reverse-engineering new firmware versions.

## License

GPL-3.0

## Disclaimer

Educational and research use only. **FSD is a premium Tesla feature and must be properly purchased or subscribed to.** Modifying vehicle systems may void your warranty and may violate local laws. You are solely responsible for what you do with this. Full safety and responsible-use notes in [`SECURITY.md`](SECURITY.md).
