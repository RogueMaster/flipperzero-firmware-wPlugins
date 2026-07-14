# NFC QR Presenter

NFC QR Presenter is a Flipper Zero external application for Momentum firmware. It turns the device into a small digital business card presenter: the selected payload is rendered as a QR code on screen and emulated at the same time as an NFC NDEF tag.

The app stores payloads on the SD card and lets you manage them directly from the Flipper UI. It ships without bundled personal or demo payloads.

## Features

- Dual presentation mode: QR code on screen plus NFC NDEF emulation in the background.
- NFC emulation through the firmware NFC stack using a virtual NTAG203 / Mifare Ultralight profile.
- Local payload management from the Flipper: Share, Add, Edit, and Delete.
- SD persistence under `/ext/apps_data/nfc_presenter/`.
- URL, vCard, and plain text payload support.
- QR generation with the lightweight `qrcodegen` C library.

## SD Card Layout

The app creates the data directory and maintains payload pairs:

```text
/ext/apps_data/nfc_presenter/
  <payload>.txt
  <payload>.ndef
```

`*.txt` contains the text used for the QR code. `*.ndef` contains the binary NDEF message used by NFC emulation.

## Build With uFBT

Install or update uFBT:

```bash
python3 -m pip install --upgrade ufbt
```

Configure Momentum firmware SDK:

```bash
ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json --channel=release
```

Build from the repository root:

```bash
ufbt
```

The compiled app is generated at:

```text
dist/nfc_qr_presenter.fap
```

Build, install, and launch on a connected Flipper:

```bash
ufbt launch
```

## Flipper Usage

1. Open `Apps -> Tools -> NFC QR Presenter`.
2. Use `Add` to create a URL, vCard, or short text payload.
3. Select `Share` and choose a payload.
4. Scan the QR code or bring a phone near the Flipper to read the NFC NDEF tag.
5. Press `Back` to stop NFC emulation and return to the menu.
6. Use `Edit` or `Delete` to manage local payloads from the device.

When entering vCard data with the on-screen keyboard, literal `\n` sequences are converted into real line breaks before saving.

## Repository Layout

```text
application.fam              Flipper app manifest
src/                         App source, NFC worker, storage, QR generation
assets/                      Embedded XBM icon used by the Canvas UI
images/                      10x10 catalog/app icon
screenshots/                 Catalog screenshots
docs/catalog_description.md  Catalog-ready short description
docs/changelog.md            Catalog changelog
```

## Validation

This release was validated with:

```bash
ufbt
ufbt launch
```

The hardware smoke test covered payload selection, QR view entry, NFC worker startup, and clean exit with `Back`.

## Third-Party Components

`src/qrcodegen.c` and `src/qrcodegen.h` are from Project Nayuki and retain their upstream MIT license notice in the source files.
