<!-- Source: https://docs.flipper.net/zero/basics/sd-card -->
<!-- Mirrored: 2026-07-14 -->

# MicroSD Card Setup

## Overview

The Flipper Zero microSD card stores keys, cards, remotes, databases, and other data. The device supports FAT12, FAT16, FAT32, and exFAT file systems and is compatible with microSD cards up to 256 GB, though a standard 16–32 GB card is sufficient.

Flipper Zero uses a slower SPI interface rather than the high-speed SDIO found in smartphones and computers. Despite this limitation, "flipper zero's spi interface can read data at almost 600 kibibytes per second, which is sufficient for the device's tasks."

## Inserting a MicroSD Card

1. Locate the microSD card slot at the bottom of the device
2. Hold the microSD card with pins facing up
3. Align and gently push the card into the slot until it clicks into place
4. Verify that Flipper Zero recognizes the card

## If the Card Isn't Recognized

Possible causes include:
- Incompatible or missing file system → format the card
- Dust in the slot → clean with compressed air
- Card lacks SPI interface → try a different card
- Damaged card → try a different card

## Formatting the MicroSD Card

Through Flipper Zero: Navigate to **Main Menu > Settings > Storage**, select **Format SD Card**, and press right to confirm. The card will be formatted to FAT32.

If formatting fails on the device, try formatting via a PC or use a different card.

## Safely Removing the Card

Always unmount before removal to prevent data corruption:

1. Go to **Main Menu > Settings > Storage**
2. Select **Unmount SD Card** and follow on-screen instructions
3. Once unmounted, gently push and pull the card to remove it
