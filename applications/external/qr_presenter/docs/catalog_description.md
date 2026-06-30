# NFC QR Presenter

NFC QR Presenter turns Flipper Zero into a compact digital business card presenter. It stores local payloads on the SD card, renders the selected payload as a QR code, and emulates the same content as an NFC NDEF tag using a virtual NTAG profile.

Main features:

- QR presentation through native Canvas drawing.
- NFC NDEF emulation through the firmware NFC stack.
- Local payload management from the device: share, add, edit, and delete.
- SD card storage under /ext/apps_data/nfc_presenter/ using paired TXT and NDEF files.
- No bundled personal or demo payloads; payloads are created by the user on device.

The app is designed for quick sharing of URLs, vCards, and short text payloads during events, meetings, and demos.
