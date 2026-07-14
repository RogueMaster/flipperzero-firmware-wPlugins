# PRD: USB CCID Smartcard Emulator (`ccid_emulator`)

## Overview
A programmable USB CCID (Chip Card Interface Device) smartcard emulator. The
Flipper Zero presents itself as a USB smartcard reader with a virtual card
inserted, responding to APDU commands with configurable responses loaded from
script files on the SD card.

## Problem Statement
Many enterprise and government systems use smartcard authentication (PIV, CAC,
corporate PKI badges). Testing these systems requires physical smartcards, which
are tightly controlled. The Flipper Zero's USB CCID HAL is fully functional but
completely unexposed to end users — no app uses it. This app turns the Flipper
into a programmable smartcard for:
- Testing smartcard-dependent systems with known-bad/edge-case data
- Emulating a smartcard to test reader software behavior
- Replaying captured APDU sequences to analyze system requirements

## Target Users
- Pentesters assessing smartcard-based authentication systems
- Developers testing smartcard reader integration code
- Security researchers analyzing APDU protocols

## Hardware Utilized
- **USB CCID** interface via `furi_hal_usb_ccid` API
- **SD card** for script storage
- **LED** for status indication (green = connected, red = error)

## Core Features

### F1 — Virtual Card Scripts
Scripts define a complete virtual smartcard personality:

```ini
# /ext/ccid_emulator/cards/piv_test.ccid
[card]
name = PIV Test Card
description = Emulates PIV card with test certificates
atr = 3B 88 80 01 00 73 C8 40 13 00 90 00

# Rules: match APDU command → respond with data
# Format: COMMAND_HEX = RESPONSE_HEX

[rules]
# SELECT PIV applet
00 A4 04 00 09 A0 00 00 03 08 00 00 10 00 = 61 11 4F 06 00 00 10 00 01 00 79 07 4F 05 A0 00 00 03 08 90 00

# GET DATA - Card Holder UID (tag 0x3000)
00 CB 3F FF 05 5C 03 5F C1 02 = 53 3A 30 19 ... 90 00

# Default response for unmatched commands
[default]
response = 6A 82
# 6A 82 = "File or application not found"
```

### F2 — ATR Configuration
The Answer-To-Reset (ATR) is the first thing a reader sees:
- Configurable per script
- Pre-built ATR templates for common card types:
  - Generic Java Card
  - PIV / CAC
  - MIFARE DESFire (contacted interface)
  - SIM card (2FF/3FF/4FF)
  - Generic T=0 / T=1

### F3 — APDU Rule Engine
Pattern matching for incoming APDU commands:
- **Exact match**: Full command hex match
- **Masked match**: Wildcard bytes (e.g., `00 A4 04 00 ?? ...`)
- **Prefix match**: Match first N bytes (for variable-length commands)
- **Chained responses**: `61 XX` status triggers GET RESPONSE automatically
- **Sequence rules**: Respond differently to Nth occurrence of same command
- **Default rule**: Catch-all response for unmatched commands

### F4 — Live APDU Monitor
Real-time display of APDU exchange:
- Incoming command (C-APDU) in hex
- Outgoing response (R-APDU) in hex
- Timestamp for each exchange
- Color coding: green = matched rule, yellow = default, red = error
- Full log saved to SD card

### F5 — Card Profiles Manager
- List available card scripts from SD card
- Quick-switch between profiles without unplugging USB
- Import/export card profiles
- Edit basic properties on-device (name, ATR, default response)

### F6 — Record Mode
- Connect Flipper between a real reader and real card (transparent proxy)
- Record all APDU exchanges
- Auto-generate a .ccid script from recorded session
- Allows replaying a real card's behavior without the card

### F7 — USB Descriptor Configuration
- Configurable USB VID/PID to impersonate specific readers
- Configurable manufacturer and product strings
- Presets for common readers: ACR122U, HID Omnikey, Gemalto

## Architecture
```
main ──► SceneManager + ViewDispatcher
           ├── card_browser (FileBrowser)
           ├── card_info (Widget)
           ├── apdu_monitor (Custom View — scrolling hex log)
           ├── record_mode (Custom View)
           ├── settings (VariableItemList)
           └── usb_config (VariableItemList)

ccid_handler (callbacks from USB CCID HAL)
  ├── atr_callback: returns configured ATR
  ├── apdu_callback: matches rules, returns response
  └── pushes APDU log entries to monitor view
```

## Key API Calls
```c
// USB CCID setup
furi_hal_usb_set_config(&usb_ccid, &ccid_cfg);
furi_hal_usb_ccid_set_callbacks(&ccid_callbacks, context);
furi_hal_usb_ccid_insert_smartcard();
furi_hal_usb_ccid_remove_smartcard();

// In callbacks:
// icc_power_on_callback → return ATR bytes
// xfr_datablock_callback → match APDU, return response
```

## File Format Specification
```
.ccid file format:
- INI-style sections
- [card] section: name, description, atr (hex, space-separated)
- [rules] section: COMMAND_HEX = RESPONSE_HEX (one per line)
- [default] section: response hex for unmatched commands
- [sequence:COMMAND_HEX] section: ordered list of responses
- Comments: lines starting with # or ;
- Wildcards: ?? for any byte in command matching
```

## Out of Scope
- Cryptographic operations (no on-card RSA/ECC — just static responses)
- T=0/T=1 protocol negotiation (uses T=0 fixed)
- PC/SC middleware emulation (just raw CCID)
- Writing to real smartcards

## Success Criteria
- Target computer recognizes Flipper as a USB smartcard reader
- Virtual card ATR is read correctly by PC/SC tools (pcsc_scan, opensc-tool)
- Can emulate a PIV card well enough to trigger PIN prompt in Windows
- APDU log captures complete authentication exchange
- Profile hot-swap works without USB re-enumeration
- Record mode captures real card interaction faithfully
