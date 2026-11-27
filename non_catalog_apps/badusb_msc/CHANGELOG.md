# Changelog

## v0.1 — Initial GitHub source release

- Combined BadUSB script execution with an isolated 8 MiB FAT16 USB Mass Storage drive (`BADUSBMSC`).
- Included the Files browser/editor and restored the BadUSB Config interface with USB/BLE switching.
- Preserved `drive.img` across launches; safely handle zero-byte placeholders and refuse to overwrite nonempty invalid images.
- Display **"Installing disk..."** and byte-based 0–100% progress only during disk preparation, not when reusing a valid image.
- Embedded 55 keyboard layouts and auto-install missing `.kl` files without overwriting user customizations, including the tested `fr-CA-Legacy` map.
- Added Windows file-transfer examples, usage warnings, GPLv3 licensing, and upstream attribution.

### Status

The source archive was inspected and checked for file consistency. The final GitHub presentation change has **not** been compiled or regression-tested on a Flipper in this environment. Test with your firmware before attaching a binary to a public GitHub Release.
