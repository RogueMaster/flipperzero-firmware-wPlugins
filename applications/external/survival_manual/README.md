# Survival Manual — Flipper Zero app

An offline survival reference reader. Content is the libre **Survival Manual**
(github.com/ligi/SurvivalManual), derived from US Army field manual **FM 21-76**.
37 chapters, 421 sections.

## How it's built

The text is **not** compiled into the FAP — that would blow the flash/heap
(108k words). Instead it ships as SD-card assets and the app is a reader:

```
Pages submenu  ──►  Sections submenu  ──►  TextBox reader
   (37 chapters)      (per-chapter)          (scrollable)
```

Each section is loaded on demand by byte offset from its chapter `.txt`, so RAM
use stays tiny — the largest single section is ~3.5 KB regardless of how big the
chapter is. The built-in `TextBox` widget handles word-wrap and scrolling.

## Files

```
application.fam        manifest (fap_file_assets="assets")
survival_manual.c      the app (single file, ViewDispatcher)
pages.h               auto-generated chapter table (37 entries)
assets/
  <Chapter>.txt        cleaned plain text, one per chapter
  <Chapter>.idx        section index: offset<TAB>length<TAB>title per line
```

## Build (ufbt, Windows PowerShell)

From inside this folder:

```powershell
ufbt            # build the .fap
ufbt launch     # build + upload + run on a connected Flipper
```

`fap_file_assets="assets"` makes ufbt pack the `assets/` folder and deploy it to
`/ext/apps_assets/survival_manual/` on the SD card at install time. The app reads
from there.

### If your firmware doesn't auto-deploy assets

Some setups (or manual `.fap` copying) won't unpack assets. In that case just
copy the `assets/` folder contents to the SD card yourself:

```
/ext/apps_assets/survival_manual/   <- put all .txt and .idx files here
```

(That path is `ASSETS_DIR` at the top of `survival_manual.c` if you ever want to
change it.)

## Regenerating the text

The `assets/` and `pages.h` are generated from the wiki markdown by `clean.py`
and `index.py` (included in the source bundle this came from). Re-run those if
the upstream wiki changes; `MAXCHUNK` in `index.py` controls section size.

## Notes / possible tweaks

- No icon in the manifest (kept it simple). Add `fap_icon="icon.png"` with a
  10x10 1-bit PNG if you want one.
- Tables in the source (a few chapters) are kept as pipe-separated text — they
  wrap rather than render as grids on the 128px screen.
- Figures are replaced with a `[figure]` marker (the Flipper can't show the PNGs
  inline without a lot more work). The images are still in the wiki zip if you
  ever want to convert key ones to 1-bit XBM and embed them.
- `fap_category="Tools"` puts it under Apps > Tools.
