# MR60FDA Radar — Flipper Zero FAP

Flipper Zero application for the **MicRadar R60AFD1 / R60AFD2** 60 GHz mmWave radar (also sold as Seeed MR60FDA1 / MR60FDA2).

Displays in real time: **presence**, **motion**, **fall alarm**, **distance**, **breathing rate**, **heart rate**, and body movement level.

## Hardware

| Flipper pin | Radar pin | Signal |
|-------------|-----------|--------|
| Pin 1 (+5V) | VCC | power |
| Pin 8 (GND) | GND | ground |
| Pin 13 (TX, PB6) | RX | UART TX→RX |
| Pin 14 (RX, PB7) | TX | UART RX←TX |

**Note:** TX and RX are **crossed**. Before launching enable **Settings → Power → 5V on GPIO: ON** — otherwise the radar gets no power.

## Screen

MR60FDA Radar (live indicator)

Left column: HR (bpm), Resp (/min), Dist (cm), BSign

Right column: PRESENT/absent box, Motion state, Fall alarm, Residence time

Footer: body movement signal bars, frame counter, Back button

When a fall is detected, the Fall field inverts to **! FALL !**.

## Protocol

Frame format: 53 59 {CTRL} {CMD} {LEN_H} {LEN_L} {DATA...} {CHKSUM} 54 43

Checksum = sum of all bytes from 53 through last data byte, & 0xFF.

| CTRL | CMD  | Data     | Description |
|------|------|----------|-------------|
| 0x80 | 0x01 | 1 B      | Presence push: 0=no, 1=yes |
| 0x80 | 0x81 | 1 B      | Presence response (reply to query) |
| 0x80 | 0x02 | 1 B      | Motion: 0=none 1=static 2=active |
| 0x80 | 0x03 | 1 B      | Body Sign energy 0–100 |
| 0x80 | 0x04 | 2 B BE   | Distance to person, cm |
| 0x80 | 0x05 | 6 B      | XY position (2B x, 2B y, 2B z), bit15=sign |
| 0x80 | 0x0E | 6 B      | Height zones: total(2B) + 4 × 50 cm bands |
| 0x81 | 0x02 | 1 B      | Breathing rate, /min |
| 0x83 | 0x01 | 1 B      | Fall alarm: 0=no, 1=fall |
| 0x83 | 0x04 | 4 B BE   | Residence time, seconds |
| 0x85 | 0x02 | 1 B      | Heart rate, bpm |
| 0x01 | 0x01 | 1 B      | System heartbeat |
| 0x07 | 0x07 | 1 B      | Function status flag |

**Note:** On startup the app sends a presence query (53 59 80 81 00 01 0F BD 54 43) so the display is correct immediately — even if the radar already detected someone before the app opened.

## Build & flash

Install uFBT once: python3 -m pip install ufbt --break-system-packages

Then in the project directory run ufbt to build, ufbt launch to flash.

The app appears at **Apps → GPIO → MR60FDA Radar**.

The Flipper SDK is downloaded automatically to ~/.ufbt/ on first run.

## PC sniffer

Requires **pyserial** (pip install pyserial).

Put Flipper into UART Bridge mode (**Apps → GPIO → UART Bridge**, baud 115200), then run: python3 sniffer.py /dev/ttyACM0 --raw

Prints every decoded frame with a timestamp and hex dump.

## File structure

- application.fam — FAP manifest (GPIO, 4 KB stack)
- mr60fda_app.c — main loop, UART ISR, GUI, input
- mr60fda_parser.h — protocol types and parser API
- mr60fda_parser.c — state machine, auto-resync, CRC
- sniffer.py — PC-side debug sniffer
- icon.png — 10×10 px monochrome app icon
- flipper_zero_r60afd1_wiring.svg — wiring diagram

## Firmware compatibility

Requires OFW ≥ 1.0 (API 87+). Tested on **OFW 1.4.3**.

## Possible extensions

- **TX commands** — change scene (living room / bedroom / bathroom), adjust sensitivity via furi_hal_serial_tx()
- **Settings menu** — variable_item_list for scene and threshold config
- **SD logging** — storage_file_write to record frames for offline analysis
