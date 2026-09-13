# Screenshots for the Flipper App Catalog

The catalog requires **real screenshots captured on a Flipper Zero**, not
mockups. They must be PNG files taken with qFlipper (or `ufbt cli` screenshot),
left at their original resolution and format.

## How to capture

1. Install the app on the Flipper (`ufbt launch`, or copy the `.fap` to
   `apps/Tools/`).
2. Open qFlipper, navigate the app to the screen you want, and use qFlipper's
   screenshot button (or press the screenshot control) to save a PNG.
3. Save the files here with these names, matching `catalog/manifest.yml`:
   - `menu.png` - the main menu (used as the catalog preview)
   - `work.png` - Work mode clock / greeting
   - `today.png` - the Today summary

Style mockups of these screens (SVG) live in [../docs/img](../docs/img) for the
README, but they cannot be used for the catalog submission.
