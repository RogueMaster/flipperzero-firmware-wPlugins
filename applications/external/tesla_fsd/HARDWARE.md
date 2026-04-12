# Hardware

## Quick comparison

| Option | Cost | Connection | CAN buses | WiFi | Best for |
|--------|------|------------|-----------|------|----------|
| **Any ESP32 + MCP2515 → X179** | **~$5-7** | X179 4-wire | 1 (bus 6 = mixed) | Yes | Cheapest full-feature setup |
| M5Stack ATOM Lite + ATOMIC CAN → X179 | ~$13-15 | X179 4-wire | 1 (bus 6) | Yes | Plug & play, no soldering |
| **LILYGO T-2CAN ESP32-S3** → X179 | **~$24** | X179 4-wire (+ spare CAN2) | **2 independent** | Yes | Future-proof, dual-CAN ready |
| Waveshare ESP32-S3-RS485-CAN → X179 | ~$18 | X179 4-wire | 1 (TWAI) | Yes | All-in-one board |
| Flipper Zero + Electronic Cats CAN Add-On → OBD-II | ~$234 | OBD-II plug | 1 (Party CAN) | No | If you already own a Flipper |
| Flipper Zero + generic MCP2515 → OBD-II | ~$202-205 | OBD-II wire | 1 (Party CAN) | No | Budget Flipper option |

---

## Connection points on Tesla Model 3/Y

There are two places to tap the CAN bus. **The X179 connector is
recommended** — it provides more signals and built-in 12V power.

### OBD-II (16-pin) — under the steering column

The standard automotive diagnostic port. Every car has one.

```
    ┌──────────────────────────────┐
    │  1  2  3  4  5  6  7  8     │
    │   9 10 11 12 13 14 15 16    │
    └─────────────────────────────┘
```

| Pin | Signal | Notes |
|-----|--------|-------|
| 4 | Chassis GND | |
| 5 | Signal GND | |
| **6** | **CAN-H** | **Party CAN** |
| **14** | **CAN-L** | **Party CAN** |
| **16** | **+12V always-on** | Constant power even when car is locked |

**Bus: Party CAN only.** This carries DAS, ESP, BMS, FSD gate, nag
(EPAS), ISA chime, precondition — everything our v1.0–v2.3 used.

Limitation: Party CAN does not carry stalk signals (SCCM_rightStalk),
lighting commands (VCFRONT_lighting), or steering wheel button inputs
(STW_ACTN_RQ). Those are on Vehicle CAN.

### X052 — 2019 Model 3 (pre-facelift)

The 2019 Model 3 does **not** have the X179 connector or a standard
OBD-II port under the steering column. Instead, it uses the X052
connector, located behind the center console / passenger footwell area.

Confirmed by community tester @THER4iN (issue #21):

| X052 Pin | Signal | Notes |
|----------|--------|-------|
| **44** | **CAN-H** | CAN bus |
| **45** | **CAN-L** | CAN bus |
| **20** | **12V** | Power (no service mode errors confirmed) |
| **22** | **GND** | Ground |

Same 4-wire pattern as X179 — CAN + power. Compatible with all the
same ESP32/MCP2515 setups described below.

The 2019 Model 3 also has an **X930m** connector near the A-pillar.
Pinout not yet confirmed — if you test it, please report in an issue.

### X179 — behind the rear center console (2021+ Model 3/Y)

Tesla's own service/diagnostic connector. Requires removing a trim
panel behind the rear armrest. Two versions exist:

#### X179 20-pin (2021–2023 Model 3/Y)

```
     +---------------------------------+
     |  1   2   3   4   5   6   7      |
     |  8   9  10  11  12  13  14      |
     | 15  16  17  18  19  20          |
     +---------------------------------+
```

| Pin | Signal | CAN bus |
|-----|--------|---------|
| **1** | **+12V** | Power |
| 2 | CAN-H | Bus 4 (diagnostic/forwarded) |
| 3 | CAN-L | Bus 4 |
| 9 | CAN-H | Bus 2 (Vehicle CAN) |
| 10 | CAN-L | Bus 2 |
| **13** | **CAN-H** | **Bus 6 (Body/Left — Gateway mixed forwarding)** |
| **14** | **CAN-L** | **Bus 6** |
| **15** | **+12V** | Power (2mm² wire, alternate to pin 1) |
| 18 | CAN-H | Bus 3 (Chassis CAN — EPAS/brake) |
| 19 | CAN-L | Bus 3 |
| **20** | **GND** | Ground |

**4 separate CAN bus pairs** on one connector. Pin 13/14 (bus 6) is
what aftermarket products (Feifan Commander, enhauto, etc.) connect to.

#### X179 26-pin (2024+ Highland Model 3 / Juniper Model Y)

| Pin | Signal | Notes |
|-----|--------|-------|
| **13** | **CAN-H** | Bus 6 (same as 20-pin) |
| **14** | **CAN-L** | Bus 6 |
| **15** | **+12V** | Power (red wire, 2mm²) |
| 18 | CAN-H | Vehicle CAN (blue wire) |
| 19 | CAN-L | Vehicle CAN (yellow wire) |
| **26** | **GND** | Ground (black wire, 2mm²) |

### Why X179 Pin 13/14 is the best single connection point

The Gateway forwards signals from **multiple internal CAN buses** onto
bus 6 (pin 13/14). Community testing confirms that the following
"Party CAN" signals are visible on X179 pin 13/14:

- `0x3FD` UI_autopilotControl (FSD gate)
- `0x370` EPAS3S_sysStatus (nag killer)
- `0x132` BMS_hvBusStatus (battery voltage/current)
- `0x292` BMS_socStatus (state of charge)
- `0x312` BMS_thermalStatus (battery temp)
- `0x399` ISA speed limit
- `0x39B` DAS_status (AP state, blind spot)
- `0x2B9` DAS_control (ACC state)

And these "Vehicle CAN" signals are also writable on bus 6:

- `0x229` SCCM_rightStalk (gear shift, park)
- `0x3F5` VCFRONT_lighting (hazard, wiper)
- `0x249` SCCM_leftStalk (high beam, turn signal)

**One bus, one connection, reads and writes almost everything.**

This is how the 非凡指揮官 (Feifan Commander, 69K+ sales in China)
achieves its full feature set with just 4 wires:

```
X179 Pin 13 → CAN-H ─┐
X179 Pin 14 → CAN-L ──┤── CAN module (MCP2515 / TWAI)
X179 Pin 15 → 12V ────┤── buck converter → 3.3V/5V
X179 Pin 20 → GND ────┘   (26-pin: use Pin 26 for GND)
```

---

## Recommended setups

### Setup A — Cheapest full-feature (~$6)

Any ESP32 dev board + any MCP2515 CAN module from Aliexpress.

| Component | Price |
|-----------|-------|
| ESP32-C3-SuperMini or ESP32-DevKitC | ~$3-4 |
| MCP2515 CAN module (TJA1050 transceiver) | ~$1.50-3 |
| X179 pigtail cable (4-wire, or DIY from connector) | ~$3-5 |
| **Total** | **~$8-12** |

Wire: X179 CAN-H/CAN-L → MCP2515 module CAN-H/CAN-L. X179 12V → buck
converter → ESP32 VIN. X179 GND → common GND.

Build with `pio run -e esp32-mcp2515`, adjust pin config in
`esp32/.firmware/config.h`.

### Setup B — M5Stack plug & play (~$20)

| Component | Price |
|-----------|-------|
| [M5Stack ATOM Lite](https://shop.m5stack.com/products/atom-lite-esp32-development-kit) | ~$7.50 |
| [ATOMIC CAN Base (CA-IS3050G)](https://shop.m5stack.com/products/atomic-can-base) | ~$5-7 |
| X179 pigtail cable (4-wire) | ~$3-5 |
| **Total** | **~$16-20** |

ATOMIC CAN Base snaps onto the ATOM Lite. Solder X179 CAN-H/CAN-L to
the screw terminals, 12V to VIN, GND to GND. Build: `pio run -e esp32-twai`.

### Setup C — LILYGO T-2CAN dual-CAN (~$33)

| Component | Price |
|-----------|-------|
| [LILYGO T-2CAN ESP32-S3](https://lilygo.cc/products/t-2can) | ~$24 |
| X179 pigtail cable (4-wire) | ~$3-5 |
| **Total** | **~$27-29** |

The T-2CAN has **dual isolated MCP2515 controllers**, dual screw
terminals, 12–24V input, WiFi, BLE, QWIIC, and USB-C. Connect X179
to CAN1 screw terminal. CAN2 stays free for future use (e.g., OBD-II
Party CAN for redundancy, or a second X179 bus pair).

This is the recommended board for anyone who wants headroom for
dual-bus features in a future firmware update.

### Setup D — Flipper Zero + CAN Add-On (~$210)

The original reference platform. Connect to **OBD-II** (not X179) using
the Electronic Cats CAN Bus Add-On or a generic MCP2515 module.

| Component | Price |
|-----------|-------|
| [Flipper Zero](https://flipper.net/) | $199 |
| [Electronic Cats CAN Bus Add-On](https://electroniccats.com/store/flipper-addon-canbus/) | $35 |
| OBD-II pigtail cable | ~$5-10 |
| **Total** | **~$239-244** |

OBD-II wiring (Party CAN only):

| OBD-II pin | Wire | Add-On terminal |
|------------|------|-----------------|
| Pin 6 | CAN-H | CAN-H |
| Pin 14 | CAN-L | CAN-L |
| Pin 4/5 | GND | GND |

The Flipper can also be wired to X179 instead of OBD-II for bus 6
access, but the cable run from the rear console to the Flipper is long.

### Termination resistor

Tesla's CAN buses are already terminated. **Do not add a second 120 Ω
terminator.** Most aftermarket CAN modules ship with the termination
resistor enabled — disable it before connecting to the car.

- **Electronic Cats Add-On v0.1**: open the `J1 / TERM` solder jumper
- **Electronic Cats Add-On v0.2+**: ships disabled, no action needed
- **Generic MCP2515 modules**: find and remove `R4` or `J1`
- **M5Stack ATOMIC CAN Base**: no termination by default
- **LILYGO T-2CAN**: check documentation

Verify: measure resistance between CAN-H and CAN-L with the module
disconnected from the car. ~120 Ω = good (terminator off, car provides
its own). ~60 Ω = your module's terminator is on, disable it.

---

## Power and sleep

### OBD-II Pin 16 — always on

OBD-II Pin 16 supplies +12V **even when the car is locked and
sleeping**. If your module draws 50 mA at 12V (typical ESP32 idle),
that's 0.6W continuous → will drain the 12V battery over days.

### X179 Pin 1/15 — behavior varies

On some Model 3/Y builds, X179 12V is gated by the car's wake state.
On others it's always-on like OBD-II. Test with a multimeter before
relying on it.

### Deep sleep (recommended for permanent install)

For any module that stays plugged in:

1. Monitor CAN bus traffic. If no frames seen for 5 minutes → the car
   is asleep.
2. Enter ESP32 deep sleep (~10 µA draw, negligible battery impact).
3. Wake on MCP2515 INT pin (frame received = car woke up) or on a
   timer (check every 60 seconds).

This is how commercial products (Feifan Commander, enhauto Commander)
handle permanent installation without draining the 12V battery.

---

## What about other CAN modules?

Anything with an MCP2515-over-SPI interface or an ESP32 TWAI peripheral
works with a config change. Community-confirmed boards:

- Joy-IT SBC-CAN01 (MCP2515) — Europe source
- Waveshare RS485-CAN-HAT (MCP2515) — re-wire jumpers for Flipper
- Waveshare ESP32-S3-RS485-CAN — TWAI driver, all-in-one
- Adafruit RP2040 / Feather M4 CAN — see upstream
  [Karolynaz/waymo-fsd-can-mod](https://github.com/Karolynaz/waymo-fsd-can-mod)

If you get a non-listed board working, open a PR with the pin map.
