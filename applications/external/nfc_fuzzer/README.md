# NFC Fuzzer

```
 _   _ _____ ____   _____
| \ | |  ___/ ___| |  ___|   _ _______  _ __
|  \| | |_ | |     | |_ | | | |_  /_ / | '__|
| |\  |  _|| |___  |  _|| |_| |/ / / /_ | |
|_| \_|_|   \____| |_|   \__,_/___/____||_|
```

**NFC protocol fuzzer with 11 profiles and 4 strategies for Flipper Zero**

[![Platform](https://img.shields.io/badge/platform-Flipper%20Zero-orange)](https://flipperzero.one)
[![Firmware](https://img.shields.io/badge/firmware-1.4.3-blue)](https://github.com/flipperdevices/flipperzero-firmware)
[![Category](https://img.shields.io/badge/category-NFC-purple)](.)

> **For authorized security research, NFC device testing, and educational use only.**

---

## What It Does

NFC Fuzzer operates the Flipper Zero's NFC hardware in both **listener** (card emulation) and **poller** (reader) modes to send malformed, boundary-case, and mutation-based data to target NFC devices and readers.

Use cases:
- Security assessment of NFC-enabled access control systems
- Fuzzing NFC payment terminals and reader stacks
- Protocol compliance testing for ISO 14443-A/B, ISO 15693, and FeliCa
- Research into NFC middleware and firmware vulnerabilities

---

## Installation

**Build:**
```bash
cd nfc_fuzzer
ufbt
```

**Deploy:**
```
dist/nfc_fuzzer.fap  →  /ext/apps/NFC/nfc_fuzzer.fap
```

---

## Fuzz Profiles

NFC Fuzzer ships with 11 built-in profiles. Profiles 1–9 run in **listener mode** (Flipper emulates a tag; the target is a reader). Profiles 10–11 run in **poller mode** (Flipper emulates a reader; the target is a tag).

### Profile 1 — UID Fuzzing *(listener)*

Emulates a tag with a fuzzed 7-byte ISO 14443-A UID on every poll cycle. Tests reader collision-resolution and de-duplication logic against arbitrary UIDs, including reserved and boundary values.

### Profile 2 — ATQA/SAK Fuzzing *(listener)*

Emulates a tag with fuzzed ATQA (2 bytes) and SAK (1 byte) values. Tests reader type-detection and card-selection logic for malformed or unexpected tag type indicators.

### Profile 3 — Frame Fuzzing *(listener)*

Transmits malformed raw ISO 14443-A frames after anti-collision. Targets frame-layer parsers in NFC middleware.

### Profile 4 — NTAG Fuzzing *(listener)*

Emulates an NTAG-class tag with fuzzed NDEF/page data. Tests NTAG211/213/215/216 command handling in reader applications.

### Profile 5 — ISO 15693 Fuzzing *(listener)*

Emulates an ISO 15693 (vicinity) tag with fuzzed responses. Targets inventory and read command parsers in readers that support HF vicinity cards.

### Profile 6 — Reader Commands *(poller)*

Sends fuzzed ISO 14443-A frames to a tag. Fuzzes INS, P1/P2, and payload bytes across all CLA classes. Requires a physical tag in range.

### Profile 7 — MIFARE Auth *(poller)*

Sends fuzzed MIFARE Classic AUTH60/AUTH61 commands with mutated keys and sector numbers. Tests authentication logic and error-handling in Mifare-capable readers and tags.

### Profile 8 — MIFARE Read/Write *(poller)*

Sends fuzzed READ/WRITE commands with boundary-case block addresses and data payloads. Tests out-of-bounds handling and response parsing.

### Profile 9 — RATS/ATS Fuzzing *(listener)*

Emulates an ISO 14443-4 tag with fuzzed ATS (Answer to Select) responses. Tests reader RATS handling and T=CL layer initialization logic.

### Profile 10 — NFC-B PUPI *(listener)*

Emulates ISO 14443-B tags with fuzzed 4-byte PUPIs (Pseudo-Unique PICC Identifiers). The listener is restarted with each new PUPI. Targets reader collision-resolution for ISO 14443-B devices such as e-passports and transit cards.

### Profile 11 — FeliCa IDm *(listener)*

Emulates FeliCa / ISO 18092 tags with fuzzed 8-byte IDm (Manufacture ID) values. Common Sony manufacturer codes (Suica 0x0120, PASMO 0x0428, FeliCa Lite-S 0x012F) are used as baselines for boundary and mutation strategies. Targets FeliCa reader stacks and transit gate systems.

---

## Fuzz Strategies

Each profile can be run with one of four strategies:

### Strategy 1 — Sequential (Exhaustive)

Steps through all values in the fuzz space in ascending order. Predictable, reproducible, and thorough.

Best for: Compliance testing, regression testing, documentation of tested value space.

```
UID: 04 00 00 00 00 00 00  →  04 00 00 00 00 00 01  →  04 00 00 00 00 00 02  → ...
```

### Strategy 2 — Random Mutation

Applies uniformly random byte values across the fuzz target for each round. Covers the full input space non-deterministically.

Best for: Quickly finding unexpected crashes and parser bugs without exhaustive enumeration.

```
UID seed: 04 01 02 03 04 05 06
  →  A3 F1 02 03 04 05 06
  →  04 01 FF 7F 04 05 06
  →  00 00 00 00 00 00 00
```

### Strategy 3 — Bitflip

Flips one bit at a time across the entire fuzz target. Covers all single-bit mutations systematically.

Best for: Finding bit-level parsing bugs, parity errors, and single-fault injection scenarios.

```
Byte 0, bit 0: 04 → 05
Byte 0, bit 1: 04 → 06
Byte 0, bit 2: 04 → 00
...
```

### Strategy 4 — Boundary

Tests a compact set of known high-value boundary values: `0x00`, `0xFF`, `0x7F`, `0x80`, `0x01`, `0xFE`. Covers every byte position.

Best for: Integer overflow, off-by-one, and length field bugs with a small test case count.

---

## Usage

### From the Menu

1. Open `NFC Fuzzer` from the NFC apps menu
2. Select **Profile** (1–11)
3. Select **Strategy** (1–4)
4. Hold Flipper near target NFC device or reader
5. Press **OK** to start fuzzing
6. Monitor results on screen — anomalies and unexpected responses are highlighted

### Screen Layout

```
┌─────────────────────────────┐
│ NFC Fuzzer  UID Fuzzing     │
│ Strategy: Random            │
│                             │
│ Round:  1247 / 65535        │
│ Payload: 04 A3 F1 03 04 05  │
│                             │
│ Anomalies: 3                │
└─────────────────────────────┘
```

**Anomaly types:**
- **Timeout** — no response received within the configured window
- **Unexpected Response** — response bytes don't match expected protocol
- **Timing Anomaly** — response time exceeds 3× rolling average (potential crash/reset)

### Settings

Accessible from the main menu → **Settings**:

| Setting | Options |
|---------|---------|
| Timeout | 50 ms / 100 ms / 250 ms / 500 ms |
| Inter-test delay | 0 ms / 10 ms / 50 ms / 100 ms |
| Max test cases | 100 / 1000 / 10000 / Unlimited |
| Auto-stop on anomaly | On / Off |

---

## Results

Detected anomalies are saved to the Results list accessible from the main menu. Each result shows:
- Test case number and payload bytes
- Anomaly type
- Response bytes (if any)

Results are also written to the log file at:
```
/ext/nfc_fuzzer/logs/<timestamp>.log
```

---

## SD Card Layout

```
/ext/
├── apps/
│   └── NFC/
│       └── nfc_fuzzer.fap
└── nfc_fuzzer/
    ├── logs/
    │   └── session_20260302_091400.log
    └── custom/
        └── (reserved for future custom payload files)
```

---

## Legal Disclaimer

NFC Fuzzer is for **authorized security research and NFC protocol testing only.**
Fuzzing NFC devices, access cards, or payment terminals you do not own or have explicit written permission to test is illegal.
