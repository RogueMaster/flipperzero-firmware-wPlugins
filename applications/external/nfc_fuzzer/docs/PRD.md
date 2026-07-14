# PRD: NFC Protocol Fuzzer (`nfc_fuzzer`)

## Overview
An on-device NFC fuzzer that sends malformed, edge-case, and boundary-condition
NFC frames to target readers and tags. Designed to discover crashes, hangs, and
logic errors in NFC reader firmware and access control systems.

## Problem Statement
NFC readers in access control systems, payment terminals, and IoT devices rarely
undergo fuzz testing. Existing NFC fuzz tools (NFCGate, libnfc scripts) require a
laptop + USB reader. A self-contained Flipper tool lets pentesters fuzz readers
in-situ during physical assessments — walk up to a reader, run the fuzzer, walk away.

## Target Users
- Pentesters assessing physical access control systems
- IoT security researchers testing NFC-enabled devices
- Embedded firmware developers hardening NFC reader code

## Hardware Utilized
- **ST25R3916** NFC frontend via `nfc` and `furi_hal_nfc` APIs
- Supports ISO14443-A (most access control), ISO14443-B, ISO15693
- Both poller (reader) and listener (emulator) modes

## Core Features

### F1 — Fuzz Profile Selection
Pre-built fuzz profiles, each targeting a specific layer:

| Profile              | Target    | Mode   | Description                              |
|----------------------|-----------|--------|------------------------------------------|
| `iso14443a_uid`      | Readers   | Listen | Emulate tags with malformed UIDs         |
| `iso14443a_atqa_sak` | Readers   | Listen | Invalid ATQA/SAK combinations            |
| `iso14443a_rats`     | Readers   | Listen | Malformed ATS responses                  |
| `iso14443a_frame`    | Readers   | Listen | Oversized, undersized, bad-CRC frames    |
| `mifare_auth`        | Readers   | Listen | Edge-case MIFARE auth responses          |
| `mifare_read`        | Readers   | Listen | Malformed read/write block responses     |
| `ntag_read`          | Readers   | Listen | Out-of-bounds page responses, bad CC     |
| `iso15693_uid`       | Readers   | Listen | Malformed ISO15693 inventory responses   |
| `reader_commands`    | Tags      | Poll   | Send malformed commands to tags          |
| `custom`             | Either    | Either | User-defined fuzz data from SD card      |

### F2 — Fuzz Strategies
Each profile supports multiple mutation strategies:
- **Sequential**: Iterate through all values in order (e.g., UID bytes 0x00-0xFF)
- **Random**: Random mutations with configurable seed
- **Bitflip**: Flip 1-N bits in valid baseline frames
- **Boundary**: Test boundary values (0x00, 0x7F, 0x80, 0xFF, max-length, etc.)
- **Dictionary**: Use a wordlist of known-bad values from SD card

### F3 — Execution Engine
- Runs selected profile + strategy combination
- Displays progress: current test case # / total, current payload (hex)
- Detects target response anomalies:
  - No response (timeout — possible crash)
  - Unexpected response code
  - Response timing anomaly (significantly slower = possible hang)
- Logs all anomalies to SD card with full payload + response

### F4 — Results Viewer
- On-device list of detected anomalies from current/previous runs
- Each entry shows: test case #, payload (hex), anomaly type, response
- Export full results to SD: `nfc_fuzz_YYYYMMDD_HHMMSS.log`

### F5 — Custom Fuzz Data
- Load custom fuzz payloads from SD card (`/ext/nfc_fuzzer/custom/`)
- Format: one hex-encoded payload per line
- Allows replaying specific payloads that caused anomalies

### F6 — Settings
- Timeout per test case: 50ms / 100ms / 250ms / 500ms
- Inter-test delay: 0ms / 10ms / 50ms / 100ms
- Auto-stop on first anomaly: Yes / No
- Strategy selection
- Max test cases per run: 100 / 1000 / 10000 / Unlimited

## Architecture
```
main ──► SceneManager + ViewDispatcher
           ├── profile_select (Submenu)
           ├── strategy_select (VariableItemList)
           ├── fuzz_run (Custom View — shows progress)
           ├── results_list (Submenu)
           ├── result_detail (TextBox)
           └── settings (VariableItemList)

fuzz_worker (FuriThread)
  ├── generates test cases from profile + strategy
  ├── sends via nfc poller/listener API
  ├── monitors responses + timing
  └── pushes anomalies to results via FuriMessageQueue
```

## Key API Calls
```c
// Listener mode (fuzzing readers)
nfc_alloc();
nfc_config(nfc, NfcModeListener, NfcTechIso14443a);
nfc_set_fdt_listen_fc(nfc, delay);
nfc_listener_tx(nfc, tx_buffer);

// Poller mode (fuzzing tags)
nfc_config(nfc, NfcModePoller, NfcTechIso14443a);
nfc_poller_trx(nfc, tx_buffer, rx_buffer, fwt);

// Collision resolution (setting emulated UID/ATQA/SAK)
furi_hal_nfc_iso14443a_listener_set_col_res_data(uid, uid_len, atqa, sak);
```

## Safety Considerations
- This tool is for authorized security testing only
- Does NOT attempt to clone or replay valid credentials
- All test cases are intentionally malformed — won't open any doors
- Warning splash screen on app launch

## Out of Scope
- FeliCa fuzzing (limited install base outside Japan)
- Contactless payment protocol fuzzing (EMV — legal minefield)
- Writing to target tags (read-only operations in poll mode)

## Success Criteria
- Can detect a timeout/crash in a known-vulnerable NFC reader
- Completes 1000 ISO14443-A UID fuzz cases in < 60 seconds
- Results log is parseable and contains enough detail to reproduce findings
- Clean recovery if Flipper is pulled away from reader mid-fuzz
