# ZeroFIDO

ZeroFIDO turns your Flipper Zero into a FIDO2 passkey and U2F security-key app over USB.

Use it with browsers and services that support WebAuthn, passkeys, or legacy U2F security keys. Open ZeroFIDO, connect your Flipper over USB, start the sign-in or registration flow, and approve the request on the Flipper screen.

## Features

- FIDO2.0 / CTAP2.0 passkey registration and sign-in over USB HID
- U2F compatibility for older security-key flows
- ClientPIN support for sites that require a PIN
- Discoverable credentials stored on the Flipper
- On-device approval prompts before credential creation or sign-in
- Local software attestation support

## Usage

1. Open ZeroFIDO on your Flipper Zero.
2. Connect the Flipper to your computer over USB.
3. Start a passkey or security-key flow in your browser or app.
4. Confirm the request on the Flipper screen when the site and account look correct.

Keep at least one backup sign-in method for important accounts.

## Security Notes

ZeroFIDO runs on general-purpose Flipper Zero hardware and is not a hardware secure element. Credential private keys are generated on the device and stored wrapped with Flipper crypto APIs, but the device is not equivalent to a certified hardware security key.

ZeroFIDO is experimental software and is not FIDO Alliance certified.
