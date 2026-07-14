# CCID Emulator

```
  ____ ____ ___ ____  _____                 _       _
 / ___/ ___|_ _|  _ \| ____|_ __ ___  _   _| | __ _| |_ ___  _ __
| |  | |    | || | | |  _| | '_ ` _ \| | | | |/ _` | __/ _ \| '__|
| |__| |___ | || |_| | |___| | | | | | |_| | | (_| | || (_) | |
 \____\____|___|____/|_____|_| |_| |_|\__,_|_|\__,_|\__\___/|_|
```

**USB CCID smartcard emulator for Flipper Zero**

[![Platform](https://img.shields.io/badge/platform-Flipper%20Zero-orange)](https://flipperzero.one)
[![Firmware](https://img.shields.io/badge/firmware-1.4.3-blue)](https://github.com/flipperdevices/flipperzero-firmware)
[![Category](https://img.shields.io/badge/category-USB%20CCID-yellow)](.)

> **For authorized security research, smartcard testing, and educational use only.**

---

## What It Does

CCID Emulator makes the Flipper Zero appear as a USB smartcard reader with a virtual card inserted. The host OS sees a standard CCID-compliant reader, and the Flipper responds to ISO 7816-4 APDUs according to rules defined in `.ccid` card profile files.

Use cases:
- Test applications that rely on smartcard authentication (PIV, CAC, OpenPGP)
- Replay or fuzz smartcard APDU sequences
- Prototype new card profiles without physical hardware
- Security assessment of CCID/smartcard middleware

---

## Installation

**Build:**
```bash
cd ccid_emulator
ufbt
```

**Deploy:**
```
dist/ccid_emulator.fap  →  /ext/apps/USB/ccid_emulator.fap
```

**Card profiles:**
```
*.ccid files  →  /ext/ccid_emulator/cards/
```

---

## Card Profile Format (`.ccid`)

Card profiles are plain-text INI-style files. Two sample profiles ship in `ccid_emulator_sample_cards/`.

### File Structure

```ini
# Comment

[card]
name        = My Card
description = One-line description
atr         = 3B 88 80 01 00 73 C8 40 13 00 90 00

[rules]
# APDU request (hex, space-separated) = APDU response (hex, space-separated)
00 A4 00 00 02 3F 00 = 90 00
00 A4 04 00 09 A0 00 00 03 08 00 00 10 00 = 61 11 4F ...

# Wildcard byte: ?? matches any single byte
00 20 00 80 ?? = 90 00

[default]
# Response for any APDU not matched by a rule
response = 6A 82
```

### Sections

**`[card]`** — Card metadata

| Key | Description |
|---|---|
| `name` | Display name shown on Flipper screen |
| `description` | One-line description |
| `atr` | Answer-To-Reset bytes (hex, space-separated). Sent to the host on card insertion. |

**`[rules]`** — APDU response rules

Each rule maps an incoming APDU command to a response:
```
<command APDU hex> = <response APDU hex>
```

- Bytes are space-separated hex values (`00 A4 04 00 ...`)
- `??` in the command pattern matches any single byte
- Rules are matched in order — first match wins

**`[default]`** — Fallback response

The `response` value is returned for any APDU not matched by a rule. Standard error codes:
- `6A 82` — File not found
- `69 85` — Conditions not satisfied
- `90 00` — Success

---

## Sample Profiles

### `test_card.ccid`

Basic test card implementing a minimal PIV applet:
- SELECT MF (Master File)
- SELECT by PIV AID
- GET DATA — Card Holder Unique Identifier (CHUID)
- VERIFY PIN (always succeeds)
- GET RESPONSE

### `piv_emulator.ccid`

Extended PIV card profile with additional data objects. Use for testing PIV middleware, Windows Smart Card Logon, and macOS CryptoTokenKit.

---

## Common APDU Reference

| Command | APDU | Notes |
|---|---|---|
| SELECT MF | `00 A4 00 00 02 3F 00` | Select master file |
| SELECT by AID | `00 A4 04 00 <len> <AID>` | Select applet |
| GET DATA | `00 CB 3F FF 05 5C 03 <tag>` | Retrieve data object |
| VERIFY PIN | `00 20 00 80 <len> <PIN>` | PIN verification |
| GET RESPONSE | `00 C0 00 00 <Le>` | Fetch buffered response |
| EXTERNAL AUTH | `00 82 00 00 08 <data>` | External authenticate |

**Common PIV AIDs:**

| Applet | AID |
|---|---|
| PIV | `A0 00 00 03 08 00 00 10 00` |
| OpenPGP | `D2 76 00 01 24 01` |
| OATH/TOTP | `A0 00 00 05 27 21 01` |

---

## Writing a Custom Profile

**Goal:** Emulate a simple access badge that responds to SELECT + GET DATA.

```ini
[card]
name        = Access Badge Demo
description = Simple building access card emulator
atr         = 3B 90 11 00 1F

[rules]
# SELECT by AID (building access applet)
00 A4 04 00 07 D4 10 00 00 01 00 01 = 90 00

# GET DATA — badge ID
00 CB 3F FF 05 5C 03 5F C1 02 = 53 08 41 42 43 44 31 32 33 34 90 00

# PIN VERIFY (always accept)
00 20 00 00 ?? = 90 00

[default]
response = 6A 82
```

**Testing with pcsc-tools (Linux):**
```bash
# Verify host sees the card
pcsc_scan

# Send custom APDUs
opensc-tool --send-apdu 00A4040007D410000001000100
```

---

## SD Card Layout

```
/ext/
├── apps/
│   └── USB/
│       └── ccid_emulator.fap
└── ccid_emulator/
    └── cards/
        ├── test_card.ccid
        ├── piv_emulator.ccid
        └── your_card.ccid
```

---

## Legal Disclaimer

CCID Emulator is for **authorized security research and educational use only.**
Do not use to bypass physical access controls, MFA systems, or authentication mechanisms you do not own or have explicit written permission to test.
