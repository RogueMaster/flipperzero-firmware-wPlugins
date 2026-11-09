# Release test checklist

This source release has passed static consistency checks and host-side FAT16 tests.
Real-device behavior must still be verified with a build and a Flipper Zero.

- Build on the target firmware SDK: `ufbt -j 1` and inspect `APPCHK`.
- First launch without `drive.img`: the screen displays "Installing disk..." and progress advances and creates a valid 8 MiB FAT16 disk named `BADUSBMSC`.
- Second launch with an existing valid `drive.img`: no "Installing disk..." screen and no creation progress, files preserved.
- A zero-byte image is recreated; a nonempty invalid image is NOT overwritten.
- Home menu shows only BadUSB and Files. BadUSB -> Config still provides Keyboard Layout and connection settings.
- Run a harmless typing test in USB HID mode, then verify Keyboard Layout selection persists.
- Verify the native BadUSB Config opens and USB/BLE selection works while idle.
- Transfer a non-sensitive text file Windows -> BADUSBMSC -> Windows; compare contents.
- Safely eject in Windows before exiting the app. Check files through qFlipper afterward.

Do not treat these items as passed until verified on hardware.
