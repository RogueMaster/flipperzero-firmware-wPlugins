NFC Maker generates Flipper NFC (.nfc) files containing NDEF formatted data of various types.

After saving in NFC Maker, you can find and emulate the file in NFC app > Saved.

Supported data types:
  - Bluetooth MAC
  - Contact Vcard
  - Empty
  - HTTPS Link
  - Mail Address
  - Text Note
  - Plain URL
  - WiFi Login

Supported tag types:
- NTAG 203
- NTAG 213
- NTAG 215
- NTAG 216
- NTAG I2C 1K
- NTAG I2C 2K
- MIFARE Classic Mini 0.3K
- MIFARE Classic 1K
- MIFARE Classic 4K
- SLIX
- SLIX-S
- SLIX-L
- SLIX2

Note that not all NDEF record types are supported on all devices. In particular, iPhones and other iOS devices lack support for many useful types such as Contact Vcards and WiFi Logins. You can find more information [here](https://gist.github.com/equipter/de2d9e421be9af1615e9b9cad4834ddc).
