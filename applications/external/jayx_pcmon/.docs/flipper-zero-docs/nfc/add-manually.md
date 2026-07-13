<!-- Source: https://docs.flipper.net/zero/nfc/add-manually -->
<!-- Mirrored: 2026-07-14 -->

# Adding new NFC cards

## Overview

With Flipper Zero, you can create virtual NFC cards programmed as access keys or to store data like URLs or business card information. The documentation describes two primary methods for card creation.

## Creating NFC Cards

### Method 1: Generate from Standard Card Types

You can generate virtual NFC cards with the same data structure as these card types:

- MIFARE Ultralight® (standard, EV1 variants)
- NTAG® (203, 213, 215, 216, and i2c variants)
- MIFARE Mini®
- MIFARE Classic® (1K and 4K, with 4-byte or 7-byte UIDs)

**Process:**
1. Navigate to Main Menu > NFC > Add Manually
2. Select your desired card type
3. Review the data and select "More"
4. Enter the card name and select "Save"

The virtual card receives a random UID and default values, similar to a new physical card.

### Method 2: Manual Creation with Custom Values

For systems using UID, ATQA, and SAK data for authentication, you can enter these values manually:

**Process:**
1. Go to Main Menu > NFC > Add Manually
2. Select the card type based on UID size (7-byte or 4-byte NFC A)
3. Enter SAK value in hexadecimal
4. Enter ATQA value in hexadecimal
5. Enter UID value in hexadecimal
6. Enter the card name and save

## Using Your Card

After creating and saving a card, emulate it for programming:
1. Navigate to Main Menu > NFC > Saved
2. Select your card and press "Emulate"
3. Hold Flipper Zero near a reader or smartphone
