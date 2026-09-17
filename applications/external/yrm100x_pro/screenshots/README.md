# Catalog screenshots

The Flipper Application Catalog requires at least one original screenshot made
through qFlipper. Do not crop, resize, sharpen, recolor, annotate, re-encode, or
reconstruct the captured image. The first file in the catalog manifest becomes
the application preview.

## Recommended capture set

1. **01-main-menu.png** — clean main menu with YRM100X_PRO and free RAM visible.
2. **02-antenna-info.png** — connected reader hardware, firmware, and vendor.
3. **03-read-multi.png** — a successful Full Multi result with EPC/PC/RSSI visible.
4. **04-read-forever.png** — stopped Read Forever history with Prev, Next, More,
   and the X/Y counter visible.
5. **05-saved-dumps.png** — the paged Saved Dumps list. Use neutral test names
   and avoid publishing sensitive production EPC values.
6. **06-clone-options.png** — Clone or Clone_PC3400 bank selection with the
   distinct [ START CLONE ] row visible.
7. **07-bank-info.png** — a completed Get Size Bank result.
8. **08-configure.png** — Configure list showing representative settings.

Five good screenshots are sufficient for a compact submission; all eight give
users a clearer catalog preview. Always keep **01-main-menu.png** first.

## Capture procedure

1. Install and launch the exact release-candidate FAP that will be submitted.
2. Open qFlipper Remote Control and navigate to the required application screen.
3. Use qFlipper's screenshot function and save the file directly as PNG.
4. Rename the file only; do not open and resave it in an image editor.
5. Place the PNG in this directory and verify that no private EPC, password,
   device name, or unrelated notification is visible.
6. Repeat the capture if the screen contains a transient error, clipped label,
   stale version, or unintended tag value.

The screenshot files referenced by the catalog manifest must exist in the exact
source commit named by that manifest.
