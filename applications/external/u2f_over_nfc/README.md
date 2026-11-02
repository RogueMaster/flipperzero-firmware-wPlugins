# U2F over NFC for Flipper Zero

This application allows your Flipper Zero to act as a FIDO U2F security key over NFC. You can use it to securely authenticate on supported services just by tapping your Flipper to your phone.

## Features

- **FIDO U2F over ISO14443-4A:** Implements the FIDO Universal 2nd Factor (U2F) protocol transport over NFC.
- **Privacy & Anti-Tracking:** Implements ISO14443-A Random UID. The device generates a random 4-byte UID upon every NFC field drop, preventing tracking across multiple readings.

## Shared Keys & Compatibility

All cryptographic keys and counters are stored in the standard location used by the official Flipper Zero U2F app. This means you can seamlessly use both the standard USB HID U2F application and this NFC application interchangeably with the same synced accounts and registrations!

## Usage

1. Launch the application. The screen will show the Dolphin animation with the text **"Ready. Bring to NFC reader"**.
2. When a service or website prompts you to tap your security key, bring the Flipper Zero close to the NFC reader.
3. The Flipper will seamlessly handle the `REGISTER` or `AUTHENTICATE` APDU commands and display the result. 

## Acknowledgements

- Part of the code for interacting with the `nfc_listener` is taken from [bettse/seos_compatible](https://github.com/bettse/seos_compatible).
