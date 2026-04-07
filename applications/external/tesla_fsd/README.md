[English](README.md) | [繁體中文](README_zh-TW.md)

# Tesla FSD Unlock for Flipper Zero

[![GitHub stars](https://img.shields.io/github/stars/hypery11/flipper-tesla-fsd?style=flat-square)](https://github.com/hypery11/flipper-tesla-fsd/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/hypery11/flipper-tesla-fsd?style=flat-square)](https://github.com/hypery11/flipper-tesla-fsd/network)
[![GitHub release](https://img.shields.io/github/v/release/hypery11/flipper-tesla-fsd?style=flat-square)](https://github.com/hypery11/flipper-tesla-fsd/releases)
[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-blue?style=flat-square)](LICENSE)
[![Build](https://img.shields.io/badge/build-ufbt-brightgreen?style=flat-square)](https://github.com/flipperdevices/flipperzero-ufbt)
[![Featured on FSD CAN Mod Hub](https://img.shields.io/badge/featured%20on-FSD%20CAN%20Mod%20Hub-orange?style=flat-square)](https://fsdcanmod.com/project/hypery11-flipper-zero)

Unlock Tesla FSD on your Flipper Zero. No subscription, no computer, just plug into OBD-II and go.

<p align="center">
  <img src="screenshots/main_menu.png" alt="Main Menu" width="256">&nbsp;&nbsp;&nbsp;
  <img src="screenshots/fsd_running.png" alt="FSD Running" width="256">
</p>

---

## Features

- Auto-detect HW3/HW4 from `GTW_carConfig` (`0x398`), or force manually
- **Legacy mode** for HW1/HW2 (Model S/X 2016-2019)
- FSD unlock via bit manipulation on `UI_autopilotControl` (`0x3FD` / `0x3EE`)
- Nag suppression (hands-on-wheel reminder)
- Speed profile defaults to fastest, syncs from follow-distance stalk
- Live status on Flipper screen

### Settings (runtime toggles)

| Setting | Description |
|---------|-------------|
| **Force FSD** | Enable FSD without "Traffic Light and Stop Sign Control" toggle — for regions where this option doesn't exist |
| **Suppress Chime** | Kill the ISA speed warning chime (HW4 only, CAN ID `0x399`) |
| **Emerg. Vehicle** | Enable emergency vehicle detection flag (HW4 only, bit59) |

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

> **Important:** Cut or disable the 120-ohm termination resistor on the CAN Add-On. The vehicle's CAN bus already has its own termination — adding a second one causes communication errors.

Alternative connection point: **X179 diagnostic connector** in the rear center console (Pin 13 CAN-H, Pin 14 CAN-L on 20-pin; Pin 18/19 on 26-pin).

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

| Vehicle | HW Version | Firmware | Mode | Status |
|---------|-----------|----------|------|--------|
| Model 3/Y (2019-2023) | HW3 | Any | HW3 | Supported |
| Model 3/Y (2023+) | HW4 | < 2026.2.3 | **HW3** | Supported |
| Model 3/Y (2023+) | HW4 | >= 2026.2.3 (excl. 2026.2.9.x, 2026.8.6) | HW4 | Supported |
| Model 3/Y (2023+) | HW4 | 2026.2.9.x or 2026.8.6 | **HW3** | Use HW3 mode |
| Model S/X (2021+) | HW4 | >= 2026.2.3 (excl. 2026.2.9.x, 2026.8.6) | HW4 | Supported |
| Model S/X (2016-2019) | HW1/HW2 | Any | Legacy | **Looking for testers** |

### HW1/HW2 Legacy Support — Volunteers Needed

Older Model S/X vehicles (2016-2019) use a Mobileye-based architecture with different CAN IDs. The autopilot control frame is on `0x3EE` (1006) instead of `0x3FD` (1021), and the bit layout differs.

The logic is already documented (see [CanFeather LegacyHandler](https://gitlab.com/Starmixcraft/tesla-fsd-can-mod)), but we need someone with a HW1/HW2 car to validate it before we ship.

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

## Credits

- [commaai/opendbc](https://github.com/commaai/opendbc) — Tesla CAN signal database
- [ElectronicCats/flipper-MCP2515-CANBUS](https://github.com/ElectronicCats/flipper-MCP2515-CANBUS) — MCP2515 driver for Flipper
- [Starmixcraft/tesla-fsd-can-mod](https://gitlab.com/Starmixcraft/tesla-fsd-can-mod) — original CanFeather FSD research

## License

GPL-3.0

## Disclaimer

Educational and research use only. Modifying vehicle systems may void your warranty and may violate local laws. You are solely responsible for what you do with this. Use at your own risk.
