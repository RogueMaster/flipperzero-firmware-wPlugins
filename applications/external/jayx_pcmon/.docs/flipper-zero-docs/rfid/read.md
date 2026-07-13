<!-- Source: https://docs.flipper.net/zero/rfid/read -->
<!-- Mirrored: 2026-07-14 -->

# 125 kHz RFID: Reading 125 kHz RFID Cards

## Overview

Flipper Zero enables you to read, save, and emulate 125 kHz RFID cards. A 125 kHz RFID card is a transponder that stores a unique identification number. When scanned with a reader, it transmits its ID number if the protocol is supported.

## Supported Protocols

Flipper Zero can read 125 kHz RFID cards with the following protocols:

- EM Micro EM4100
- EM Micro EM4100/32
- EM Micro EM4100/16
- HID H10301
- IDteck
- Motorola Indala 26
- Kantech
- IOPROX
- AWID
- Farpointe
- Pyramid
- Viking
- Jablotron
- Paradox
- PAC
- Stanley
- Keri
- Gallagher
- Honeywell
- Nexwatch
- Electra
- Securakey
- Guardall
- G-Prox II
- Noralsy

## How to Read 125 kHz Cards

To read and save a 125 kHz card's data:

1. Go to Main Menu > 125 kHz RFID
2. Press Read
3. Hold the card near your Flipper Zero's back
4. Flipper Zero switches between ASK and PSK codings every three seconds
5. Once reading finishes, the card's data appears on screen
6. To save: go to More > Save, name the card, then press Save

**Note:** If reading fails, the card might use NFC technology. Some cards may require up to 10 seconds to read.

## Reading with Preselected Coding

Flipper Zero allows you to read 125 kHz RFID cards with a specific coding:

1. Go to Main Menu > 125 kHz RFID > Extra Actions
2. Select Read ASK or Read PSK
3. Hold the card near your Flipper Zero's back
4. After reading, go to More > Save, name the card, then press Save

## Emulating 125 kHz RFID Cards

To emulate saved 125 kHz RFID cards:

1. Go to Main Menu > 125 kHz RFID > Saved
2. Select the card you want to emulate
3. Press Emulate
4. Hold your Flipper Zero near the reader with the device's back facing the reader
