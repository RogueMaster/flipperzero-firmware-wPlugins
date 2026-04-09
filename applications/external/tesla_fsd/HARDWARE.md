# Hardware

This project runs on three families of hardware. Pick whichever fits your
budget, region availability, and how much soldering you're willing to do.

## Quick comparison

| Option | Total cost | Difficulty | Display | WiFi | Notes |
|--------|------------|-----------|---------|------|-------|
| **Flipper Zero + Electronic Cats CAN Add-On** | ~$210 | Plug & play | Built-in 1.4" mono | No | Reference platform; everything works on day one |
| **Flipper Zero + generic MCP2515 module** | ~$175 | Wire 5 jumpers | Built-in 1.4" mono | No | Cheapest if you already have a Flipper |
| **M5Stack ATOM Lite + ATOMIC CAN Base** (PR #6 ESP32 port) | ~$14 | Plug & play | None (web UI) | Yes — built-in AP | No Flipper required |
| **Waveshare ESP32-S3-RS485-CAN** (PR #6 ESP32 port, TWAI driver) | ~$15 | Wire OBD-II to header | None (web UI) | Yes — built-in AP | All-in-one ESP32 + transceiver on a single board |

## Option 1 — Flipper Zero + Electronic Cats CAN Add-On

The default and most-tested combination. This is what the screenshots and the
demo videos use.

| Component | Source | Price |
|-----------|--------|-------|
| [Flipper Zero](https://flipper.net/) | Flipper store, Joom, Lab.Flipper, Aliexpress | ~$170 |
| [Electronic Cats CAN Bus Add-On](https://electroniccats.com/store/flipper-addon-canbus/) | Electronic Cats store | ~$30 |
| OBD-II male plug + 30 cm pigtail (or DB9 → OBD-II adapter) | Aliexpress / Amazon | ~$10 |

Wiring is plug-and-play once you have the OBD-II pigtail soldered:

| OBD-II pin | Wire | Add-On terminal |
|------------|------|-----------------|
| Pin 6 | CAN-H (usually green/white) | CAN-H |
| Pin 14 | CAN-L (usually green) | CAN-L |
| Pin 16 | 12 V (optional, only if you want to power the Flipper from the car) | n/c |
| Pin 4/5 | GND | GND |

### Termination resistor — important detail

Electronic Cats ships **two revisions** of this Add-On and the termination
default differs:

| Board revision | 120 Ω termination at boot | What to do |
|----------------|---------------------------|------------|
| **v0.1** (early run) | **Enabled** (jumper closed) | Open the `J1 / TERM` solder jumper on the bottom of the board with a knife or by reflowing the bridge |
| **v0.2 / v0.3** (current store stock) | **Disabled** (jumper open) | Nothing to do |

How to tell which one you have without opening the board: with the Add-On
**not** plugged into a car, measure the resistance between the CAN-H and
CAN-L screw terminals.

| Reading | Meaning |
|---------|---------|
| ~120 Ω | Termination is **off** on the Add-On (good — the car already provides 120 Ω) |
| ~60 Ω | Termination is **on** on the Add-On AND the board is somehow already in parallel with another 120 Ω. Open the J1 jumper. |
| Open / overload | Add-On has no termination active and is not yet connected to the car (still good, just connect it). |

If your CAN bus shows lots of error frames or no traffic at all, this is the
first thing to check.

## Option 2 — Flipper Zero + generic MCP2515 module

Any MCP2515-based SPI module from Aliexpress (~$5–10) works. You'll wire 6
lines from the module to the Flipper GPIO header by hand. Pin map:

| MCP2515 module pin | Flipper GPIO pin |
|--------------------|------------------|
| VCC (+5 V) | Pin 1 (5 V) |
| GND | Pin 8/11/18 (any GND) |
| CS | Pin 4 (PA4) |
| SCK | Pin 5 (PB3) |
| MOSI | Pin 6 (PA7) |
| MISO | Pin 7 (PA6) |
| INT | Pin 2 (PA7 alt) — optional, polled mode works without it |

Most generic MCP2515 modules ship with the 120 Ω termination resistor
soldered on. **Find R4 (or J1 / R7 depending on the board revision) and lift
it** before connecting to a car.

This is the cheapest path if you already own a Flipper Zero, but you give
up the snap-in mechanical fit of the Electronic Cats add-on.

## Option 3 — ESP32 (no Flipper required)

PR [#6](https://github.com/hypery11/flipper-tesla-fsd/pull/6) adds a full
ESP32 port maintained as a sibling project under `esp32/`. Same CAN logic
(FSD unlock, nag killer, OTA Guard, BMS dashboard, etc.) running on bare
ESP32 with a built-in WiFi web dashboard accessed from a phone browser. No
Flipper purchase needed.

### Option 3a — M5Stack ATOM Lite + ATOMIC CAN Base

| Component | Source | Price |
|-----------|--------|-------|
| [M5Stack ATOM Lite](https://shop.m5stack.com/products/atom-lite-esp32-development-kit) | M5Stack, Aliexpress | ~$10 |
| [ATOMIC CAN Base (CA-IS3050G)](https://shop.m5stack.com/products/atomic-can-base) | M5Stack, Aliexpress | ~$5 |
| OBD-II male plug + 30 cm pigtail | Aliexpress | ~$5 |
| **Total** | | **~$20 / ¥100** |

Plug & play — the ATOMIC CAN Base snaps onto the ATOM Lite, OBD-II Pin 6/14
solder onto the CAN-H/CAN-L screw terminals. Build with `pio run -e
esp32-twai`.

### Option 3b — Waveshare ESP32-S3-RS485-CAN

A single board with the ESP32-S3 + an SN65HVD230 CAN transceiver already
wired together. Same `esp32-twai` build target — only the GPIO pin
assignment in `esp32/src/config.h` needs to change to match the Waveshare
pinout (TX/RX on GPIO 4/5 by default on that board). Around ¥90 / $13.

This is the cheapest **all-in-one** option, with the trade-off that the
OBD-II connection is via the screw header on the side of the board rather
than a pre-built snap fit.

## Power

The Flipper Zero powers itself from its internal battery — you don't need
12 V from the car. The Add-On runs from the Flipper's GPIO 5 V rail.

The ESP32 boards draw ~150 mA and can be powered from:
- The car's 12 V via OBD-II Pin 16 → a small buck converter to 5 V (cheapest)
- A USB power bank (most flexible)
- The car's USB port if it has one (many Tesla owners use this)

OBD-II Pin 16 is **always live** (it does not go off when the car is in
park-and-locked) so leaving anything plugged in indefinitely will eventually
drain the 12 V battery. Unplug when you're not using the car.

## What about other CAN modules?

Anything that exposes the same MCP2515-over-SPI interface or the ESP32 TWAI
peripheral can be made to work with a config change. We don't actively
support them, but a few that have been confirmed by community testers:

- Joy-IT SBC-CAN01 (MCP2515) — Europe-friendly source for the Flipper path
- Waveshare RS485-CAN-HAT (MCP2515) — works with Flipper after re-wiring jumpers
- Adafruit RP2040 CAN Bus Feather / Feather M4 CAN — out of scope for this
  app, but the upstream CanFeather project targets it directly. Original
  upstream (`Starmixcraft/tesla-fsd-can-mod` on GitLab) has been removed;
  a working mirror lives at
  [Karolynaz/waymo-fsd-can-mod](https://github.com/Karolynaz/waymo-fsd-can-mod).

If you get a non-listed board working, please open an issue or a PR with
the pin map and we'll add it to this table.
