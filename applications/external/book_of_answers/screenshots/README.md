# Screenshots

The Flipper App Catalog requires screenshots **captured with qFlipper** and
committed **unmodified** (original 128×64 resolution and PNG format — do not crop,
scale, or annotate them).

## How to capture

1. Build and install the app: `ufbt launch` (or copy `dist/answer_book.fap` to
   `/ext/apps/Tools/` on the SD card).
2. Open the app on the Flipper: **Apps → Tools → Answer Book**.
3. Connect the Flipper and open **qFlipper**.
4. Use qFlipper's screenshot button (camera icon in the top bar) to capture each
   screen below. qFlipper saves a 128×64 1-bit PNG — commit those files as-is.

## Capture these three screens

| File | Screen | How to get there |
| --- | --- | --- |
| `1.png` | **Cover** (idle) | Launch the app — the framed cover with `Press OK`. |
| `2.png` | **Page** (result) | Press `OK` to open a page; capture the answer page (`No.NNN`, the answer line, the category). |
| `3.png` | **Opening** (optional) | Press `OK` and capture mid page-turn animation. |

At least one screenshot is required; 2–3 make a stronger listing.

After adding the PNGs here, commit and push, then update `commit_sha` in
`catalog/manifest.yml` to the new commit.
