v1.2:

- Added BCC Recover tool to diagnose and repair corrupted UID BCC0/BCC1 bytes on DirectWrite 7-byte MIFARE Ultralight / NTAG magic tags.
- Upgraded home menu with standard 16px row layout, smooth window scrolling, and native Flipper scrollbar indicator.
- Added app icon to the main menu header.
- Optimized NFC polling with instant, responsive back-navigation to the main menu.
- Show possible Gen1 for 28 and 56 block ISO 15693 tags.

v1.1:

- Added saved NFC file selection to UID Changer for MIFARE Classic 1K/4K, ISO 15693, and compatible Ultralight/NTAG cards.
- Import UID, SAK, ATQA, and available Classic block 0 data, with alerts for unsupported or incomplete files.
- Improved Type 2 chip identification in card details.
- Fixed ISO 15693 Gen1 UID writes with the correct unlock sequence, hidden block addresses, and handling of silent commands.
- Added seven-byte UID entry and DirectWrite UID changes for compatible Ultralight/NTAG cards, keeping RF active across UID page writes and verifying the final UID.
- Added Proxmark3-style MFU Gen2 detection and improved MFC Gen2/CUID detection, including seven-byte UID ATS matching.
- Show combined GDM/USCUID and GEN1A capabilities on one line and use consistent GEN1A labels.
- Built and validated against the Flipper Zero 1.5.1 SDK (API 88.2).

v1.0:

- Initial Flipper Zero release of MTools.
- Added Magic Check for supported MIFARE Classic and ISO 15693 magic cards.
- Added UID Changer with manual entry, card reading, and write verification.
- Added card details for ISO 14443-A, ISO 15693, and Ultralight/NTAG cards.
