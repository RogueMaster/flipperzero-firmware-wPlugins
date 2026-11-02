# Troubleshooting

If `Storage unavailable` appears, check `SD Card/apps_data/badusb_msc/drive.img`.
The FAT16 image must be exactly 8,388,608 bytes and have volume label `BADUSBMSC`.
A zero-byte image is safely regenerated on the next app launch; larger damaged images are preserved.
Back up an invalid image with qFlipper before removing it and relaunching the app.
Do not interrupt the first 8 MiB creation, and eject the disk in Windows before closing the app.

If scripts type wrong characters, select the keyboard layout that matches the host PC in BadUSB → Config → Keyboard Layout. The FAP installs missing bundled `.kl` layouts automatically at startup. If no layouts are listed, check SD-card access and confirm the `.fap` was built from sources containing `fap_file_assets`.
If a script cannot be opened while the disk is being prepared, retry once preparation is finished.

## Keyboard layout verification

Keyboard layout files must be readable 256-byte `.kl` maps, normally found in
`SD Card/badusb/assets/layouts/`. BadUSB MSC now verifies the selected mapping
with the same reader used by its script engine. If the file is missing or
truncated, a non-US selection is rejected instead of silently typing with US
keys. The displayed layout reflects the loaded selection; the PC must also use
a matching keyboard layout for special characters to be correct.
