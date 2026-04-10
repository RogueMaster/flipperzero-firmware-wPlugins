# MFP Reader

A standalone application for reading, dumping and emulating **MIFARE Plus SL3** smart cards on the Flipper Zero. Implements the full MFP SL3 protocol over ISO 14443-4A using only stock firmware APIs — no firmware modifications required.

## Features

- **Card identification** — UID, SAK, ATQA, ATS parsing, manufacturer decoding, BCC validation
- **Full sector dump** — recovers both KeyA and KeyB for every sector using a bundled default dictionary plus any custom dictionary files dropped on the SD card
- **Live visual progress** — sector activity grid that fills in real time as keys are found and blocks are read
- **Hex viewer** — scrollable monospace dump of every block
- **Save / Load** dumps with editable file names and a duplicate-name validator
- **Card emulation** — full MFP SL3 listener implementing GetVersion, Auth (First and NonFirst), ReadEncrypted and WriteEncrypted
- **Two emulation modes** — *Writable* (the dump is updated when the reader writes) or *Read-only* (writes are discarded)
- **Delete saved dumps** from inside the app

## Usage

1. Launch **MFP Reader** from Apps → NFC.
2. Tap **Read card** and place a MIFARE Plus card on the back of the Flipper. The Card Info screen shows the decoded card identity.
3. Press **Dump**. Pick a dictionary file. The scan reads every sector that responds to one of the keys in the dictionary or in the built-in defaults.
4. The result screen shows a sector activity grid:
   - filled square — both KeyA and KeyB recovered
   - top half filled — only KeyA
   - bottom half filled — only KeyB
   - dotted corners — sector unreadable
5. Press **Actions** to **Save** the dump (with an editable file name), open the **View dump** hex viewer, **Emulate** the card, or view **Card Info**.

To re-open a saved dump, use **Saved cards** from the main menu.

## Dictionaries

The app loads dictionary files from the apps_data/mfp_reader folder on the SD card. On first launch it creates mfp_default_keys.dic with about 20 well-known factory and development AES keys, plus a README.txt explaining the format.

To add your own keys, drop additional .dic files into the same folder. They appear in the picker on the next dump.

## Compatibility

- Works on **MIFARE Plus EV1 SL3 2K** cards. The 4K layout is implemented but has not been tested on real hardware.
- Does not work on Plus SL1 (use the built-in MIFARE Classic app instead) or DESFire / Ultralight.
- Built against stock firmware **API 87.1** (Release 1.4.3 and newer Release Candidates).
