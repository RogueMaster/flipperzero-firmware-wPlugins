# BinRAW Analyzer — Flipper Zero FAP

Native Flipper Zero app that classifies SubGhz BinRAW `.sub` captures on-device. No desktop required after flashing.

## What it does

1. Browse `.sub` files on the SD card (`/ext/subghz/`)
2. Parse the capture (Frequency, TE, Bit_RAW / Data_RAW)
3. Extract ~25 signal features (entropy, PWM params, Manchester decode, segment similarity, rolling-code detection, …)
4. Run 11-stage classifier (NOISE → AMR_METER → TPMS → ALARM_SENSOR → SHUTTER_BLIND → DOORBELL → OUTLET_SWITCH → GARAGE_REMOTE → KEYFOB_REMOTE → WEATHER_STATION → UNKNOWN_STRUCTURED)
5. Display a scrollable report on screen and save `<capture>.report.txt` next to the source file

The classifier logic and thresholds are a 1:1 port of [`analyze.py`](../analyze.py).

## Build & install

```bash
pip install ufbt
ufbt update              # download SDK once (~50 MB)
cd flipper-app
ufbt                     # builds dist/bitraw_analyzer.fap
```

### Deploy

Connect your Flipper via USB, then:

```bash
ufbt launch              # build + deploy + run
```

Or copy `dist/bitraw_analyzer.fap` to `/apps/Sub-GHz/` on the SD card via qFlipper.

### IDE integration

```bash
ufbt vscode_dist         # generates .vscode/ with compile_commands.json
```

### Debug logs

```bash
ufbt cli                 # connect to device; logs appear tagged [BitrawAnalyzer]
```

## Usage

1. Open the app from the Sub-GHz apps menu (or run directly via `ufbt launch`).
2. Use the file browser to navigate to a `.sub` capture.
3. Press OK to select — the app parses and classifies immediately.
4. Scroll the report with Up/Down.
5. Press Back to return to the file browser and analyze another file.

The `.report.txt` sidecar is written automatically next to the `.sub` source.

## Known limits

| Limit | Value | Reason |
|---|---|---|
| Max bits per segment | 8 192 | Heap budget |
| Max total bits | 16 384 | Heap budget |
| Max segments | 16 | Stack limit |
| Max decoded bits | 256 | Typical remote payload upper bound |

Captures that exceed these limits are **not rejected** — they are analyzed with a truncated subset and the report notes the truncation.

## Supported frequencies

315 MHz · 433.42 MHz · 433.92 MHz · 434.42 MHz · 868.35 MHz · 915 MHz

## Classifier quick-reference

| Label | Key discriminators |
|---|---|
| NOISE | ≤2 set bits, <50 total bits, or >97% zeros |
| AMR_METER | 315/868 MHz, long preamble, Manchester decode |
| TPMS | ISM, TE 50–200 µs, 2–8 repeating segments, ~70–99% similarity |
| ALARM_SENSOR | 433.92/868 MHz, entropy ≥0.90, no clean PWM, ≤3 segments |
| SHUTTER_BLIND | 433.42/433.92 MHz, TE 550–700 µs |
| DOORBELL | ISM, 5–10 identical repeats, PWM, 16–40 decoded bits |
| OUTLET_SWITCH | ISM, 3–4 repeats, ≥97% similarity, 24–32 decoded bits, fixed code |
| GARAGE_REMOTE | ISM, 2–6 repeats, ≥92% similarity, clean PWM preferred |
| KEYFOB_REMOTE | 315/433.92 MHz, PWM required, 16–48 decoded bits |
| WEATHER_STATION | 433.92 MHz, TE 150–600 µs, entropy ≥0.85, no clean PWM |
| UNKNOWN_STRUCTURED | Fallback — always matches |

## Firmware compatibility

Built against Flipper Zero OFW SDK. Compatible with **Momentum firmware** (the SDK is the same; Momentum extends it but does not remove APIs used here).
