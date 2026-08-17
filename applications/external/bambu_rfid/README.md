# Bambu RFID for Flipper Zero

A Flipper Zero external app for reading, exporting, browsing, and inspecting Bambu Lab filament RFID tags.

## Features

- Reads the 4-byte tag UID using ISO14443-3A anticollision.
- Derives all 16 MIFARE Classic sector A-keys and all 16 sector B-keys directly from the UID.
- Reads all 64 MIFARE Classic 1K blocks without a pre-populated user dictionary, using the derived sector A/B keys.
- Saves every successful scan as a Bambu-Lab-RFID-Library-compatible five-file bundle in a UID folder.
- Browses saved UID folders as one tag per folder.
- Retains backward compatibility with legacy flat Flipper `.nfc` files and 1024-byte raw `.bin` dumps in the tags directory.
- Displays decoded Bambu fields: material and variant IDs, filament type, primary/secondary color, spool weight and width, filament diameter and length, hotend/bed/drying temperatures, nozzle diameter, production date, tray UID, X-Cam bytes, and known/unknown auxiliary fields.

## Saved tag bundle

Each successful scan is written to:

```text
/ext/apps_data/bambu_rfid/tags/<UID>/
├── hf-mf-<UID>-dump.bin
├── hf-mf-<UID>-dump.json
├── hf-mf-<UID>-key.bin
├── hf-mf-<UID>.nfc
└── README.md
```

The four RFID files follow the same conventions used by `queengooborg/Bambu-Lab-RFID-Library/convert.py`:

- `hf-mf-<UID>-dump.bin` — exactly 1024 bytes: blocks 0 through 63 concatenated in order.
- `hf-mf-<UID>-dump.json` — Proxmark-style `mfc v2` JSON containing card data, all blocks, sector keys, access bytes, and decoded access-condition text.
- `hf-mf-<UID>-key.bin` — exactly 192 bytes: all 16 six-byte Key A values first, followed by all 16 six-byte Key B values, extracted from the sector trailers.
- `hf-mf-<UID>.nfc` — Flipper NFC device Version 4 / MIFARE Classic 1K text format with all 64 blocks.
- `README.md` — human-readable parsed information for that spool/tag.

The `.nfc` export reverses the two ATQA bytes as required by the Flipper file convention, matching the library converter.

## Key derivation

The app implements the public derivation documented by Bambu Research Group:

- IKM: the 4-byte tag UID
- HKDF-SHA256 salt: `9A759CF2C4F7CAFF222CB9769B41BC96`
- Key A info/context: `RFID-A\0`
- Key B info/context: `RFID-B\0`
- output per context: 96 bytes, split into sixteen 6-byte MIFARE Classic keys

The implementation uses mbedTLS HMAC-SHA256, which is already available to Flipper external apps through `fap_libs=["mbedtls"]`.

## Data layout

The parser follows the documented Bambu blocks:

- Block 1: variant ID / material ID
- Block 2: filament type
- Block 4: detailed filament type
- Block 5: RGBA color, spool weight, diameter
- Block 6: drying/bed/hotend settings
- Block 8: X-Cam data and nozzle diameter
- Block 9: tray UID
- Block 10: spool width
- Blocks 12-13: production timestamps
- Block 14: filament length
- Block 16: multi-color metadata
- Block 17: currently-unknown two-byte field
- Sectors 10-15: RSA signature bytes are retained in the saved dump even though the app does not attempt to validate the signature.

## Building

### GitHub Actions

Push the project to GitHub. The included workflow builds against both the Flipper release and dev SDK channels and uploads `.fap` artifacts. Tags matching `vX.Y.Z` publish the release-channel FAP as a GitHub release.

### Local uFBT

With uFBT installed:

```sh
ufbt
```

The resulting FAP can be copied to the NFC apps directory on the Flipper SD card.

## Installing / using

1. Install the built `bambu_rfid.fap` under `apps/NFC` on the Flipper SD card.
2. Launch **Bambu RFID**.
3. Choose **Scan tag** and hold the spool RFID tag against the Flipper.
4. Keep the tag steady while all 16 sectors are read.
5. The five-file bundle is saved automatically under `/ext/apps_data/bambu_rfid/tags/<UID>/`.
6. Choose **Browse scans** to inspect previously saved tags.

To inspect existing dumps, either copy a library-style UID folder under `/ext/apps_data/bambu_rfid/tags/`, or place a legacy Flipper `.nfc` / raw 1024-byte `.bin` file directly in that directory.

## References

This implementation was written from the public research and examples in:

- `Bambu-Research-Group/RFID-Tag-Guide` — KDF and tag memory documentation.
- `queengooborg/Bambu-Lab-RFID-Library` — real-world dumps and canonical format conversion logic.
- `DanTheMan827/AmiiboZero` — ViewDispatcher/custom-view UI structure and GitHub uFBT workflow pattern.
- `flipperdevices/flipperzero-firmware` — public NFC/MIFARE Classic and storage APIs.
- `RfidResearchGroup/proxmark3` — Bambu `RFID-A\0` / `RFID-B\0` UID key-generation implementation.

## Notes

- Bambu tags contain an RSA signature. Reading a tag does not give the private signing key and this app does not attempt to forge signatures.
- The Flipper screen is monochrome, so colors are displayed as RGBA hex codes rather than rendered literally.
- The Bambu KDF derives both sector key types using the `RFID-A\0` and `RFID-B\0` contexts. The app supplies both sets during the read and still verifies that all 64 blocks were actually read before exporting.
- Classic sector key bytes are represented in the Flipper read structure from the derived keys; the exported trailer blocks therefore contain the same A/B key layout used by the reference library dumps.
- The reader requires all 64 blocks before saving. If the tag moves during the operation, retry while holding it steady.
