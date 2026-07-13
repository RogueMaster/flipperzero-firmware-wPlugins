<!-- Source: https://docs.flipper.net/zero/nfc -->
<!-- Mirrored: 2026-07-14 -->

# NFC - Flipper Zero Documentation

## NFC

Flipper Zero includes NFC technology support for reading, saving, and emulating NFC cards. The device features "a built-in 13.56 MHz NFC module capable of reading, saving, and emulating NFC cards."

### NFC Menu

The NFC application provides several key functions:

- **Read** — Captures and stores NFC card data including UID, SAK, ATQA, and stored data
- **Extract MF Keys** — Emulates an NFC card to collect cryptographic nonces used for key calculation by readers
- **Saved Lists** — Displays saved NFC cards for emulation and renaming
- **Extra Actions** — Offers additional reading scripts, plugins, and key management tools
- **Add Manually** — Generates new virtual NFC cards through manual data entry

### NFC Hardware

The NFC implementation uses an ST25R3916 NFC chip paired with a 13.56 MHz high-frequency antenna. This antenna is integrated into the device's dual-band RFID antenna, positioned alongside the separate 125 kHz low-frequency antenna. The chip handles high-frequency protocols and manages both card reading and emulation functions.

### Related Topics

The documentation covers additional NFC capabilities including reading cards, recovering MIFARE Classic keys, unlocking password-protected cards, and writing data to magic cards.
