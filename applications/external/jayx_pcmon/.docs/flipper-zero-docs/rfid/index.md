<!-- Source: https://docs.flipper.net/zero/rfid -->
<!-- Mirrored: 2026-07-14 -->

# 125 kHz RFID

## Overview

Flipper Zero supports low frequency (LF) radio frequency identification (RFID) technology used in access control systems, animal chips, and supply chain tracking. Unlike NFC cards, LF RFID cards typically offer lower security levels. The technology appears in various form factors including plastic cards, key fobs, tags, wristbands, and animal microchips.

The device includes a built-in LF RFID module capable of reading, saving, emulating, and writing to LF RFID cards.

## 125 kHz RFID Menu

The 125 kHz RFID application offers the following functions:

- **Read** — Reads and saves LF RFID cards
- **Saved** — Lists saved cards that can be emulated and written to rewritable cards
- **Add manually** — Generates new virtual LF RFID cards by entering card IDs
- **Extra actions** — Allows reading LF RFID cards with preselected ASK or PSK coding

## 125 kHz RFID Hardware

Flipper Zero features built-in RFID support with a low frequency antenna positioned at the back of the device. The STM32WB55 microcontroller handles 125 kHz RFID functionality. The low frequency 125 kHz antenna is integrated into a dual-band RFID antenna alongside the high frequency 13.56 MHz antenna.

## Related Topics

- Reading 125 kHz RFID cards
- Adding 125 kHz cards manually
- Writing data to T5577 cards
- Animal microchips
- Flipper Zero schematics
- 125 kHz RFID application source code
