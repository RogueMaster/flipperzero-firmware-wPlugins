# subhound

Automated classifier for Flipper Zero BinRAW `.sub` captures. Identifies 11 ISM-band signal types with a full reasoning chain, scoring, and optional wardrive logging.

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
git clone https://github.com/maxwalks/BinRAW_Analyzer
cd BinRAW_Analyzer
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
  target                .sub file or directory (omit when using --db-summary)

output:
  --summary-only        print batch summary table, skip per-file reports
  --json                emit JSON instead of human-readable text
  --geojson FILE        write a GeoJSON FeatureCollection to FILE
                        (GPS coords from .sub Lat/Lon fields; zero-coord records excluded)

logging:
  --db FILE             log every capture to an SQLite session database
  --db-summary FILE     print summary stats for an existing session database
```

### Examples

```bash
# JSON output for a single file
python3 analyze.py capture.sub --json

# Batch classify + write map
python3 analyze.py wardrive_session/ --geojson session.geojson

# Log to database while classifying
python3 analyze.py wardrive_session/ --db session.db

# Database summary
python3 analyze.py --db-summary session.db

# All at once
python3 analyze.py wardrive_session/ --json --geojson out.geojson --db session.db
```

---

## Signal types

| Label | Description | Typical frequencies |
|---|---|---|
| `NOISE` | No real signal — all zeros, too short, or near-flat | any |
| `AMR_METER` | Automatic meter reading (gas / electric / water) | 433.92 MHz |
| `TPMS` | Tyre pressure monitor (rolling FSK bursts) | 315, 433.92 MHz |
| `ALARM_SENSOR` | Door / window / PIR sensor | 433.92 MHz |
| `SHUTTER_BLIND` | Motorised shutter or blind remote | 433.92 MHz |
| `DOORBELL` | Wireless doorbell (≥5 segment repeats) | 433.92 MHz |
| `OUTLET_SWITCH` | Smart plug / RF outlet (3–4 segment repeats) | 433.92 MHz |
| `GARAGE_REMOTE` | Garage door / parking barrier remote | 315, 433.92 MHz |
| `KEYFOB_REMOTE` | Car or building keyfob | 433.92, 868 MHz |
| `WEATHER_STATION` | Temperature / humidity sensor | 433.92 MHz |
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

## Wardrive workflow

1. **Capture** — walk or drive with Flipper Zero in BinRAW SubGhz mode
2. **Dump files** — copy `.sub` files from SD card to laptop
3. **Triage** — `python3 analyze.py captures/ --summary-only` to see what's there
4. **Deep dive** — run individual files for full reasoning chains and sub-protocol hints
5. **Log session** — `--db session.db` persists every capture with GPS coords, classification, payload hex, and signal quality
6. **Map it** — `--geojson session.geojson` → load into QGIS, uMap, or Felt

```bash
# Full wardrive pipeline
python3 analyze.py captures/ \
    --db 2026-04-12-session.db \
    --geojson 2026-04-12-session.geojson

# Review session
python3 analyze.py --db-summary 2026-04-12-session.db
```

---

## Session database schema

```
captures (
    id            INTEGER PRIMARY KEY,
    captured_at   TEXT,       -- UTC ISO-8601
    filename      TEXT,
    frequency     INTEGER,    -- Hz
    te_us         REAL,       -- microseconds
    classification TEXT,
    confidence    TEXT,       -- HIGH / MEDIUM / LOW
    lat           REAL,
    lon           REAL,
    payload_hex   TEXT,
    signal_quality REAL,      -- 0.0–1.0
    sub_protocol  TEXT
)
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

## Tests

```bash
pytest tests/ -v
```

22 tests covering: Manchester decoding (both conventions + tiebreak), rolling/fixed code detection, signal quality scoring, all classifiers, GeoJSON formatting, and database operations.

---

## Supported frequencies

315 MHz · 433.42 MHz · 433.92 MHz · 868.35 MHz · 915 MHz
