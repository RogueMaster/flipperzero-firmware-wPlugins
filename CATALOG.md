# MTools

MTools brings magic card tools to Flipper Zero using its built-in NFC hardware. No expansion module is required.

## Features

- Check ISO 14443-A and ISO 15693 cards and view their UID and card details.
- Identify supported MIFARE Classic magic card generations: Gen1 (UID), Gen2 (CUID), Gen3 (APDU), Gen4 (UMC), and GDM (USCUID).
- Identify ISO 15693 magic card Gen1 and Gen3. Cards whose generation cannot be proven are shown as unconfirmed.
- Enter a UID manually or read it from a card, write it to a compatible magic card, and verify the result by reading it back.
- Write supported ISO 15693 Gen2 cards when their generation is selected manually. Read-only detection cannot reliably confirm Gen2.

Use these tools only with cards you own or are authorized to modify.
