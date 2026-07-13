<!-- Source: https://docs.flipper.net/zero/nfc/mfkey32 -->
<!-- Mirrored: 2026-07-14 -->

# Recovering MIFARE Classic Keys

## Overview

The documentation explains how to recover MIFARE Classic® encryption keys using the Flipper Zero device. This process exploits vulnerabilities in the Crypto-1 encryption algorithm through several attack methods.

## MFKey32 Attack

The MFKey32 attack recovers keys by exploiting "weaknesses in the Crypto-1 encryption algorithm" through nonce pairs collected from readers.

### With Card and Reader Access

1. Read and save the card using Flipper Zero
2. Navigate to Main Menu > NFC > Saved > [card name] > Extract MF Keys
3. Tap the reader repeatedly until 10 nonce pairs are collected
4. Save the collected data

### Recovering Keys from Nonces

Three methods are available:

- **Flipper Mobile App**: Synchronize device and use Tools > MFKey32
- **Flipper Lab**: Connect via USB-C to lab.flipper.net and use NFC Tools
- **MFKey App**: Run directly on Flipper Zero (takes several minutes due to limited processing power)

### With Reader Access Only

Follow similar steps through Main Menu > NFC > Extract MF Keys without needing the physical card, then add recovered keys to the user dictionary.

## Card-Only Attacks

These attacks work directly on cards without reader access, using nested, static nested, and hardnested attack methods. The process involves:

1. Reading the card to collect nonces
2. Using the MFKey app to calculate keys
3. Automatically adding new keys to the user dictionary
4. Re-reading the card to access previously protected sectors

## Troubleshooting

If the MFKey app fails, users can reboot the device, delete nonce files at `/ext/nfc/nested_log`, or clear the user key dictionary at `/ext/nfc/assets/mf_classic_dict_user.nfc`.
