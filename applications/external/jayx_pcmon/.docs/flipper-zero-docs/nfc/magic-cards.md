<!-- Source: https://docs.flipper.net/zero/nfc/magic-cards -->
<!-- Mirrored: 2026-07-14 -->

# Writing Data to Magic Cards

## Overview

Standard NFC cards have manufacturer-assigned unique identifiers (UIDs) that cannot be modified. However, "magic cards" or UID-rewritable cards are special NFC devices capable of changing their UIDs, enabling them to replicate both the identifier and data from original cards. The documentation references different generations of these cards available on the market.

## Supported Magic Card Types

**Gen1 Magic Cards** can be configured as:
- MIFARE Classic® 1K

**Gen2 Magic Cards** can be configured as:
- MIFARE Classic® 1K (requires compatible Gen 2 card)
- MIFARE Classic® 4K (requires compatible Gen 2 card)

**Gen4 (Ultimate) Magic Cards** support the widest range of configurations:
- Any MIFARE Classic®
- MIFARE Ultralight® EV1
- MIFARE Ultralight® EV2
- NTAG® 203, 213, 215, 216

## Writing Process

To copy an original NFC card to a magic card:

1. **Read the original card** via the NFC menu and ensure all sectors/pages are captured
2. **Access NFC Magic** from Apps > NFC > NFC Magic
3. **Verify compatibility** using the "Check Magic Tag" option
4. **For password-protected Gen4 cards**, authenticate via Gen4 Actions > Auth with Password
5. **Write the data** by selecting the original card file and holding the magic card against the Flipper Zero's back
6. **Optional**: Use the wipe function to reset the card to default UID and clear data
