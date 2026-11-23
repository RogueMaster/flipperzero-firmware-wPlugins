# MTools

MTools brings magic card tools for MIFARE Classic, MIFARE Ultralight / NTAG, and ISO 15693 to Flipper Zero using its built-in NFC hardware. No expansion module is required.

## Features

- Check ISO 14443-A and ISO 15693 cards and view their UID and card details.
- Identify supported MIFARE Classic magic card generations: Gen1 (UID), Gen2 (CUID), Gen3 (APDU), Gen4 (UMC), and GDM (USCUID).
- Identify MIFARE Ultralight / NTAG DirectWrite Gen2 magic cards and recognize common Type 2 chip models.
- Identify ISO 15693 magic card Gen1 and Gen3. Cards whose generation cannot be proven are shown as unconfirmed.
- Enter a UID manually, read it from a card, or import it from a supported saved NFC file, write it to a compatible magic card, and verify the result by reading it back.
- Support 7-byte UID DirectWrite on compatible MIFARE Ultralight and NTAG magic cards with automatic BCC recalculation and lock preservation.
- Recover corrupted UID BCC bytes on DirectWrite 7-byte MIFARE Ultralight and NTAG tags to unbrick soft-bricked cards.
- Write supported ISO 15693 Gen2 cards when their generation is selected manually. Read-only detection cannot reliably confirm Gen2.

Use these tools only with cards you own or are authorized to modify.
