# Subhound

Subhound tells you what a Flipper Zero SubGhz BinRAW capture actually is. Point it at a `.sub` file on the SD card and it identifies the signal — garage remote, TPMS, weather station, utility meter, alarm sensor, and 12 more — along with a confidence level, a step-by-step reasoning chain, and a sub-protocol hint. Everything runs on-device. No PC or laptop required after install.

## What it does

1. Browses `.sub` files on the SD card under `/ext/subghz/`.
2. Parses the capture (frequency, TE, bit segments).
3. Extracts around 30 signal features — entropy, PWM parameters, Manchester decode (including Differential), tri-state PWM, segment similarity, inter-segment timing, rolling-code detection, and a CRC scan.
4. Runs a 17-stage classifier pipeline and picks the first matching label.
5. Displays a scrollable on-screen report with a summary, drill-down sections, and a full text view.
6. Saves two sidecar files next to the original capture — one human-readable report and one machine-readable key=value metadata file.

## Usage

1. Open Subhound from the Sub-GHz apps menu on your Flipper.
2. Use the file browser to navigate to a `.sub` capture in `/ext/subghz/`.
3. Press OK to select — the app parses and classifies immediately.
4. Use the sections submenu to drill into the reasoning chain, key metrics, payload, Manchester decode, warnings, or the full report.
5. Press Back to return to the file browser and analyze another capture.

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

- Maximum bits per segment: 8192 (heap budget).
- Maximum total bits across all segments: 16384 (heap budget).
- Maximum segments: 16 (stack limit).
- Maximum decoded payload bits: 256 (typical remote upper bound).
- Captures that exceed these limits are **not rejected** — they are analyzed with a truncated subset, and the report notes the truncation.

## Firmware compatibility

Built against the Flipper Zero official SDK and compatible with Momentum firmware. The SDK is shared between them; Subhound only uses APIs that exist in both.

## Source

Full source code, sample captures, and the desktop Python reference implementation are available at https://github.com/maxwalks/subhound.
