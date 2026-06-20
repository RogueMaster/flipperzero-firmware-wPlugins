# RS41 Tracker — Flipper Zero external app

Receives and decodes **Vaisala RS41** radiosondes using the Flipper Zero's
built-in CC1101 sub-GHz radio — or an **external CC1101 module** on the GPIO header.

Developed by OK1CHP, inspired by [twatch-rs41](https://github.com/yeckel/twatch-rs41).


## Features

- Live decoding of RS41-SG / RS41-SGP radiosonde frames at 1 Hz
- **Reed-Solomon error correction** — RS(255,231) t=12, both interleaved codewords
- Sonde serial number (e.g. N1320638)
- GPS position: latitude, longitude, altitude, satellite count
- Temperature with full sensor calibration (calframes 3–6)
- **Barometric pressure** with full sensor calibration (calframes 6–7) — RS41-SGP only
- RSSI display
- **Signal quality history** — last 10 packets shown as filled (✓) / outline (✗) boxes
- **Frequency scanner** — sweeps 400–406 MHz in 0.1 MHz steps, bar-graph RSSI display
- Frequency tunable from 400.1 to 406.0 MHz in 0.1 MHz steps
- **External CC1101 support** — plug in a cheap CC1101 module on the GPIO SPI header


## Controls

## Decoder view

| Button | Action |
|--------|--------|
| **OK** | Start / stop reception |
| **UP** | Increase frequency +0.1 MHz |
| **DOWN** | Decrease frequency −0.1 MHz |
| **RIGHT** | Switch to frequency scanner |
| **LEFT** | Toggle radio source: built-in ↔ external CC1101 (only when stopped) |
| **BACK** | Exit app |

## Scanner view (RIGHT from decoder)

| Button | Action |
|--------|--------|
| **UP / RIGHT** | Move cursor right (+0.1 MHz) |
| **DOWN / LEFT** | Move cursor left (−0.1 MHz) |
| **OK** | Tune to cursor frequency and return to decoder |
| **BACK** | Exit app |


## Scanner display

Scan 400-406 MHz
────────────────────
 ▌  ▌▌    ▌▌▌  ▌▌  ▌     (61-bar RSSI graph)
────────────────────
403.0 MHz  -84 dBm
OK:decode  BACK:exit

Move the cursor with UP/DOWN.  Press OK to tune the decoder to that frequency.


## Display (decoder)

RS41  405.1 MHz (RX) IN
────────────────────────
ID:N1320638  -84dBm
14.6685N  037.0377W
Alt: 1460m T:+19.0C
P: 849.1hPa Sats: 0
─────────────────────
■■■□□■■■□■              (signal quality: last 10 packets)
OK:stop RT:scan UP/DN:freq

- IN / EX in the header shows the active radio source.
- Signal history row: filled box **■** = decoded OK, outline □ = failed.
- Fields show --- while waiting for calibration frames or GPS fix.


## External CC1101 wiring

Connect a bare CC1101 module to the Flipper Zero GPIO header:

| CC1101 pin | Flipper GPIO | Function       |
|------------|--------------|----------------|
| VCC        | Pin 3        | 3.3 V          |
| GND        | Pin 2        | Ground         |
| MOSI       | Pin 12       | SPI1 MOSI (PA7)|
| MISO/GDO1  | Pin 13       | SPI1 MISO (PA6)|
| SCK        | Pin 15       | SPI1 CLK  (PB3)|
| CSN        | Pin 14       | SPI1 NSS  (PA4)|
| GDO0       | Pin 7        | PC3 (optional) |

Press **LEFT** on the idle decoder screen to toggle between built-in (IN) and
external (EX) CC1101.  The setting takes effect at the next **OK** (start).


## Radio configuration (CC1101)

| Parameter | Value |
|-----------|-------|
| Modulation | 2-FSK |
| Data rate | 4800 bps |
| Frequency deviation | ≈ 2.4 kHz |
| Channel filter BW | ≈ 58 kHz |
| Sync word | 0x086D (bit-reversed RS41 preamble) |
| Frequency range | 400.1 – 406.0 MHz |
| Packet mode | Infinite length (manual FIFO drain via SPI) |

The CC1101 outputs bits MSB-first; RS41 transmits LSB-first.
Each FIFO byte is bit-reversed, then XOR-descrambled with a 64-byte key.

**Note:** The Flipper firmware's furi_hal_subghz_read_packet() silently
consumes the first FIFO byte as a length field, corrupting 318-byte frames.
This app bypasses it with direct SPI FIFO reads.


## RS41 frame decoding

Each frame is 318 bytes in the FIFO (6 sync + 312 payload):

1. **Bit-reversal** — MSB↔LSB swap on every byte
2. **XOR descramble** — 64-byte repeating key, offset 8
3. **Reed-Solomon correction** — RS(255,231) shortened to (156,132), two interleaved codewords, up to t=12 errors per codeword
4. **Skip** byte 48 (frame type), then **TLV block parsing** from byte 49

| Block ID | Name   | Decoded fields |
|----------|--------|----------------|
| 0x79   | STATUS | Serial number; rotating calibration data (calframes 0–50) |
| 0x7A   | PTU    | Temperature and pressure ADC measurements (uint24 LE × 3) |
| 0x7B   | GPS    | ECEF position (int32 × 0.01 m) → WGS-84 via Bowring iteration |

## Temperature calibration (calframes 3–6)

The sonde embeds calibration constants in the STATUS block, 16 bytes per frame,
rotating through 51 calframe indices.  Four consecutive calframes (3, 4, 5, 6)
are needed to assemble Rf1, Rf2, co1(3), calT1(3).

Formula (rs1729 / rs41ptu.c algorithm):
g  = (f2 - f1) / (Rf2 - Rf1)
Rb = (f1·Rf2 - f2·Rf1) / (f2 - f1)
Rc = meas(0) / g - Rb
R  = Rc · calT1(0)
T  = (co1(0) + co1(1)·R + co1(2)·R²  + calT1(1)) · (1 + calT1(2))

## Pressure calibration (calframes 6–7, RS41-SGP only)

Same resistive-bridge structure as temperature, using Fp1, Fp2, Cp(3):
g  = (fp2 - fp1) / (Fp2 - Fp1)
Rb = (fp1·Fp2 - fp2·Fp1) / (fp2 - fp1)
Rc = meas(3) / g - Rb
P  = Cp(0) + Cp(1)·Rc + Cp(2)·Rc²     (hPa)


## Build & deploy

Requires [ufbt](https://github.com/flipperdevices/flipperzero-ufbt):

pip install ufbt          # first time only
cd rs41_tracker
ufbt build                # compile
ufbt launch               # compile + install via USB
ufbt cli                  # open serial console
log                       # (inside ufbt cli) stream debug log

Tested on firmware **1.4.3 (release)**, API 87.1, Target 7, device on /dev/ttyACM1.


## Finding RS41 frequencies

Active sondes near you:

- <https://radiosondy.info>
- <https://sondehub.org>
- Typical Central-European range: **403–406 MHz**


## License

GNU GPL v3 — see top-level LICENSE file.
