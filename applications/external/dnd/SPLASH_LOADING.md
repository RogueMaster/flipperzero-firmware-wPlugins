# Randomized loading artwork — 4.20.3

The Hub startup splash, local loading views and DND app handoffs choose from the existing project splash plus fourteen supplied 128×64 monochrome images. They draw the existing animated SDK hourglass over the lower-right corner at `(100, 36)` in a 28×28 frame. Finished artwork is preserved; there is no resizing, dithering or redrawing.

## Choices and file format

| Choice | Raw file | Preserved PNG source |
|---:|---|---|
| 0 | `original.bin` | `original_128x64.png` |
| 1 | `gta.bin` | `dnd_gta_loading_128x64.png` |
| 2 | `gta_v2.bin` | `dnd_gta_loading_v2_128x64.png` |
| 3 | `rogue.bin` | `dnd_gta_loading_rogue_128x64.png` |
| 4 | `wizard.bin` | `dnd_gta_loading_wizard_128x64.png` |
| 5 | `ranger.bin` | `dnd_gta_loading_ranger_128x64.png` |
| 6 | `paladin.bin` | `dnd_gta_loading_paladin_128x64.png` |
| 7 | `cleric.bin` | `dnd_gta_loading_cleric_128x64.png` |
| 8 | `bard.bin` | `dnd_gta_loading_bard_128x64.png` |
| 9 | `artificer.bin` | `dnd_gta_loading_artificer_128x64.png` |
| 10 | `druid.bin` | `dnd_gta_loading_druid_128x64.png` |
| 11 | `monk.bin` | `dnd_gta_loading_monk_128x64.png` |
| 12 | `sorcerer.bin` | `dnd_gta_loading_sorcerer_128x64.png` |
| 13 | `warlock.bin` | `dnd_gta_loading_warlock_128x64.png` |
| 14 | `barbarian.bin` | `dnd_gta_loading_barbarian_128x64.png` |

Each raw file is exactly 1,024 bytes: 64 rows × 16 bytes per row, row-major, least-significant bit first, `1=black`, with no header. This matches `canvas_draw_xbm()` and the supplied firmware's inverted-PNG-to-XBM convention. Source PNGs and both generated bundles are checked in, with SHA-256 hashes in `splash_sources/ASSETS.json`.

The Hub packs `character_assets/loading/`; the loading FAL packs `dnd_loading_assets/splashes/`. Native preload extracts them under `/ext/apps_assets/dndolphins/loading/` and `/ext/apps_assets/dnd_loading/splashes/`. Both copies let Hub startup and a companion's first handoff work independently. Their combined raw SD payload is 30,720 bytes; pack metadata is additional. PNG sources are not packed onto the device.

The supplied build rules mark `.fapassets` as nonresident. Native extraction streams files with a 512-byte copy buffer rather than mapping every bitmap as an ELF icon array. First extraction/update still writes the full bundle to SD; subsequent cached preload and the runtime selected-image read are separate costs. Install the matching FAPs/FALs as described in `FAL_INTEGRATION.md`; no manual PNG install or firmware changes are required.

## Selection and ownership

`dnd_splash_image.c` is linked by the Hub and the loading FAL. The first acquisition chooses an index with `furi_hal_random_get()` and rejection sampling, giving all fifteen choices equal probability. Consecutive sequences may repeat. It allocates one `DndSplashImage` containing a 1,024-byte bitmap, reference count, selected index and loaded flag: 1,028 bytes in the 32-bit layout proxy, excluding allocator/record overhead.

A private `dnd_splash_image_v1` record shares that immutable object across overlapping startup, local-loading and handoff owners. Additional acquisitions increase its reference count without another bitmap allocation, random draw or file read. A loading sequence keeps the same artwork through overlapping views; the next acquisition after the last owner exits makes a new selection.

Acquire/release run only on the serialized DND app thread. GUI/timer callbacks borrow and draw immutable bytes. The record contains no callback or pointer to code, strings or assets in an outgoing FAP/FAL, and the native record implementation copies its name. The bitmap therefore survives the particular module that allocated it being unmapped.

Cleanup first detaches/drains view and animation callbacks, then releases the owner's image reference. The last reference destroys the record and frees the bitmap. A failed handoff can retain one inactive loading cache and the same selected image until a DND app safely reclaims it; this does not load another image. Draw callbacks use `canvas_draw_xbm()` and do no storage reads or allocations.

## Failure behavior

Normal acquisition opens only the selected 1,024-byte file. A missing, short, oversized or failed-read file retries that selection in the other asset directory, then the original in either directory. All attempts reuse the same image buffer. Failed native File opens are closed before retrying the File object.

The shared record is published only after a complete successful read. If neither original copy is usable, or image/File allocation fails, the buffer is released and callers draw `Loading...` with the hourglass. A later loading sequence can retry after the assets or memory become available. Existing FAL descriptor/load/allocation fallback behavior remains in place.

## Build and verification

FBT consumes the committed raw files and needs no Pillow dependency. To reproduce them after changing source images:

```sh
python3 tools/build_splash_assets.py
python3 tools/build_splash_assets.py --check
python3 tests/host/run_tests.py
```

The converter requires finished mode-1 128×64 PNGs. Its independent manual-bit and Pillow-XBM encoders must agree before outputs are written. The host regression exercises every choice, exact file bytes, rejection sampling, overlapping independently mapped FALs, allocating-module unload, last-owner cleanup, read/allocation faults and draw paths without I/O or allocations.

Source/host verification does not certify ARM relocation, native heap high-water, actual SD preload latency or visible zero-desktop/blank-frame timing. Record startup and app handoffs on a Flipper using `DEVICE_TEST_MATRIX.md`. Loading before DND app entry remains controlled by stock firmware.
