# Development preview

Normal builds have screenshot capture disabled. To enable it temporarily, set
HAM_ENABLE_PREVIEW to 1 in ham/build_config.h and rebuild with uFBT. Set it back
to 0 and rebuild before creating a release. This is a build-time switch, not an
on-device setting. Preview state, framebuffer storage, launch argument parsing,
and capture-file output are excluded from normal builds by the preprocessor.

The capture mode remains in the source because HAM takes over USB HID while open,
preventing qFlipper from capturing its normal screen. This opt-in path supports the
catalog's explicit HID screenshot exception without adding capture code to release
builds.

With a developer build installed, these launch arguments render the actual device
canvas without changing the USB interface or sending keyboard/mouse input:

- preview: paused screen.
- preview-infinite: paused screen with unlimited duration selected.
- preview-running-a and preview-running-b: fixed poses for the running animation.

Each preview exits after about 800 ms and saves its framebuffer to
/ext/apps_data/ham/preview.bin. Render-only running previews do not tick the engine
or send HID reports. Preview settings are never saved to the normal profile.

# Catalog screenshots

The catalog permits alternate screenshot capture for apps that take over USB HID.
Its required image format is 512x256, with only orange (254,138,44) and black
(0,0,0). See the HID exception in the
[catalog instructions](https://github.com/flipperdevices/flipper-application-catalog/blob/main/AGENTS.md).

The source PNGs in docs are lossless 4x renders of framebuffers captured on the
physical Flipper. They use a presentation palette for the README. The preparation
script verifies that each 4x pixel block is uniform, recovers the original 1-bit
pixels, then maps pixel-on to catalog orange and pixel-off to black. It adds no
annotations, borders, antialiasing, or changes to the screen content.

To regenerate the catalog PNGs, install Pillow in your Python environment and run:

```sh
python scripts/prepare_screenshots.py
```

Use ham/catalog/description.md and ham/catalog/changelog.md for the catalog text.
They deliberately avoid the code blocks and images used in the main README.
The catalog source subdirectory will be ham, making the corresponding metadata
paths catalog/description.md, catalog/changelog.md, and catalog/screenshots/.

Keep ham/LICENSE identical to the root LICENSE. The catalog bundles only the ham
source subdirectory, so it needs its own copy of the license notice.

# Settings recovery

Profile saves write, sync, and verify profile.txt.tmp before touching the current
settings. A verified profile.txt.bak and profile.txt.pending marker protect the
replacement because Flipper's rename operation is implemented as copy/delete.
If replacement fails or is interrupted, the backup remains authoritative until a
verified recovery succeeds. Incomplete backups without a pending marker are ignored.
Failed saves display a storage error after HID inputs are released and USB restored.

Run the storage fault tests from the project root:

```sh
cc -O2 -std=c11 -Wall -Wextra -Werror -I tests/stubs \
  tests/profile_store_test.c ham/profile_store.c -o /tmp/ham-profile-test
/tmp/ham-profile-test
```
