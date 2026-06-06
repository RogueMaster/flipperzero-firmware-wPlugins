# Subhound
<img width="1924" height="1924" alt="image" src="https://github.com/user-attachments/assets/018aaa38-5762-43e6-abe4-bef1a641cdd3" />

Automated classifier for Flipper Zero BinRAW `.sub` captures. Identifies 17 ISM-band signal types with a full reasoning chain, scoring, and optional wardrive logging.

---

## Hardware

- Flipper Zero with SubGhz
- Flux Capacitor (external RF board) — recommended for BinRAW capture
- High-gain antenna for 315 / 433 / 868 / 915 MHz

Captures are saved as **BinRAW** `.sub` files (not the key-learning format). Subhound reads these directly.

---

## Install

Python 3.10+ and numpy are required. No other dependencies.

```bash
pip install numpy
git clone https://github.com/maxwalks/subhound
cd subhound
```

Optional (run tests):

```bash
pip install pytest
pytest tests/
```

---

## Quick start

```bash
# Classify a single capture
python3 analyze.py data/subfiles/capture.sub

# Classify every .sub in a folder
python3 analyze.py data/subfiles/

# Batch summary table only
python3 analyze.py data/subfiles/ --summary-only
```

---

## CLI reference

```
usage: analyze.py [target] [options]

positional:
  target                .sub file or directory of .sub files

output:
  --summary-only        print batch summary table, skip per-file reports
  --json                emit JSON instead of human-readable text
  --csv FILE            write one CSV row per capture (batch mode)
  --anomalies           flag captures w/ low quality, off-band freq, or class-outlier entropy

calibration:
  --calibrate-from FILE read filename→true-label JSON, write calibration.json
                        (subsequent runs append "~XX% measured, n=K" to confidence line)
```

Pick the mode that fits: a bare `target` gives full per-file reports (great for one capture),
`--summary-only` collapses a folder into one table, and `--json` / `--csv` produce
machine-readable output for a whole batch.

### Examples

```bash
# Full report for a single file
python3 analyze.py capture.sub

# JSON for a single file
python3 analyze.py capture.sub --json

# Batch a folder to a CSV table
python3 analyze.py captures/ --summary-only --csv captures.csv
```

---

## Signal types

| Label | Description | Typical frequencies |
|---|---|---|
| `NOISE` | No real signal — all zeros, too short, or near-flat | any |
| `AMR_METER` | Automatic meter reading (gas / electric / water) | 315, 868 MHz |
| `TPMS` | Tyre pressure monitor (rolling FSK bursts) | 315, 433.92 MHz |
| `WMBUS_METER` | Wireless M-Bus utility meter (single Manchester burst) | 868 MHz |
| `HONEYWELL_5800` | Honeywell 5800-series alarm sensor (specific fingerprint) | 433.92, 915 MHz |
| `ALARM_SENSOR` | Generic alarm sensor (Visonic / DSC / others) | 433.92, 868 MHz |
| `SHUTTER_BLIND` | Somfy RTS / Nice Evo / Faac motorised blind remote | 433.42, 433.92, 868 MHz |
| `ENOCEAN_SWITCH` | EnOcean PTM self-powered switch | 868 MHz |
| `PT2262_REMOTE` | PT2262/PT2272 tri-state fixed-code remote | 315, 433.92 MHz |
| `EV1527_REMOTE` | EV1527/HS1527 learning-code remote (24-bit) | 433.92 MHz |
| `DOORBELL` | Wireless doorbell (≥5 segment repeats) | 315, 433.92 MHz |
| `OUTLET_SWITCH` | Smart plug / RF outlet (3–6 segment repeats) | 315, 433.92 MHz |
| `GARAGE_REMOTE` | Garage / barrier / car remote (TE buckets hint at Skylink/Linear/Stanley) | 315, 433.92 MHz |
| `KEYFOB_REMOTE` | Car or building keyfob | 315, 433.92 MHz |
| `WEATHER_STATION` | Temperature / humidity sensor | 433.92 MHz |
| `LORA_BEACON` | Long-preamble OOK beacon at 868 MHz (LoRa-adjacent) | 868 MHz |
| `UNKNOWN_STRUCTURED` | Structured signal, unrecognised protocol | any |

---

## Example output

```
============================================================
FILE: BinRAW_2026-04-09_19,56,24.sub
============================================================

CLASSIFICATION : GARAGE_REMOTE
CONFIDENCE     : HIGH
SUB-PROTOCOL   : PT2262/generic fixed-code  |  433.92MHz → European garage

KEY METRICS
  Frequency    : 433.92 MHz
  TE           : 174 µs  →  bitrate ≈ 5747 bps
  Segments     : 3  (sizes: 448, 448, 448 bits)
  Segment sim  : 96.2% identical
  Zero ratio   : 75.2%
  Entropy      : 0.808
  PWM          : pulse=3 TE, short_gap=6 TE, long_gap=11 TE  [consistency: 100%]
  Decoded bits : 36

REASONING CHAIN
  [G1] 2–6 segments — consistent with repeated remote transmissions
  [G2] Segment similarity 96.2% — near-identical copies
  [G3] Clean PWM encoding detected (uniform pulse, two distinct gap lengths)
  [G4] PWM decoded 36 bits — in range for PT2262/generic fixed-code remote
  ...
```

---

## .sub file format (reference)

Subhound expects the Flipper Zero **SubGhz Key File v1** BinRAW format:

```
Filetype: Flipper SubGhz Key File
Version: 1
Frequency: 433920000
Preset: FuriHalSubGhzPresetOok650Async
TE: 174
Lat: 52.370200
Lon: 4.895200
Bit_RAW: 448
Data_RAW: AA BB CC ...
Bit_RAW: 448
Data_RAW: AA BB CC ...
```

Each `Data_RAW` line is treated as a separate **segment**. Bits are extracted MSB-first from hex bytes.

---

## Decoders

- **PWM** (2-symbol + optional 3-symbol tri-state for Genie/CAME variants)
- **Manchester** — G.E.Thomas, IEEE 802.3, and Differential Manchester; lowest-error wins
- **CRC** — CRC-8 (poly 0x07 / 0x31) and CRC-16-CCITT trailer scan on decoded payload
- **Rolling-code detection** — per-position diff across PWM-decoded segments
- **Inter-segment timing** — mean + jitter (ms) from padding × TE, surfaced in reports

## Tests

```bash
pytest tests/ -v
```

The suite covers the parity harness (locks current labels on `data/subfiles/`), Manchester
decoding, rolling/fixed code, signal quality, every classifier, CSV output, CRC, tri-state
PWM, and inter-segment timing. Regenerate parity goldens with `python3 tests/regen_golden.py`
after intentional classifier changes.

---

## On-device (Flipper Zero FAP)

The same classifier runs natively on the Flipper Zero — no laptop needed after flashing.

```bash
pip install ufbt
ufbt update
cd flipper-app
ufbt launch    # build + deploy + run on connected Flipper
```

Or copy `flipper-app/dist/subhound.fap` to `/apps/Sub-GHz/` via qFlipper.

Usage: open the app, pick a `.sub` from the file browser, read the report, press Back to pick another. Two sidecars are auto-saved next to each capture: `<name>.report.txt` (human-readable) and `<name>.bra` (key=value metadata for desktop reimport).

See [flipper-app/README.md](flipper-app/README.md) for build options, on-device limits, and debug logging.

---

## Supported frequencies

315 MHz · 433.42 MHz · 433.92 MHz · 868.35 MHz · 915 MHz
