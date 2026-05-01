# Flipper Access Audit

A Flipper Zero app for **defensive auditing of NFC and RFID access-control credentials**.

Tap a card, get an instant risk score and plain-English advice. Save a named session report to SD card.

**Authorized use only.** This tool is intended for security professionals, system owners, and researchers assessing systems they own or are permitted to test.

## Features

- **Deep card classification**: MIFARE Classic 1K/4K/Mini, DESFire EV1/EV2/EV3/Light, MIFARE Plus SL1/SL2/SL3, Ultralight C, NTAG203/213/215/216, NTAG I2C, ISO14443-A/B, ISO15693, FeliCa (Standard/Lite), SLIX, ST25TB; 125 kHz RFID: EM4100, HID H10301, HID Generic, Indala; HID iCLASS (Legacy) 2k/16k/32k
- **Instant risk score**: 0-100 score with HIGH RISK / MODERATE / LOW RISK / SECURE label
- **Per-card advice**: plain-English recommendation written to every report entry
- **Multi-scan sessions**: scan up to 20 cards per session with a live counter on screen
- **Named sessions**: optionally label a session before saving using an on-screen QWERTY keyboard
- **SD card reports**: timestamped .txt report saved to /ext/apps_data/access_audit/ with per-card UID, SAK/ATQA, advice, and session-level advisory
- **On-device report viewer**: browse, scroll, and delete saved reports without leaving the app
- **NFC + RFID + iCLASS**: Left/Right cycles between 13.56 MHz NFC, 125 kHz RFID, and HID iCLASS scanning
- **Active key check**: MIFARE Classic cards are tested against common default keys; if sector 0 is readable with factory defaults the report flags it as a critical finding

## Installation

Download access_audit.fap from the [latest release](https://github.com/matthewkayne/flipper-access-audit/releases/latest) and copy it to apps/Tools/ on your Flipper SD card via qFlipper or USB. Launch from Apps -> Tools -> Access Audit.

To build from source, install [uFBT](https://github.com/flipperdevices/flipperzero-ufbt) and run ufbt in the repo root.

## Usage

**Scan screen**: tap or hold a card to the Flipper. Left/Right cycles between NFC, RFID, and iCLASS modes. Up opens the report list. Back exits.

**Result screen**: shows the card type, UID, risk score (0-100), risk label, and plain-English advice. OK rescans. Back prompts to save the session.

**Reports**: sessions are saved as named .txt files on the SD card. The on-device viewer lets you scroll through past reports and delete them.

## Score labels

- **HIGH RISK** (35-100): legacy credential family (MIFARE Classic, EM4100, HID iCLASS Legacy, MIFARE Plus SL1) or static-replay pattern detected
- **MODERATE** (20-34): risk indicators present; review recommended
- **LOW RISK** (10-19): minor concerns such as incomplete metadata
- **SECURE** (0-9): modern cryptographic family with no major findings

## Card families detected

- MIFARE Classic: 1K, 4K, Mini (identified via SAK byte); active default key check on sector 0
- MIFARE DESFire: EV1, EV2, EV3, Light (identified via GetVersion)
- MIFARE Plus: SL1, SL2, SL3 (identified via security level response)
- MIFARE Ultralight / NTAG: Ultralight C, NTAG203, NTAG213, NTAG215, NTAG216, NTAG I2C
- HID iCLASS: Legacy 2k, Legacy 16k, Legacy 32k (via ACTALL/IDENTIFY/READ block 1 exchange)
- 125 kHz RFID: EM4100, HID H10301, HID Generic, Indala, generic 125 kHz
- ISO14443-A/B, ISO15693, FeliCa Standard/Lite, SLIX, ST25TB

## Source

[github.com/matthewkayne/flipper-access-audit](https://github.com/matthewkayne/flipper-access-audit)
