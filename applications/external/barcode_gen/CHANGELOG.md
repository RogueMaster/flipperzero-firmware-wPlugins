# Changelog

## 1.5

- Fix the first save after a fresh install always failing with "A saving error has occurred": the barcodes folder was only created when the app exited; it is now created on startup and rechecked before every save
- Show the actual save error instead of a generic message: empty name or data, a file with this name already exists, SD card not accessible, rename failure
- Fix renaming a barcode to the name of another saved barcode silently overwriting it - it is now rejected with a clear message
- Fix "File Saved!" being reported when the file could not be fully written (e.g. SD card removed mid-save)
- Save and delete failures now return to the edit form with the entered data intact instead of dropping to the main menu

## 1.4

- Add a custom keyboard (by thevan4): symbols can be entered for barcode data, while characters not allowed in file names stay hidden when naming a file
- Update for a newer firmware API (no functional changes)

## 1.3

- Keep the display backlight on while a barcode is shown
- Version bump for catalog compatibility

## 1.2

- Add an "Error Codes Info" screen to the main menu explaining every error the app can show
- Add an About screen
- Fix CODE-128 barcodes containing `#` producing an "Invalid Characters" error
- Fix the CODE-128 check digit encoding lookup for check digit values below 100

## 1.1

- Initial release in this pack: create, edit and display UPC-A, EAN-8, EAN-13, CODE-39, Codabar, CODE-128 (Set B) and CODE-128C barcodes; saved barcodes are stored in `/apps_data/barcodes`
