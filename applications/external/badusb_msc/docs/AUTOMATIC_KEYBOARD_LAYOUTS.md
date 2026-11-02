# Built-in keyboard layouts: automatic installation

All 55 tables in `keyboard_layouts/` are compiled into `helpers/bundled_keyboard_layouts.h`
using `python tools/generate_embedded_layouts.py`.

At the beginning of **each application launch**, the installer creates
`/ext/badusb/assets/layouts/` if missing, and installs only missing `.kl` files
from its **embedded tables**, writing a `.tmp` file first and renaming it on success.
Existing `.kl` files (including any customized versions) are never overwritten.
The installer runs before the saved layout is loaded and before the existing
disk check or first-time 8 MiB FAT16 creation. Thus layout provisioning is
part of the **same initial setup**, but is **not dependent on first-time disk
creation**: an existing `drive.img` can be reused on every app upgrade.

The previous package used runtime enumeration of `/assets` and could fail to
copy layouts when the bundled assets folder was unavailable. The new installer
does not use `/assets` or `fap_file_assets` and does not need manual qFlipper copying.

## What to test on Flipper

1. Back up the SD card. Install the newly compiled `badusb_msc.fap`.
2. Open BadUSB MSC once; let it finish preparing storage if needed.
3. With qFlipper inspect `SD Card/badusb/assets/layouts/`: there should be
   **55 `.kl` maps**, or more if you already have custom maps.
4. Open BadUSB -> script -> Config -> Keyboard Layout. Check that
   `ro-RO-Legacy.kl` and `ar-SA-101-Legacy.kl` appear.
5. Re-launch app: the maps remain; the image is not recreated.
6. After an app update, newly bundled maps must appear without deleting the
   existing disk or overriding current keyboard selections.

If a map does not appear, inspect app logs for
`Layouts bundled: 55; installed: ...; errors: ...` or
`Cannot install layout:`. The installer cannot detect Windows input locale;
select the matching map manually once.

The eight Windows Legacy maps are ASCII-only and have known coverage limitations;
see `docs/WINDOWS_LEGACY_LIMITATIONS.md`.
