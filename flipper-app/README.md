# BinRAW Analyzer — Flipper Zero FAP

Native Flipper Zero app that classifies SubGhz BinRAW `.sub` captures on-device. No desktop required after flashing.

## What it does

1. Browse `.sub` files on the SD card (`/ext/subghz/`)
2. Parse the capture (Frequency, TE, Bit_RAW / Data_RAW)
3. Extract ~30 signal features (entropy, PWM params, Manchester decode incl. Differential, tri-state PWM, segment similarity, inter-segment timing, rolling-code detection, CRC scan, …)
4. Run 17-stage classifier: NOISE → AMR_METER → TPMS → WMBUS_METER → HONEYWELL_5800 → ALARM_SENSOR → SHUTTER_BLIND → ENOCEAN_SWITCH → PT2262_REMOTE → EV1527_REMOTE → DOORBELL → OUTLET_SWITCH → GARAGE_REMOTE → KEYFOB_REMOTE → WEATHER_STATION → LORA_BEACON → UNKNOWN_STRUCTURED
5. Display a scrollable report on screen and save two sidecars next to the source file:
   - `<capture>.report.txt` — full human-readable report
   - `<capture>.bra` — machine-readable key=value metadata (label, confidence, freq, TE, payload_hex, GPS)

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
ufbt cli                 # connect to device; logs appear tagged [BitRaw]
                         # `log info` (default) shows stage checkpoints + heap
                         # `log debug` adds per-segment + per-feature traces
```

## Usage

1. Open the app from the Sub-GHz apps menu (or run directly via `ufbt launch`).
2. Use the file browser to navigate to a `.sub` capture.
3. Press OK to select — the app parses and classifies immediately.
4. Scroll the report with Up/Down.
5. Press Back to return to the file browser and analyze another file.

Both `.report.txt` and `.bra` sidecars are written automatically next to the `.sub` source.

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
| TPMS | ISM, TE 50–200 µs, 2–8 repeating segments; fixed-address sensors warned |
| WMBUS_METER | 868 MHz single Manchester burst, 64–600 bits, <10% errors |
| HONEYWELL_5800 | 433.92/915 MHz, TE 150–250 µs, 40–48 bit PWM, high entropy |
| ALARM_SENSOR | 433.92/868 MHz, entropy ≥0.80, ≥40 inner bits, no clean PWM, ≤3 segments |
| SHUTTER_BLIND | 433.42/433.92/868 MHz, TE 500–780 µs (Somfy/Nice/Faac) |
| ENOCEAN_SWITCH | 868 MHz, 3–5 identical repeats, ≥95% PWM consistency, 28–36 decoded bits |
| PT2262_REMOTE | ISM, tri-state PWM (8–16 symbols), fixed code, TE 100–450 µs |
| EV1527_REMOTE | 433.92 MHz, 2-symbol PWM, 23–26 bits, ~3:1 gap ratio, fixed code, ≥3 repeats |
| DOORBELL | ISM, 5–10 identical repeats, PWM, 16–40 decoded bits |
| OUTLET_SWITCH | ISM, 3–6 repeats, ≥97% similarity, 24–32 decoded bits, fixed code |
| GARAGE_REMOTE | ISM, 2–6 repeats, ≥92% similarity, clean PWM preferred; TE-bucket sub-hints |
| KEYFOB_REMOTE | 315/433.92 MHz, PWM required, 16–48 decoded bits |
| WEATHER_STATION | 433.92 MHz, TE 150–600 µs, entropy ≥0.85, no clean PWM |
| LORA_BEACON | 868 MHz, preamble ≥32 bits, TE 500–1000 µs |
| UNKNOWN_STRUCTURED | Fallback — always matches |

## Firmware compatibility

Built against Flipper Zero OFW SDK. Compatible with **Momentum firmware** (the SDK is the same; Momentum extends it but does not remove APIs used here).
