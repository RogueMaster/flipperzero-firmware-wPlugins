<h1 align="center">Barcode + QR Generator</h1>

A Flipper Zero app for creating, saving, editing, and displaying **UPC-A**, **EAN-8**, **EAN-13**, **Code 39**, **Codabar**, **Code 128**, **Code 128C**, and compact **QR Code** symbols.

## Version 2.0 highlights

- Added compact QR Code generation with a full printable-ASCII keyboard.
- Fixed restricted keyboards becoming unresponsive when focus landed on a hidden key.
- Added format-specific keyboards, validation, and helpful input hints.
- Fixed validation-overlay navigation and a one-byte text-rendering buffer overflow.
- Preserved the original project attribution and added AzureSky's QR and keyboard revisions.

## Installing

Install the app package through your firmware's app catalog when available. For manual installation:

1. Copy `barcode_app.fap` to the SD card's `apps/Tools` folder.
2. Create `apps_data/barcode_data` if it does not already exist.
3. Copy these files from `barcode_encoding_files` into `apps_data/barcode_data`:
   - `code39_encodings.txt`
   - `code128_encodings.txt`
   - `code128c_encodings.txt`
   - `codabar_encodings.txt`

Saved barcode and QR entries are stored in `/apps_data/barcodes`.

## Building

The simplest build method is [ufbt](https://github.com/flipperdevices/flipperzero-ufbt):

```shell
ufbt
```

The built app is written to `dist/barcode_app.fap`.

For a firmware-tree build, place this repository in `applications_user`, then run:

```shell
./fbt fap_barcode_app
```

## Usage

### Create

1. Select `Create Barcode`.
2. Choose a format with the left and right buttons.
3. Enter a filename and the barcode or QR data.
4. Select `Save`.

The data keyboard automatically exposes only characters supported by the selected format:

- **UPC-A:** 11 or 12 digits.
- **EAN-8:** 7 or 8 digits.
- **EAN-13:** 12 or 13 digits.
- **Code 39:** `A-Z`, `0-9`, space, `- . $ / + %`; outer `*` guards are optional.
- **Code 128 (Set B):** printable ASCII.
- **Code 128C:** digits; grouping spaces/hyphens are ignored and odd digit counts receive a leading zero.
- **Codabar:** `0-9`, `- $ : / . +`, with optional `A-D` start/stop guards.
- **QR Code:** printable ASCII. Version 1 is used for screen readability: up to 41 digits, 25 QR-alphanumeric characters, or 17 general ASCII characters.

For UPC-A and EAN formats, the shorter accepted length receives a calculated check digit. Grouping spaces and hyphens are ignored.

### Edit or delete

Select `Edit Barcode`, choose a saved entry, then change its format, name, or data. Scroll to the bottom of the form to delete it.

### View

Select `Load Barcode`, then choose a saved entry.

## Screenshots

![Barcode Create Screen](screenshots/Creating%20Barcode.png "Barcode Create Screen")

![Flipper Code-128 Barcode](screenshots/Flipper%20Barcode.png "Flipper Code-128 Barcode")

![Flipper Box EAN-13 Barcode](screenshots/Flipper%20Box%20Barcode.png "Flipper Box EAN-13 Barcode")

![Flipper QR Code](screenshots/Flipper%20QR%20Code.png "Flipper QR Code")

## Credits

- [Kingal1337](https://github.com/Kingal1337) - Original developer
- [AzureSky](https://github.com/AzzureSkyy/flipper-barcode-generator) - QR Code support and keyboard revisions
- [Z0wl](https://github.com/Z0wl) - Code 128C support
- [teeebor](https://github.com/teeebor) - Menu code snippet
- [thevan4](https://github.com/thevan4) - Custom keyboard
- [Project Nayuki](https://www.nayuki.io/page/qr-code-generator-library) - MIT-licensed QR Code generator library
