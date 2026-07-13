<!-- Source: https://docs.flipper.net/zero/nfc/read -->
<!-- Mirrored: 2026-07-14 -->

# Reading NFC Cards - Flipper Zero Documentation

## Overview

Flipper Zero enables users to read, save, and emulate NFC cards operating at 13.56 MHz. These cards contain a unique identifier (UID) and rewritable memory that may be organized into sectors, pages, applications, or other structures depending on the card type.

## How to Read NFC Cards

The reading process is automatic and requires no manual configuration:

1. Navigate to Main Menu > NFC
2. Press Read and hold the card near Flipper Zero's back
3. Once reading completes, go to More > Save
4. Name the card and press Save

### Reading Card Data

Flipper Zero can display data in readable format for supported cards, such as validity dates, balance, or remaining trips. This functionality relies on community-contributed parsers available in the Flipper Zero GitHub repository.

### If Reading Fails

- The card may use 125 kHz RFID technology—use the 125 kHz RFID application instead
- The card might combine two NFC types—preselect the desired card type before reading

## Supported NFC Card Types

### Type A
**MIFARE Classic®** — Flipper Zero reads 1K, 4K, and Mini variants by locating encryption keys from system and user dictionaries. Changes made via mobile app or emulation create shadow files; use "Write to Initial Card" or "Update from Initial Card" to synchronize with physical cards.

**MIFARE Ultralight® & NTAG®** — Flipper Zero reads open pages. Password-protected pages (16 bytes for Ultralight C; 4 bytes for EV1/NTAG21x) can be unlocked using built-in or manually added dictionaries. NTAG data can be written to cards of the same type if default credentials and unlock bits are configured.

**MIFARE® DESFire®** — Reads unprotected applications and files; memory organized by applications.

### Type V
**iCode® SLIX** — Reads and saves data from SLIX, SLIX 2, SLIX L, and SLIX S tags with block-based memory organization.

### Type F
**FeliCa™ Lite S** — Reads, saves, and emulates cards; captures manufacture ID (IDM), manufacture parameter (PMM), and block data.

### Type B
**ST25TB** — Reads and saves UID and card data from ST25TB512, ST25TB512 AC, SRIX512, ST25TB02K, ST25TB04K, and SRIX4K variants.

**Other Type B Cards** — Only UID is readable without saving capability.

## Reading Specific Card Types

For hybrid cards combining two NFC types:

1. Go to Main Menu > NFC > Extra Actions > Read Specific Card Type
2. Select the desired card type
3. Press OK
4. Hold the card near Flipper Zero's back

## Emulating NFC Cards

To emulate a saved card:

1. Navigate to Main Menu > NFC > Saved
2. Select the card and press Emulate
3. Hold Flipper Zero near the reader to transfer data

Flipper Zero can emulate entire cards or just the UID, depending on card type and available data.
