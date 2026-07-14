<!-- Source: https://docs.flipper.net/zero/rfid/write-data -->
<!-- Mirrored: 2026-07-14 -->

# Writing data to T5577 cards

## Overview

The Flipper Zero documentation explains that while most 125 kHz RFID cards are read-only, T5577 rewritable blank cards can be programmed to emulate various low-frequency RFID protocols. These blanks come in multiple formats including cards, keyfobs, stickers, and animal microchips.

## How to Write Data to T5577 Blank Cards

The process involves these steps:

1. Navigate to Main Menu > 125 kHz RFID > Saved
2. Select the card you want to write
3. Press Write
4. Hold your Flipper Zero near the T5577 blank card with the device's back facing the card
5. Upon success, the device displays "successfully written"

## Important Notes

**Potential False Positives:** The device may display the success message when writing to a read-only card if identical data already exists on it.

**If Writing Fails:**

The documentation lists possible causes:
- The T5577 blank card might be password-protected
- You may be attempting to write to a read-only RFID card rather than a T5577 blank
- The card may use a chip incompatible with Flipper Zero for writing operations

The Flipper Zero supports writing with all its supported low-frequency RFID protocols to compatible T5577 blanks.
