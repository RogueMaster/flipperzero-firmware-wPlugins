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

> **Tesla FSD region-gate bypass for Flipper Zero.** Enables the FSD UI toggle for users who **already have an active FSD subscription or purchase** but are in a region where the "Traffic Light and Stop Sign Control" option is not exposed in the vehicle menu. HW3, HW4, Legacy HW1/HW2. FSD v14 ready. Also includes nag killer, ISA speed chime suppression, OTA guard, battery preconditioning trigger, and a live BMS dashboard — these work independently of FSD entitlement. Total hardware cost: Flipper Zero + Electronic Cats CAN Bus Add-On + OBD-II cable — or build the [$14 ESP32 port](https://github.com/hypery11/flipper-tesla-fsd/tree/main/esp32) instead.

> [!IMPORTANT]
> **An active FSD package is required for FSD features** — either purchased or subscribed. This tool enables the FSD functionality at the CAN bus level, but the vehicle still needs a valid FSD entitlement from Tesla. It is NOT a purchase bypass.

> [!CAUTION]
> **Tesla has begun issuing VIN-level bans** (April 2026). Affected vehicles lose the TLSSC toggle silently — no OTA, no warning, persists across account transfers and re-subscriptions. CAN injection cannot override a VIN ban. See [SECURITY.md](SECURITY.md) and [issue #18](https://github.com/hypery11/flipper-tesla-fsd/issues/18) for details. Non-FSD features (nag killer, BMS dashboard, diagnostics) are not affected.
>
> If FSD subscriptions are not available in your region, the upstream community documents a workaround: create a Tesla account in a region where FSD subscriptions are offered (e.g. Canada), transfer the vehicle to that account, and subscribe. See the [upstream documentation](https://gitlab.com/slxslx/tesla-open-can-mod-slx-repo) for details.
>
> Features like the Nag Killer, ISA Speed Chime Suppression, BMS Dashboard, and battery preconditioning work **without** FSD and do not require any subscription.

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

- Auto-detect HW3/HW4 from `GTW_carConfig` (`0x398` on legacy / `0x7FF` on Ethernet), or force manually — **note:** on 2020+ Model 3/Y HW3/HW4, `0x398` is on the Ethernet bus and may not appear on the CAN bus tap; use Force HW3 or Force HW4 if auto-detect doesn't find it
- **Legacy mode** for HW1/HW2 (Model S/X 2016-2019)
- FSD unlock via bit manipulation on `UI_autopilotControl` (`0x3FD` / `0x3EE`)
- Nag suppression (hands-on-wheel reminder)
- Speed profile defaults to fastest, syncs from follow-distance stalk
- Live status on Flipper screen

### Settings (runtime toggles)

| Setting | Description |
|---------|-------------|
| **Mode** | `Active` / `Listen-Only` / `Service`. Listen-Only is the **first-boot default** as of v2.4 — the MCP2515 is put into hardware listen-only mode and physically cannot TX. Switch to Active when you're ready. |
| **Force FSD** | Bypass the `isFSDSelectedInUI` CAN-level check so frames are modified even if the car's Traffic Light toggle is absent. **This does not bypass Tesla's server-side entitlement verification** — it only affects the local CAN frame flow. Useful when the toggle exists in the car but is hidden by the region UI gating. |
| **Suppress Chime** | Kill the ISA speed warning chime (HW4 only, CAN ID `0x399`) |
| **Emerg. Vehicle** | Enable emergency vehicle detection flag (HW4 only, bit59) |
| **Nag Killer** | EPAS counter+1 echo on `0x370` (CAN 880 method, ported from upstream MR !44) |
| **Precondition** | Periodic `0x082 byte[0] = 0x05` inject to trigger BMS battery preheat. Same trick Tesla uses for Supercharger preconditioning. |

### HW Support

| Tesla HW | Bits Modified | Speed Profile |
|----------|---------------|---------------|
| Legacy (HW1/HW2) | bit46 | 3 levels (0-2) |
| HW3 | bit46 | 3 levels (0-2) |
| HW4 (FSD V14+) | bit46 + bit60, bit47 | 5 levels (0-4) |

> **Firmware warning:** 2026.2.9.x and 2026.8.6 — FSD is **not working on HW4**. Use HW3 mode on these versions even if your car has HW4 hardware. See [Compatibility](#compatibility).

---

## Hardware Requirements

| Component | Description | Price |
|-----------|-------------|-------|
| [Flipper Zero](https://flipper.net/) | The multi-tool device | ~$170 |
| [Electronic Cats CAN Bus Add-On](https://electroniccats.com/store/flipper-addon-canbus/) | MCP2515-based CAN transceiver | ~$30 |
| OBD-II cable or tap | Connect to Tesla's Party CAN bus | ~$10 |

### Wiring

<p align="center">
  <img src="images/wiring_diagram.png" alt="Wiring Diagram" width="700">
</p>

> **Termination resistor:** Electronic Cats ships two revisions of this Add-On. v0.1 has the 120 Ω terminator enabled by default and you need to open the `J1 / TERM` solder jumper on the bottom of the board. v0.2+ ships with it already open. To check without opening anything, measure the resistance between the CAN-H and CAN-L screw terminals **before** plugging into the car: ~120 Ω = good (terminator off), ~60 Ω = open the jumper, open circuit = also fine. Full breakdown in [`HARDWARE.md`](HARDWARE.md#termination-resistor--important-detail).

Alternative connection point: **X179 diagnostic connector** in the rear center console (Pin 13 CAN-H, Pin 14 CAN-L on 20-pin; Pin 18/19 on 26-pin).

### Other supported hardware

Don't have or don't want a Flipper Zero? An ESP32 port (PR [#6](https://github.com/hypery11/flipper-tesla-fsd/pull/6)) brings the total cost down to **~$14 / ¥100** with a built-in WiFi web dashboard. Generic MCP2515 modules from Aliexpress also work with the Flipper Zero if you wire them by hand. See [`HARDWARE.md`](HARDWARE.md) for the full comparison + pin maps.

---

## Installation

### Option 1: Download Pre-built FAP

1. Go to [Releases](https://github.com/hypery11/flipper-tesla-fsd/releases)
2. Download `tesla_fsd.fap`
3. Copy to your Flipper's SD card: `SD Card/apps/GPIO/tesla_fsd.fap`

### Option 2: Build from Source

```bash
# Clone the Flipper Zero firmware
git clone --recursive https://github.com/flipperdevices/flipperzero-firmware.git
cd flipperzero-firmware

# Clone this app into the applications_user directory
git clone https://github.com/hypery11/flipper-tesla-fsd.git applications_user/tesla_fsd

# Build
./fbt fap_tesla_fsd

# Flash to Flipper
./fbt launch app=tesla_fsd
```

---

## Usage

1. Plug the CAN Add-On into your Flipper Zero
2. Connect CAN-H/CAN-L to the vehicle's OBD-II port
3. Open the app: `Apps > GPIO > Tesla FSD`
4. Select **"Auto Detect & Start"** (or force HW3/HW4)
5. Wait for detection (up to 8 seconds)
6. The app starts modifying frames automatically

### Screen Display

```
  Tesla FSD Active
  HW: HW4    Profile: 4/4
  FSD: ON    Nag: OFF
  Frames modified: 12345
       [BACK] to stop
```

### Activation Trigger

FSD activates when **"Traffic Light and Stop Sign Control"** is enabled in your vehicle's Autopilot settings. The app watches for this flag in the CAN frame and only modifies frames when it's set.

---

## Compatibility

| Vehicle | HW | Firmware | Mode | Status |
|---------|----|----------|------|--------|
| Model 3 / Y (2019-2023) | HW3 | Any | Auto | Supported |
| Model 3 / Y (2023+) | HW4 | `< 2026.2.3` | Force HW3 | Supported |
| Model 3 / Y (2023+) | HW4 | `2026.2.3` ↔ `2026.2.8` | Auto | Supported |
| Model 3 / Y (2023+) | HW4 | `2026.2.9.x` (FSD v14) | Auto | Supported |
| Model 3 / Y (2023+) | HW4 | `2026.2.10` ↔ `2026.4.x` | Auto | Supported |
| Model 3 / Y (2023+) | HW4 | `2026.8.6` | **Force HW3** | Use HW3 mode (HW4 path broken on this build) |
| Model 3 Highland (2024+) | HW4 | `2026.2.x` | Auto | Reported working — needs more confirmations |
| Model 3 / Y (China, MIC) | HW3 / HW4 | `2026.2.11` | Auto + Force FSD | Reported working — see issues #1, #4, #7 |
| Model S / X (2021+) | HW4 | `>= 2026.2.3` (excl. 2026.8.6) | Auto | Supported |
| Model S / X (2016-2019) | HW1 / HW2 | Any | Legacy | Implemented in v2.0, **needs on-car confirmation** |

### Tested by community

Reports from real cars (file your own via the [Car compatibility report](https://github.com/hypery11/flipper-tesla-fsd/issues/new?template=car_compatibility.yml) issue template):

| Reporter | Car | HW | Firmware | Region | Mode | Result |
|----------|-----|----|----------|--------|------|--------|
| @vbarrier | Model 3 | HW4 | 2026.4.x | EU | Auto | Working |
| @kwangseok73-sudo | Model 3 | HW4 | 2026.2.x | KR | Force FSD | Working |
| @andreiboestean | Model 3 | HW4 | 2026.2.9.3 (FSD v14) | EU | Auto | Working |
| Marow | Model Y Juniper | HW4 | 2026.8.6 | EU | (Force HW3 not yet tested) | "Region not available" → use Force FSD + Force HW3 |

If your car is listed and you've tested, please leave a thumbs-up on the relevant issue so we can confirm.

### HW1/HW2 Legacy Support — Volunteers Needed

Older Model S/X vehicles (2016-2019) use a Mobileye-based architecture with different CAN IDs. The autopilot control frame is on `0x3EE` (1006) instead of `0x3FD` (1021), and the bit layout differs.

The logic is documented in the [Karolynaz/waymo-fsd-can-mod](https://github.com/Karolynaz/waymo-fsd-can-mod) CanFeather mirror (the original `Starmixcraft/tesla-fsd-can-mod` GitLab upstream has since been removed). We need someone with a HW1/HW2 car to validate it before we ship.

**If you have a 2016-2019 Model S/X with FSD and want to help:**

1. Hook up Flipper + CAN Add-On to OBD-II
2. Open the stock CAN sniffer app
3. Confirm CAN ID `0x3EE` (1006) appears on the bus
4. Capture a few frames and post them in [issue #1](https://github.com/hypery11/flipper-tesla-fsd/issues/1)

Once verified, Legacy support is a quick add.

---

## How It Works

Single-bus read-modify-retransmit on Party CAN (Bus 0). No MITM, no second bus tap.

1. ECU sends `UI_autopilotControl` (`0x3FD`) on Bus 0
2. Flipper catches it, flips the FSD enable bits
3. Flipper retransmits — receiver uses the latest frame

### CAN IDs Used

| CAN ID | Name | Role |
|--------|------|------|
| `0x398` | `GTW_carConfig` | HW detection (`GTW_dasHw` byte0 bit6-7) |
| `0x3F8` | Follow Distance | Speed profile (byte5 bit5-7) |
| `0x3FD` | `UI_autopilotControl` | FSD unlock target (mux 0/1/2) |

---

## FAQ

**Does FSD stay unlocked after I unplug?**
No. It's real-time frame modification. Unplug = back to stock.

**Can this brick my car?**
Only UI config frames are touched. No writes to brakes, steering, or powertrain. Still — your car, your risk.

**Do I need the CAN Add-On?**
Yes. Flipper has no built-in CAN. You need the Electronic Cats board or any MCP2515-based module on the GPIO header.

---

## Related projects

| Project | What it is | Hardware |
|---------|------------|----------|
| [ev-open-can-tools/ev-open-can-tools](https://github.com/ev-open-can-tools/ev-open-can-tools) | The upstream project, now on GitHub as a vehicle-agnostic CAN mod toolkit. Formerly `Tesla-OPEN-CAN-MOD` on GitLab (taken down) → `slxslx/tesla-open-can-mod-slx-repo` (archiving) → now here. | Adafruit RP2040 CAN, Feather M4, ESP32, M5Stack ATOMIC CAN |
| ESP32 port — PR [#6](https://github.com/hypery11/flipper-tesla-fsd/pull/6) by @elonleo | Full ESP32 port of this project's CAN logic with a built-in WiFi web dashboard. ~$14 alternative to Flipper Zero + Add-On. | M5Stack ATOM Lite + ATOMIC CAN, Waveshare ESP32-S3-RS485-CAN |
| [tumik/S3XY-candump](https://github.com/tumik/S3XY-candump) | Python tool to dump Tesla CAN bus over WiFi using an enhauto S3XY Commander as a Panda-protocol bridge | Commander dongle |
| [dzid26/ESP32-DualCAN](https://github.com/dzid26/ESP32-DualCAN) | "Dorky Commander" — open-source hardware alternative to the enhauto S3XY Commander | ESP32 + dual CAN |
| [Karolynaz/waymo-fsd-can-mod](https://github.com/Karolynaz/waymo-fsd-can-mod) | Mirror of the original `Starmixcraft/tesla-fsd-can-mod` CanFeather research — the source we ported from. The original GitLab upstream was taken down; this is the currently-reachable copy. | Adafruit Feather M4 CAN |
| [tuncasoftbildik/tesla-can-mod](https://github.com/tuncasoftbildik/tesla-can-mod) | Arduino reference implementation with working frame templates for several non-FSD features | Arduino + MCP2515 |

## Credits

- [commaai/opendbc](https://github.com/commaai/opendbc) — Tesla CAN signal database
- [ElectronicCats/flipper-MCP2515-CANBUS](https://github.com/ElectronicCats/flipper-MCP2515-CANBUS) — MCP2515 driver for Flipper
- `Starmixcraft/tesla-fsd-can-mod` — original CanFeather FSD research (the GitLab repo has since been removed; mirror at [Karolynaz/waymo-fsd-can-mod](https://github.com/Karolynaz/waymo-fsd-can-mod))
- mikegapinski/tesla-can-explorer — 40k Tesla CAN signal dictionary extracted from `libQtCarVAPI.so`
- talas9/tesla_can_signals — per-model wire format reference

## License

GPL-3.0

## Disclaimer

Educational and research use only. **FSD is a premium Tesla feature and must be properly purchased or subscribed to.** Modifying vehicle systems may void your warranty and may violate local laws. You are solely responsible for what you do with this. Full safety and responsible-use notes in [`SECURITY.md`](SECURITY.md).
