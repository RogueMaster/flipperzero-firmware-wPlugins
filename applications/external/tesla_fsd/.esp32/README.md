# Tesla Mod for ESP32 (OBD-II Plug & Play)

> ESP32 port of [hypery11/flipper-tesla-fsd](https://github.com/hypery11/flipper-tesla-fsd) — same CAN logic, different hardware, with a built-in WiFi dashboard.

Unlock Tesla FSD with an ESP32 + CAN transceiver via OBD-II. No Flipper Zero needed — ~¥100 total cost.

> [!CAUTION]
> **ESP32 port — tested on real cars**
>
> First tested on a **Model 3 (2022, HW3)**. Testers have since run it on a Model 3 2020 HW3 in the EU (@vrs11), a 2017 Model X and Model S (HW3/MCU2) on the LilyGO T-CAN485 (@dmagyar, nag killer), a Model 3 HW4 on 2026.20 with a T-2CAN (@SkyRaax, nag killer on Party CAN), an HW4 car on 2026.14.6 (@ssw0209-sys, nag killer + Nag Burst + AP-First) and a China HW3 car (@dunckencn, Abort Guard / Instant Engage). Per-car results are in the main README's [Compatibility](../README.md#compatibility) table and in the [changelog](../changelog.md).
>
> **If you test this on another vehicle model, please file a [Car compatibility report](https://github.com/hypery11/flipper-tesla-fsd/issues/new?template=car_compatibility.yml).**

> [!WARNING]
> **BMS panel: Model 3/Y frames only, not yet confirmed on the ESP32.**
>
> It reads `0x132` / `0x292` / `0x312`. Model S/X use different BMS IDs, so values are wrong there ([#29](https://github.com/hypery11/flipper-tesla-fsd/issues/29)). Those frames aren't on every tap either (e.g. not on Chassis CAN on a 2022 Model 3, [#46](https://github.com/hypery11/flipper-tesla-fsd/pull/46)), and no test report has confirmed the ESP32 readout on a 3/Y yet.

> [!IMPORTANT]
> The device boots in **Listen-Only mode** by default and will **not transmit any CAN frames** until the user explicitly switches to Active mode via the physical button or Web Dashboard UI. This ensures safe first-boot behavior. The chosen mode is saved and restored on the next boot; a factory reset returns it to Listen-Only.

---

## What's This?

This project takes the CAN bus logic from [hypery11's Flipper Zero FSD unlocker](https://github.com/hypery11/flipper-tesla-fsd) and ports it to the ESP32 platform (M5Stack ATOM Lite + ATOMIC CAN Base), adding a WiFi web dashboard for real-time monitoring and control.

### What Was Ported

Most CAN protocol handling from hypery11's Flipper Zero implementation (`fsd_handler.c`), including (not ported: GTW Config Replay / Tier Override on `0x7FF`, ScrollPress AP on `0x3C2`, the Nav / Hands-Off / Dev Mode / Lane Graph toggles, and the Flipper Extras menu):

- FSD activation bit manipulation (bit46 on `0x3FD`)
- HW3/HW4/Legacy auto-detection via `0x398` GTW_carConfig
- NAG Killer (EPAS `0x370` counter+1 echo with handsOnLevel spoofing)
- Speed profile mapping from follow-distance stalk
- OTA update detection with automatic TX suspension by default, plus an explicit Ignore OTA override
- In-car Autopark pause: all TX stops while the car runs Autopark and resumes when it ends (#180)
- HW-based AP/DAS mapping for Legacy/HW3 vs HW4 signal layouts
- ISA speed warning chime suppression (HW4 only)
- BMS data parsing logic (voltage, current, SOC, temperature) — Model 3/Y frames; wrong values on S/X, not yet confirmed on the ESP32
- Battery precondition trigger
- CRC/checksum recalculation after frame modification
- DLC length validation on all handlers; frames with DLC > 8 are dropped in the driver, and extended / remote frames never reach a handler

### What Was Added (New)

**WiFi Access Point + Web Dashboard** — inspired by [wjsall's ESP32 WiFi Web version](https://github.com/wjsall/tesla-fsd-controller) and [tuncasoftbildik's Tesla-style UI](https://github.com/tuncasoftbildik/tesla-can-mod):

- **WiFi AP mode** — connects without internet, SSID: `Tesla-FSD`, password: `12345678`
- **Tesla dark theme UI** — mobile-first responsive design (optimized for phone in portrait)
  - Dark background (#0a0a1a) with accent gradients, inspired by Tesla's in-car UI
  - All HTML/CSS/JS embedded in firmware (no external CDN dependencies)
- **Real-time WebSocket push** — 1 Hz state updates via WebSocket on port 81
- **FSD Status Panel** — FSD active/waiting, Listen-Only/Active mode, HW version, NAG Killer state
- **Battery SOC Ring** — animated circular progress bar with color coding (green >60%, yellow >30%, red ≤30%)
- **BMS Live Data UI hooks** — fields exist in UI/API; Model 3/Y frames only (wrong on S/X), not yet confirmed on the ESP32
- **CAN Bus Stats** — RX frame count, TX modified count, CAN errors, frames/second
- **HTTP CAN Log Stream** — phone-friendly candump collection via dashboard button; device streams CAN frames over HTTP on port 82 and the browser saves the collected `.dump` file on Stop
- **Web Controls** — toggle buttons and selectors for:
  - Activate / Stop (Listen-Only ↔ Active; TX on or off)
  - FSD Unlock on/off (the `0x3FD` FSD bits; off by default, so Active alone doesn't inject FSD)
  - Ignore OTA on/off (allows Active mode TX during a detected Tesla OTA)
  - NAG Killer on/off
  - BMS serial output on/off
  - Force FSD toggle
  - Battery Precondition (`0x082` preheat trigger, re-sent every 500 ms while on)
  - TLSSC Restore / TLSSC bit38
  - AP-First (14.x) and the steer-jerk options: Instant Engage, Minimal Inject, Soft Engage, Abort Guard
  - Nag Burst / Nag EPAS-faithful
  - Continuous AP, China Mode
  - Signal Map (advanced)
  - Summon EU Unlock (clears the EU AP restriction and enables summon on `0x3FD` mux1)
  - Continue on Green (proceed through a green light with a lead car; pairs with TLSSC)
  - Right-Hand Drive (RHD) override (`0x3F8` driving-side — RHD markets only)
  - Telemetry Off (experimental) (clears reachable telemetry-enable flags on `0x3F8` / `0x3FD` mux1)
  - AP Branch/Tier selector (experimental apmv3 — Live / Stage / Dev / Stage2 / EAP / Demo, or Off)
  - Track Mode (experimental) (adjustable rotation / stability / cooling via `0x313`)
  - Hardware override selector (Auto-detect / Force HW4 / Force HW3 / Force Legacy)
- **OTA Warning Banner** — pulsing red alert when vehicle OTA update is detected (same banner style for the in-car Autopark TX pause)
- **CAN Errors** — combined controller error count with an RX missed / bus / TX fail split (mostly RX-queue drops on a busy bus, not failed sends)
- **Connection Status** — green/red dot indicator with auto-reconnect on WebSocket disconnect
- **Device Info** — firmware build date, uptime counter, WiFi client count
- **REST API** — `GET /api/status` returns full JSON state
- **TTGO T-Display dashboard** — optional on-board ST7789 status display for the `ttgo-tdisplay` build
- **Web firmware update** — the OTA Firmware Update card takes the app-only `tesla-fsd-<env>.bin` from a release (user `admin`, password = the WiFi AP password). The new image is confirmed after 15 s of runtime; a crash, watchdog reset, brownout or power cut before that boots the previous firmware, so just run the update again.

### CAN Driver Abstraction

Dual CAN driver support (compile-time switch):
- **ESP32 TWAI** — for M5Stack ATOM Lite + ATOMIC CAN Base (CA-IS3050G transceiver)
- **MCP2515 SPI** — for generic ESP32 + MCP2515 CAN module setups
- **LilyGO T-2CAN dual** — native TWAI as `can0` + onboard MCP2515 as `can1`

### HW-Based AP/DAS Mapping

The AP/DAS status source is selected at runtime from the detected Tesla hardware version:

| Detected HW | DAS status | ISA_SPEED / chime suppress |
|-------------|------------|-----------------------------|
| Legacy HW1/HW2 | `0x399` | Disabled; `0x399` is treated as DAS status |
| HW3 | `0x399` | Disabled; `0x399` is treated as DAS status |
| HW4 | `0x39B` | Enabled on `0x399` |

Auto-detect picks the source. If it's wrong, pin the car with the Hardware selector, or override the read position under Signal Map (advanced). The dashboard hides the chime toggle until HW4 is detected.

---

## Features

| Feature | CAN ID | Description |
|---------|--------|-------------|
| **FSD Unlock** | `0x3FD` mux0 | bit46 = 1 activates FSD (HW3/HW4/Legacy) |
| **NAG Killer** | `0x370` | Suppresses hands-on-wheel reminder |
| **Speed Profile** | `0x3FD` mux0 (HW3/Legacy) / mux2 (HW4) | Follow-distance stalk selects the speed profile (HW4: mux2 bits 60-62; bit 63, the mux valid flag, is left alone) |
| **DAS Status** | `0x399` or `0x39B` | Runtime HW version selects status source |
| **ISA Chime Suppress** | `0x399` | HW4 only; disabled for Legacy/HW3 because `0x399` is DAS status |
| **Summon EU Unlock** | `0x3FD` mux1 | Clears EU AP restriction (bit19) + enables summon (bit47) |
| **Continue on Green** | `0x3FD` mux0 | Proceed through a green light with a lead car (bit39); pairs with TLSSC |
| **AP Branch/Tier** | `0x3FD` mux1 | Experimental apmv3 branch/tier hint (bits 40-42); off by default |
| **RHD Override** | `0x3F8` | Right-Hand Drive driving-side override (bits 40-41); RHD markets only |
| **Telemetry Off** | `0x3F8` / `0x3FD` mux1 | Experimental; clears reachable telemetry-enable flags |
| **Track Mode** | `0x313` | Experimental adjustable Track Mode (rotation / stability / cooling) |
| **HW Override** | — | Manual Auto-detect / Force HW4 / Force HW3 / Force Legacy selector |
| **Battery Precondition** | `0x082` | Preheat trigger, re-sent every 500 ms while the dashboard **Precondition** switch is on (off by default) |
| **TLSSC Restore** | `0x331` | Recovers stop sign / traffic light control on VIN-banned cars |
| **Continuous AP** | `0x229` | HW3/Legacy, off by default: after AP drops during a turn-signal lane change, re-engages it with a right-stalk double press (brake or full stalk-up cancels) |
| **BMS Dashboard** | `0x132`/`0x292`/`0x312` | Model 3/Y frames; wrong values on S/X, not yet confirmed on the ESP32 |
| **OTA Protection** | `0x318` | Auto-stops TX when OTA update detected unless Ignore OTA is enabled |
| **HW Auto-Detect** | `0x398` | Reads GTW_carConfig for HW version |
| **Listen-Only Mode** | — | Default on first boot, passive monitoring only; the chosen mode is saved and restored on the next boot |
| **Wiring Check** | — | rx_count + CAN error monitoring |
| **WiFi Dashboard** | — | Real-time web UI at 192.168.4.1 |

---

## Hardware

| Component | Description | Price |
|-----------|-------------|-------|
| [M5Stack ATOM Lite](https://docs.m5stack.com/en/core/ATOM%20Lite) | ESP32-PICO-D4, 24×24mm | ~¥60 |
| [ATOMIC CAN Base](https://docs.m5stack.com/en/atom/Atomic%20CAN%20Base) | CA-IS3050G CAN transceiver | ~¥40 |
| OBD-II male plug + 30cm cable | Connects to vehicle Party CAN bus | ~¥15 |

**Total: ~¥100** (vs Flipper Zero + CAN Add-On ~¥1500)

### Alternative Hardware

Any ESP32 board + CAN transceiver works. Pick the matching build env in `platformio.ini`:

| PlatformIO env | Board | CAN driver | CAN pins (TX/RX) | Notes |
|---|---|---|---|---|
| `m5stack-atom` | M5Stack ATOM Lite + ATOMIC CAN Base | TWAI | 22 / 19 | Default, cheapest |
| `m5stack-atom-swap-pins` | M5Stack ATOM Lite + ATOMIC CAN Base | TWAI | 19 / 22 | For boards with swapped silkscreen |
| `m5stack-atom-matrix` | M5Stack ATOM Matrix + ATOMIC CAN Base | TWAI | 22 / 19 | 5×5 LED grid; front button on GPIO 39 |
| `esp32-mcp2515` | Generic ESP32 + MCP2515 module | MCP2515 SPI | SPI CS=5 | 8 MHz crystal |
| `esp32-lilygo` | LilyGO T-CAN485 | TWAI | 27 / 26 | Built-in SN65HVD230 + SD slot |
| `ttgo-tdisplay` | LilyGO/TTGO T-Display + MCP2515 | MCP2515 SPI (HSPI) | CS=26, SCK=33, MISO=32, MOSI=25 | Built-in ST7789 dashboard, MISO needs 5V→3.3V divider |
| `lilygo-t2can` | LilyGO-T2CAN | TWAI/MCP2515 SPI | 7 / 6 | ESP32-S3-WROOM-1U (MCN16R8) with MCP2515 |
| `waveshare-s3-can` | Waveshare ESP32-S3-RS485-CAN | TWAI | 15 / 16 | ESP32-S3, 8MB flash/PSRAM, USB-CDC |
| `esp32-generic-twai` | Classic ESP32 DevKit + SN65HVD230 | TWAI | 21 / 22 | No LED; build from source (no prebuilt image) |
| `esp32-generic-twai-sniffer` | Classic ESP32 DevKit + SN65HVD230 | TWAI (hardware Listen-Only) | 21 / 22 | `SNIFFER_ONLY`: can never transmit; build from source |
| other | Any ESP32 / ESP32-S3 + CAN transceiver | TWAI | any two pins | Copy the closest env and override `PIN_CAN_TX` / `PIN_CAN_RX` (the envs target classic ESP32 or ESP32-S3; there is no ESP32-C3 env) |

Build + upload:

```bash
pio run -e <env-name> -t upload -t monitor
```

On first boot every target prints its pin map as `[CFG] pins: LED=.. BUTTON=.. CAN_TX=.. CAN_RX=..` — if the numbers don't match your board, the build flags are being shadowed by an unguarded `#define` somewhere.

---

## Wiring

### TTGO T-Display + MCP2515

The T-Display LCD already owns the board's default VSPI pins, so the `ttgo-tdisplay` variant places the MCP2515 on a separate HSPI bus:

| MCP2515 Pin | TTGO T-Display GPIO | Notes |
|-------------|---------------------|-------|
| CS | 26 | |
| SCK | 33 | |
| MISO / SO | 32 | **Needs 5 V → 3.3 V divider** if MCP2515 is powered from 5 V (most cheap modules are) |
| MOSI / SI | 25 | |
| INT | (unused) | |
| VCC | 5 V | 5V for TJA1050-based modules; 3V3 only if your module supports it |
| GND | GND | |

> [!IMPORTANT]
> Almost every commodity MCP2515 board on AliExpress runs the chip and
> the TJA1050 transceiver from a 5 V rail and drives MISO at 5 V logic
> levels. The ESP32 GPIOs are 3.3 V tolerant only — driving them at 5 V
> shortens the chip's life and can latch up the SoC. Add a simple
> resistor divider on the MISO line — see
> [HARDWARE.md – MCP2515 MISO 5V to 3.3V voltage divider](../HARDWARE.md#mcp2515-miso-5v-to-33v-voltage-divider).

The built-in ST7789 display is enabled by default and can be toggled from the Web Dashboard or by pushing the GPIO35 button.

### LilyGO-T2CAN

LILYGO's physical connector names are easy to confuse with this project's `can0`/`can1`, so use the mapping below. What matters is the vehicle network a function acts on (by CAN ID), not just the firmware controller name.

| Firmware bus | Vehicle network | X179 pins | CAN IDs / functions |
|---|---|---|---|
| `can0` / TWAI | Chassis CAN | `13 / 14` | Primary AP/DAS control; the `0x082` precondition is transmitted here |
| `can1` / MCP2515 | Party CAN | `2 / 3` | Nag-Killer A torque target: `0x370` |
| `can1` / MCP2515 | Vehicle CAN | `9 / 10` | Scroll/stalk/light/preheat/service: `0x3C2`, `0x229`, `0x249`, `0x273`, `0x339` |

Pins shown are for one harness (`1933903-XX`). Confirm yours on **Service Mode → CAN Port** before wiring.

> [!IMPORTANT]
> The official LILYGO T-2CAN V1.0 labels physical `CANA` as MCP2515/SPI and physical `CANB` as native TWAI.

`can1` is one MCP2515 physical channel. It can be wired to Party CAN or Vehicle CAN for a given test/install, but one MCP2515 channel cannot be on both vehicle networks at the same time.

### OBD-II (Primary — Plug & Play)

| OBD-II Pin | Function | Connect to |
|------------|----------|------------|
| Pin 6 | CAN High | ATOMIC CAN Base CAN-H |
| Pin 14 | CAN Low | ATOMIC CAN Base CAN-L |

Only 2 wires needed. Power via USB-C (car USB port or power bank).

### X179 Diagnostic Connector (Alternative)

Located in the rear center console area:
- 20-pin connector: Pin 13 (CAN-H), Pin 14 (CAN-L)
- 26-pin connector: Pin 18 (CAN-H), Pin 19 (CAN-L)

> [!IMPORTANT]
> **The X179 pin→bus map is not fixed — verify it on your own car.** On many
> harnesses pins 13/14 are **Chassis CAN**, not the "Bus 6" mix, and the third
> CAN pair has moved to other pins on newer builds. The deterministic check is
> the car's **Service Mode → CAN Port** page, which lists each pin's bus by name.
> See [HARDWARE.md – X179](../HARDWARE.md#x179--behind-the-rear-center-console-2021-model-3y)
> for the per-harness maps before you tap.

---

## CAN Bus Details

Most IDs are common. The AP/DAS status and ISA-speed meaning depends on the detected Tesla hardware version.

| CAN ID | Name | Purpose |
|--------|------|---------|
| `0x045` | STW_ACTN_RQ | Steering stalk (Legacy follow distance) |
| `0x082` | TRIP_PLANNING | Battery precondition trigger |
| `0x132` | BMS_HV_BUS | Battery voltage / current |
| `0x292` | BMS_SOC | Battery state of charge |
| `0x312` | BMS_THERMAL | Battery temperature |
| `0x318` | GTW_CAR_STATE | Vehicle state (OTA detection) |
| `0x370` | EPAS_STATUS | EPAS status (NAG killer target) |
| `0x398` | GTW_CAR_CONFIG | HW version detection |
| `0x3EE` | AP_LEGACY | Autopilot control (Legacy / HW1 / HW2) |
| `0x3F8` | FOLLOW_DIST | Follow distance / speed profile |
| `0x3FD` | AP_CONTROL | **Autopilot control (HW3/HW4) — core** |

HW-specific IDs:

| Detected HW | `0x399` | `0x39B` |
|-------------|---------|---------|
| Legacy HW1/HW2 | `DAS_STATUS` — AP / Autosteer state, speed-limit data, hands-on / lane-change state | Not used |
| HW3 | `DAS_STATUS` — AP / Autosteer state, speed-limit data, hands-on / lane-change state | Not used |
| HW4 | `ISA_SPEED` — speed warning chime | `DAS_STATUS` — AP / hands-on state (NAG killer gating) |

Bus speed: **500 kbps**

---

## HW Support

| Tesla HW | Bits Modified | Speed Profile |
|----------|---------------|---------------|
| Legacy (HW1/HW2) | bit46 | 3 levels (0-2) |
| HW3 | bit46 | 3 levels (0-2) |
| HW4 (FSD V14+) | bit46 + bit60, bit47 | 5 levels (0-4) |

---

## Build & Flash

No toolchain? Flash a prebuilt image from the [Web Flasher](https://hypery11.github.io/flipper-tesla-fsd/install/) (Chrome / Edge / Opera on a desktop).

### Prerequisites
- [PlatformIO](https://platformio.org/) (CLI or VSCode extension)
- USB-C cable connected to M5Stack ATOM Lite

### Build
```bash
git clone https://github.com/hypery11/flipper-tesla-fsd.git
cd flipper-tesla-fsd/esp32
pio run -e m5stack-atom
```

### Flash
```bash
pio run -e m5stack-atom -t upload
```

### Monitor Serial Output
```bash
pio device monitor -b 115200
```

### Expected Boot Output
```
============================
 Tesla Mod — ESP32
============================
[FSD] Build: Apr  8 2026 18:59:17
[CAN] Driver: ESP32 TWAI (M5Stack ATOM Lite + ATOMIC CAN Base)
[CAN] 500 kbps — Listen-Only
[BTN] Single click : toggle Listen-Only / Active
[BTN] Long press 3s: toggle NAG Killer
[BTN] Double click : toggle BMS serial output
[LED] Blue=Listen  Green=Active  Yellow=OTA  Red=Error
[WiFi] AP: "Tesla-FSD"  IP: 192.168.4.1
[WiFi] Dashboard: http://192.168.4.1
[Web] HTTP :80  WS :81 — ready
```

---

## Usage

### Physical Controls

1. Flash firmware to M5Stack ATOM Lite
2. Plug ATOMIC CAN Base onto ATOM Lite
3. Connect OBD-II cable: Pin 6 → CAN-H, Pin 14 → CAN-L
4. Plug into the Tesla OBD-II port (on 2019+ Model 3 / 2020–April 2024 Model Y it's in the rear center console area and needs a Tesla adapter cable; newer cars use DoIP there, not CAN — see [HARDWARE.md](../HARDWARE.md))
5. Power M5Stack via USB-C
6. Device starts in **Listen-Only mode** — verify CAN data on serial/dashboard
7. **Single click** button → Active mode (TX on). FSD injection also needs **FSD Unlock** switched on in the dashboard (off by default)

| Button Action | Function |
|---------------|----------|
| Single click | Toggle Listen-Only ↔ Active mode |
| Long press (3s) | Toggle NAG Killer on/off |
| Double click | Toggle BMS serial output |
| Hold 5 s within 20 s of a cold boot | Factory reset (clears NVS; LED turns white when armed, release to confirm) |

### LED Status

| Color | State |
|-------|-------|
| 🔵 Blue | Listen-Only (passive monitoring) |
| 🟢 Green | Active (TX enabled) |
| 🟡 Yellow | OTA detected |
| 🔴 Red | Error (no CAN traffic after 5 s, or CAN init failed) |
| ⚪ White | Factory reset armed |
| 🟣 Dim purple | About to deep-sleep (LilyGO T-CAN485) |

### WiFi Dashboard

1. Connect phone to WiFi: **Tesla-FSD** (password: **12345678**)
2. Open browser: **http://192.168.4.1**
3. Monitor and control everything from the web UI
4. Tap **STREAM LOG AND SAVE** to collect a CAN log on the phone; tap **STOP COLLECTING**, then **SAVE LOG FILE** to save it as a candump `.dump` file
5. REST API available at `http://192.168.4.1/api/status`
6. Raw CAN stream endpoint: `http://192.168.4.1:82/stream`

### Capture fidelity — the two drop metrics

A downloaded capture is only useful if you know whether it saw every frame. The
stream reports two independent drop counters:

- **`dropped`** (stream-ring) — frames the device received but could not push to
  the WiFi client fast enough, so they fell out of the outbound ring buffer.
- **`rx_missed`** (controller) — frames the CAN controller itself discarded
  because its hardware RX queue was full before firmware ever read them. This is
  the real *silent decimation* on a busy bus and is invisible in the frame data.

Both appear in `GET /api/status` under `http_can_stream` and on the dashboard;
`rx_missed` is a per-capture delta that resets when a new stream starts.

Full-rate vs. decimated:

- **All-ID capture** (`/stream`, no `?ids=`) — accept-all; on a busy bus the
  controller RX queue overflows and the capture is **decimated**. Expect a
  rising `rx_missed`.
- **Single-ID capture** (`/stream?ids=<one id>`) — installs a **hardware
  acceptance filter** for that id, so the controller only queues matching
  frames = **full-rate** for that id. `?ids=` also accepts a comma list, and
  `?bus=can0|can1` scopes to one controller. (Listen-Only only. In Active
  mode a single-ID capture falls back to software filtering, so injection
  keeps its RX path.)

Self-labeling captures (`?meta=1`):

Add `?meta=1` (e.g. `/stream?ids=39B&meta=1`) to bracket the capture with two
`#`-prefixed comment lines that candump/SavvyCAN importers ignore:

```
# capture ids=39B bus=all mode=single-id-hwfilter rx_missed_at_start=0
(0.001234) can0 39B#DEADBEEF...
# end sent=1024 dropped=0 filtered=0 rx_missed_delta=0
```

`mode` is `single-id-hwfilter` when exactly one id filter is active, otherwise
`all-id-decimated`. Without `?meta=1` the stream body is byte-for-byte identical
to before, so existing tooling is unaffected.

---

## Safety

- **OTA Protection** — automatically stops all CAN TX when a software update is detected on `0x318`; Ignore OTA can override this in Active mode
- **Listen-Only default** — on first boot the device will not modify any CAN frames until explicitly switched to Active mode; the chosen mode is then saved, and a factory reset returns it to Listen-Only
- **Wiring diagnostics** — monitors rx_count and the CAN error counters; red LED if no CAN traffic
- **Frame checks** — frames with DLC > 8 are dropped in the driver; extended and remote frames go into captures but never reach a parser or handler; `send()` refuses anything that isn't an 11-bit id with at most 8 bytes; every handler checks the data length before parsing
- **Unplug = reset** — remove the device and restart the car to clear any modified state
- **Web OTA rollback** — a web-flashed image is confirmed only after 15 s of runtime; a crash or power cut before that boots the previous firmware
- **WiFi** — runs its own access point by default. If you set a network under Connect to WiFi, it joins that network and falls back to its own AP when it can't connect. The firmware makes no internet connections

---

## Firmware Compatibility

This table is informational from field reports/upstream notes. The ESP32 code itself does not hardcode firmware-version checks.

| Tesla Firmware | HW3 | HW4 | Notes |
|----------------|-----|-----|-------|
| ≤ 2026.2.x | ✅ | ✅ | Full support |
| 2026.8.3 | ✅ | ✅ | HW4 reported working on a Model Y 2025 in Germany ([#80](https://github.com/hypery11/flipper-tesla-fsd/issues/80)) |
| 2026.8.6 | ⚠️ | ❌ | HW4 injection path broken on this build — use Force HW3. Region lock applies too (next row) |
| 2026.8.6+ | ⚠️ | ⚠️ | Region lock — FSD neural net refuses to run in some regions. Pull the SIM and use Force FSD |
| 2026.14.x and newer | ❌ | ❌ | FSD unlock blocked by the activation preflight and an off-CAN region lock; nag killer / TLSSC still work — see [#168](https://github.com/hypery11/flipper-tesla-fsd/discussions/168) |

> **⚠️ Strongly recommended: disable automatic OTA updates** to stay on a compatible firmware version.

---

## Project Structure

```
esp32/
├── .firmware/
│   ├── main.cpp            — Init, button handling, main loop
│   ├── fsd_handler.cpp/h   — CAN protocol logic (ported from hypery11; also includes the shared ../fsd_logic headers such as fsd_ota.h and fsd_autopark.h)
│   ├── can_signals.h       — Bit/byte positions for the parsed and written signals
│   ├── http_can_stream.cpp/h — HTTP candump-compatible CAN stream
│   ├── can_driver.cpp/h    — CAN driver abstraction (TWAI / MCP2515 / T-2CAN dual)
│   ├── can_dump.cpp/h      — SD-card CAN dump
│   ├── blackbox.cpp/h      — Black-box incident recorder (#124)
│   ├── capability.cpp/h    — Tap capability checker (#125)
│   ├── profile_match.cpp/h — Variant-profile auto-suggest (#126)
│   ├── prefs.cpp/h         — NVS settings persistence
│   ├── ota_verify.cpp/h    — Web-OTA image confirm / rollback
│   ├── wifi_manager.cpp/h  — WiFi AP / STA setup
│   ├── web_dashboard.cpp/h — HTTP server + WebSocket + embedded UI
│   ├── display.cpp/h       — TTGO T-Display status screen
│   ├── led.cpp/h           — NeoPixel LED status control
│   └── config.h            — CAN IDs and pin definitions
├── platformio.ini          — Build configs (one env per board, see Alternative Hardware)
├── build_firmware.sh       — Local build + packaging helper
├── merge_firmware.py       — Post-build step that writes firmware-merged.bin for the web flasher
└── README.md
```

---

## Credits

- **[hypery11/flipper-tesla-fsd](https://github.com/hypery11/flipper-tesla-fsd)** — Original Flipper Zero implementation. All CAN protocol logic ported from here.
- **[ev-open-can-tools](https://github.com/ev-open-can-tools/ev-open-can-tools)** — the upstream community project (formerly Tesla-OPEN-CAN-MOD on GitLab). Original CanFeather CAN signal research by Starmixcraft (Alex), mirror at [Karolynaz/waymo-fsd-can-mod](https://github.com/Karolynaz/waymo-fsd-can-mod).
- **[wjsall/tesla-fsd-controller](https://github.com/wjsall/tesla-fsd-controller)** — ESP32 WiFi Web architecture reference.
- **[tuncasoftbildik/tesla-can-mod](https://github.com/tuncasoftbildik/tesla-can-mod)** — Tesla-inspired dark theme UI design reference.
- **[tesla-can-explorer](https://github.com/mikegapinski/tesla-can-explorer)** by @mikegapinski — CAN signal names and DBC definitions.
- **@ssw0209-sys** ([#137](https://github.com/hypery11/flipper-tesla-fsd/issues/137)) — LilyGO T-2CAN firmware / bus / wiring reference.

---

## License

GPL-3.0 — Same as the upstream projects.

---

## Disclaimer

> **⚠️ Only the cars listed at the top of this page and in the main README's Compatibility table have test reports. Anything else is unvalidated.**

This project is for **educational and research purposes only**. Modifying vehicle CAN bus communication may:
- Void your vehicle warranty
- Violate local laws and regulations
- Cause unexpected vehicle behavior
- Create safety hazards

The authors are not responsible for any damage, legal consequences, or safety issues resulting from the use of this software. **Use entirely at your own risk.**
