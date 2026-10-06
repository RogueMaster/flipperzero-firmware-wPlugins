# Memory and performance audit — 4.20.3

The loading pool now has fifteen images, but one loading sequence keeps only one selected 1,024-byte bitmap in RAM. A shared heap record avoids duplicate buffers when Hub startup and separately mapped local/handoff loading FAL instances overlap. This adds one 1,028-byte project image object plus native allocator/record overhead. The prior feature FAL ownership and Inventory behavior remain in place. Device peak heap, ARM size and loading latency have not been measured.

## Comparable host code and constants

Both 4.20.2 and 4.20.3 were compiled with GCC 13.3.0, `-Os -fPIC`, entry-rooted section GC and the same current host shim. Totals sum `.text`, `.rodata` and `.data.rel.ro`. FAP totals include common shim code. FAL totals contain their own source, with a zero-data placeholder only for the old 4.20.2 generated splash icon. That placeholder does not measure native compressed graphics residency. Current raw file assets are excluded from resident section totals. These x86_64 figures are code-ownership proxies, not native FAP/FAL sizes.

| Target | Type | 4.20.2 bytes | 4.20.3 bytes | Change |
|---|---|---:|---:|---:|
| `dndolphins` | FAP | 137,266 | 138,706 | +1,440 |
| `dndcharactersheet` | FAP | 30,490 | 30,490 | +0 |
| `dndcombat` | FAP | 111,770 | 111,770 | +0 |
| `dndgrants` | FAP | 117,450 | 117,450 | +0 |
| `dndinventory` | FAP | 93,194 | 93,194 | +0 |
| `dndspellbook` | FAP | 74,546 | 74,546 | +0 |
| `dndadventure` | FAP | 70,298 | 70,298 | +0 |
| `dndjournal` | FAP | 18,586 | 18,586 | +0 |
| `dndinitiative` | FAP | 44,514 | 44,514 | +0 |
| `dndbestiary` | FAP | 81,450 | 81,450 | +0 |
| `dndbackup` | FAP | 54,218 | 54,218 | +0 |
| `dnd_character_sheet` | FAL | 7,339 | 7,339 | +0 |
| `dnd_journal` | FAL | 23,200 | 23,200 | +0 |
| `dnd_monster_turn` | FAL | 16,181 | 16,181 | +0 |
| `dnd_spell_damage` | FAL | 9,293 | 9,293 | +0 |
| `dnd_loading` | FAL | 2,070 | 3,574 | +1,504 |

The Hub adds 1,440 B and the loading FAL adds 1,504 B of host code/constants for selection, file reads and shared ownership. The other ten FAPs and four feature FALs are unchanged in this fresh comparison. Earlier feature conversion figures remain in `MEMORY_AUDIT_4.20.2.md`; its old totals use an earlier shim and should not be compared directly with these new totals.

## Bitmap storage and lifetime

The fifteen raw bitmap files total 15,360 bytes per SD directory. Two bundles contribute 30,720 bytes of raw SD payload plus metadata: Hub assets under `/ext/apps_assets/dndolphins/loading/` and loading-FAL assets under `/ext/apps_assets/dnd_loading/splashes/`. Source PNGs are preserved for maintenance and are not bundled onto the device.

The native builder marks `.fapassets` as nonresident. Supplied preload extracts files with a 512-byte streaming copy buffer rather than allocating all image arrays. First extraction/update writes all files to SD; cached preload and the runtime selected-image read are separate costs. Normal acquisition reads one selected 1,024-byte image. Missing/corrupt images can cause retries in the alternate directory and then the original, all in the same buffer.

The 1,028-byte image object contains one bitmap and four bytes of metadata. Overlapping owners increment references without allocating or reading another bitmap. Its heap data can outlive the allocating module; callbacks only borrow it. The last owner releases it after view/timer teardown. Failed handoffs can retain this same image alongside the one bounded inactive FAL cache until the next DND readiness/handoff. No framebuffer, native File object, animation, record or ELF allocator overhead is included in the object-size claim. Draws perform no file I/O or allocations. See `SPLASH_LOADING.md` for the full selection/fallback contract.

## Current 32-bit layout proxy

`tests/host/layout32.py` uses a freestanding 32-bit pointer ABI. Values exclude native View/dispatcher/timer/ELF allocator objects and dynamic collection storage.

| State/record | Bytes |
|---|---:|
| `DndDolphinsApp` | 3,416 |
| `DndCharacter` | 3,148 |
| `DndCatalogRuntime` | 48 |
| `DndCollectionCacheRuntime` | 64 |
| `DndRollRuntime` | 76 |
| `DndGrantReviewRuntime` | 24 |
| `DndCombatRuntime` | 256 |
| `DndProfileState` | 308 |
| `DndInventoryCollectionApp` | 1,716 |
| `DndSpellbookCollectionApp` | 1,808 |
| `BestiaryApp` | 1,536 |
| `InitiativeApp` | 5,368 |
| `JournalApp` | 1,396 |
| `DndCharacterSheetApp` | 20 |
| `DndMonsterTurnApp` | 208 |
| `DndMonsterDetail` | 1,544 |
| `DndLoading` | 12 |
| `DndLoadingHandoff` | 36 |
| `DndLoadingTransfer` | 148 |
| `DndSplash` | 24 |
| `DndSplashImage` | 1,028 |
| `DndPlugin` | 8 |
| `DndPluginLoading` | 20 |

Common Hub state remains 3,416 B and Inventory remains 1,716 B. Local loading and handoff contexts each gain one four-byte image pointer, increasing from 8/32 B to 12/36 B. Hub splash state increases from 20 to 24 B. The transfer stays 148 B. One shared image object is added regardless of overlapping owner count.

The retained feature design still borrows a character for the 20-byte Sheet state; its standalone wrapper owns a separate 3,148-byte character. Journal has a 1,396-byte state plus optional entry/search/editor storage. Initiative owns a 1,544-byte detail only during a name-resolved tools session; Bestiary borrows its existing detail. Combat keeps its 256-byte lazy runtime. These project layouts alone do not establish peak RAM.

## Stack reservations and live individual host frames

| Target | Reserved stack bytes | Largest live project host frame bytes |
|---|---:|---:|
| `dndolphins` | 6,144 | 2,192 |
| `dndcharactersheet` | 4,096 | 1,168 |
| `dndcombat` | 6,144 | 1,744 |
| `dndgrants` | 6,144 | 1,744 |
| `dndinventory` | 4,096 | 1,632 |
| `dndspellbook` | 4,096 | 1,168 |
| `dndadventure` | 4,096 | 1,264 |
| `dndjournal` | 4,096 | 272 |
| `dndinitiative` | 6,144 | 1,584 |
| `dndbestiary` | 6,144 | 2,320 |
| `dndbackup` | 4,096 | 1,184 |
| `dnd_character_sheet` | 0 | 304 |
| `dnd_journal` | 0 | 1,280 |
| `dnd_monster_turn` | 0 | 1,600 |
| `dnd_spell_damage` | 0 | 80 |
| `dnd_loading` | 0 | 192 |

The loading FAL's largest surviving host frame increases from 32 to 192 B for the selected-file path/read helper. Reservations and other largest project frames are retained. These maxima include only functions surviving the entry-rooted link, not cumulative call chains or ARM inlining. A FAL's zero reservation means it uses caller/GUI/timer/event threads; it still consumes stack. Measure native high-water, including the supplied 2 KiB GUI and Loader stacks, on hardware.

## Coexistence and device gates

- Integrated Sheet/Journal keeps the Hub resident. Standalone Journal has a smaller parent executable proxy; integrated use can increase peak coexistence. Do not treat the feature conversion as a universal OOM or latency improvement.
- Combat retains its damage module while spell workflows need it and frees it outside those screens. Casting still needs the table and ELF overhead.
- Bestiary/Initiative tools share code but can coexist with duplicated backend code in a parent. Borrowing the detail avoids another record allocation.
- Handoff retains a mapped loading FAL, the 36-byte context and 148-byte transfer plus one shared image and native framework objects. An independently mapped local loading module can briefly overlap; its code residency is separate, but its bitmap is shared. Loader barriers separate app-side map/free from native startup/unload work.
- Measure first/repeat SD extraction and selected-file read time, contiguous/peak heap, cumulative stack, visible desktop/blank frames, held inputs and all missing/invalid/OOM paths on the intended firmware. Twenty host regressions and source-contract checks do not replace these device gates.

Reproduce current sizes with `python3 tests/host/measure_host.py`. For the comparison use that script with `--source-root /path/to/4.20.2 --output-root /absolute/comparison/path`. Current evidence is in `tests/host/host_fap_fal_sizes.json`, `baseline_4202/host_fap_fal_sizes.json`, `stack_frames_host.json` and `layout32.json`. Historical audits are preserved as `MEMORY_AUDIT_4.20.2.md` and `MEMORY_AUDIT_4.19.1.md`.
