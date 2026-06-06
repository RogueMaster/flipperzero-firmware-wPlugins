# Subhound

Automated classifier for Flipper Zero BinRAW `.sub` captures. Identifies 17 ISM-band signal types — garage remotes, TPMS, weather stations, utility meters, alarm sensors, and more — with a full reasoning chain, confidence scoring, and a sub-protocol hint.

Runs natively on the Flipper. No desktop required after flashing.

## What it does

1. Browses `.sub` files on the SD card under `/ext/subghz/`.
2. Parses the capture and extracts around 30 signal features — entropy, PWM parameters, Manchester decode, segment similarity, rolling-code detection, CRC scan.
3. Runs a 17-stage classifier and picks the first matching label.
4. Shows a scrollable on-screen report — summary, reasoning chain, key metrics, decoded payload, warnings, and the full text view.
5. Saves two sidecar files next to the original capture — one human-readable, one machine-readable.

## Usage

1. Open Subhound from the Sub-GHz apps menu.
2. Pick a `.sub` capture in the file browser.
3. Press OK — the app parses and classifies immediately.
4. Drill into any section from the menu, or open the full report.
5. Press Back to analyze another capture.

## Classifier coverage

- NOISE
- AMR_METER
- TPMS
- WMBUS_METER
- HONEYWELL_5800
- ALARM_SENSOR
- SHUTTER_BLIND
- ENOCEAN_SWITCH
- PT2262_REMOTE
- EV1527_REMOTE
- DOORBELL
- OUTLET_SWITCH
- GARAGE_REMOTE
- KEYFOB_REMOTE
- WEATHER_STATION
- LORA_BEACON
- UNKNOWN_STRUCTURED (fallback)

## Supported frequencies

- 315 MHz
- 433.42 MHz
- 433.92 MHz
- 434.42 MHz
- 868.35 MHz
- 915 MHz

## Known limits

- Maximum bits per segment: 8192.
- Maximum total bits: 16384.
- Maximum segments: 16.
- Maximum decoded payload: 256 bits.
- Captures exceeding these limits are **not rejected** — they are analyzed with a truncated subset, and the report notes the truncation.

## Firmware compatibility

Built against the Flipper Zero official SDK and compatible with Momentum firmware.

## Source

Full source code, sample captures, and the desktop Python reference implementation are at https://github.com/maxwalks/subhound.
