<!-- Source: https://docs.flipper.net/zero/nfc/unlock-with-password -->
<!-- Mirrored: 2026-07-14 -->

# Unlocking Cards with Passwords

## Overview

The documentation covers methods for unlocking NFC cards (such as MIFARE Ultralight and NTAG cards) that have password-protected pages. There are three primary approaches outlined.

## Three Methods for Unlocking Cards

**1. Extracting the Password from the Reader**

If you have access to both a reader and card, you can capture the authentication password. The process involves:
- Reading and saving the card's data
- Selecting "unlock with reader" from the saved card menu
- Tapping the reader with your Flipper Zero to capture the password
- Reading the remaining pages once unlocked

**2. Generating the Password**

The Flipper Zero can generate passwords for specific card types: "toys to life nfc technology and xiaomi air purifier." The generation derives from the card's UID and occurs through the NFC extra actions menu.

**3. Manual Password Entry**

If you already know the password, you can enter it manually by:
- Accessing the saved card's unlock menu
- Selecting manual password entry
- Inputting the password in hexadecimal format
- Holding the card near the device to complete the unlock

## Related Topics

The page links to adjacent documentation on recovering MIFARE Classic keys and writing data to magic cards.
