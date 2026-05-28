# Unbank BTCLN QR Generator

A Flipper Zero app for receiving Bitcoin over the Lightning Network. Generate scannable Lightning Address QR codes for **Speed**, **Strike**, or **Wallet of Satoshi**, with on-device username editing and NDEF tag file export for writing physical NFC stickers.

## Features

- **Multi-wallet support.** Three preset wallets out of the box: Speed, Strike, and Wallet of Satoshi.
- **On-device username editor.** Type your Lightning Address username right on the Flipper using the built-in keyboard. The wallet suffix is appended automatically.
- **Persistent storage.** Usernames are saved to the SD card per wallet and survive reboots and reinstalls.
- **Equal-sized QR codes.** Every QR renders at the same on-screen size regardless of how much data is encoded.
- **Manual refresh.** Tap any QR to regenerate it on demand.
- **Haptic feedback.** A vibration buzz confirms the QR is shown.
- **NFC tag file export.** Generate an NFC tag file for each wallet that the stock Flipper NFC app can write to a blank NTAG213 sticker. When someone taps the sticker with their phone, their Lightning wallet opens with your address pre-filled.
- **Splash screen.** Brand intro on app launch with the Unbank silhouette and app name.
- **Download Unbank QR.** Built-in QR pointing to the Unbank mobile app download page.
- **About screen.** Brief description of Unbank with scrolling text.

## How to use

From the app's main menu, choose **Show QR** to pick a wallet and display its Lightning Address QR code. Use **Edit Username** to set or change your handle for any of the three wallets. **Download Unbank QR** displays a QR that links to the Unbank mobile app. **About** shows a short description of Unbank.

To write your address to an NFC sticker, use the app to generate the tag file, then use the stock Flipper NFC app to write that file to a blank NTAG213.

## Hardware

- Flipper Zero with the latest official firmware. No add-ons required.
- Optional: Blank NTAG213 NFC sticker for the NFC tag export feature.

## License

MIT. The embedded qrcodegen library is by Project Nayuki and is also MIT licensed.
