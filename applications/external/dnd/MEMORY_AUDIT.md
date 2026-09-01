# Memory audit

This audit separates values that are exact from project source from values that still require firmware/device measurement.

- **Stack reservation** is exact from `application.fam`.
- **Fixed project app block** and listed record/projection sizes are compiler-checked ARM32 layouts.
- **Project working set** is arithmetic over project-owned app blocks and bounded transient project allocations. Firmware/framework objects, allocator metadata and fragmentation are additional.
- **Source-estimated stack peak** is a conservative source review, not a measured high-water mark. Device instrumentation remains authoritative.

## Per-FAP summary

| FAP | Stack reservation | Source-estimated stack peak | Fixed project app block | Representative bounded project working set |
|---|---:|---:|---:|---|
| DNDolphins | **6,144 B** | **~2,900 B** | **4,940 B** | **7,908 B** Spell/Ritual Combat; **7,668 B** Weapon Combat; **9,716 B** conservative grant/catalog overlap |
| DNDInventory | **4,096 B** | **~2,500 B** | **1,500 B** | **3,884 B** normal 8-Item page; **5,164 B** ordinary sidecar rewrite; **~9,140 B** conservative regrant adapter/rewrite overlap |
| DNDSpellbook | **4,096 B** | **~2,400 B** | **1,456 B** | **4,080 B** normal 8-Spell page; **~8,056 B** page load with transient canonical adapter; **~9,336 B** save/rewrite overlap; **6,224 B** sort-with-page |
| DNDAdventure | **4,096 B** | **~2,300 B** | **856 B** | **1,721 B** with active scene; **3,416 B** diagnostics scan; **~8–9 KB** only during transient Inventory reward bridging |
| DNDJournal | **4,096 B** | **~2,660 B** | **1,352 B** | **2,888 B** during two-buffer index rewrite |
| DNDInitiative | **3,072 B** | **~2,300 B** | **5,276 B** | **6,812 B** during main-character two-buffer profile sync; history save is stack-bounded and adds no resident state |
| DNDBestiary | **6,144 B** | **~4,370 B** | **1,528 B** | **4,108 B** main monster window; **6,368 B** encounter generation |

The projection-based companions intentionally optimize **resident** state first. A few existing shared storage APIs still accept `PocketCharacter`, so Inventory grants, Spellbook page I/O and Adventure Item rewards create a bounded full-character adapter only for that operation. Those adapters are freed before returning and are not embedded in the app state. If hardware measurements show those transient peaks matter, the next optimization should be narrower collection-storage APIs rather than restoring a resident full character.

## Exact current ARM32 project sizes

| Record/state | Size |
|---|---:|
| `PocketSaveData` / `PocketCharacter` | **3,976 B** |
| `PocketItem` | **298 B** |
| `PocketSpell` | **324 B** |
| `PocketFeature` | **234 B** |
| `PocketGrant` | **148 B** |
| `PocketMonsterSummary` | **172 B** |
| `PocketMonsterDetail` | **1,544 B** |
| `PocketMonsterEncounter` | **2,088 B** |
| `DndAdventureScene` | **865 B** |
| `PocketCampaignSummary` | **120 B** |
| `PocketCampaignProgress` | **88 B** |
| `PocketCampaignDiagnostics` | **94 B** |
| `PocketCampaignPackPreview` | **112 B** |
| `PocketBestiaryFilterPreset` | **77 B** |
| `PocketSavedEncounter` | **432 B** |
| `PocketProfileState` | **308 B** |
| `DndInventoryProfileProjection` | **374 B** |
| `DndSpellbookProfileProjection` | **298 B** |
| `DndAdventureProfileProjection` | **72 B** |
| Resident Inventory character/page-owner state | **404 B** |
| Resident Spellbook character/page-owner state | **320 B** |

Small rule/index records include `DndDolphinsSpellClassCounts` 8 B, `PocketAttackRoll` 10 B, `PocketDamageRoll` 92 B, `DndInventoryItemAggregate` 8 B, `DndInventoryCatalogEntry` 52 B and `DndSpellbookCatalogEntry` 56 B.

## Working-set derivation

### DNDolphins

- Fixed app block: **4,940 B**. The bounded level-up review adds 13 one-byte result fields; no progression catalog is retained.
- Combat logical index: **24 B** maximum.
- Visible Combat row cache: 5 × 64 B = **320 B**.
- Item page: 8 × 298 B = **2,384 B**.
- Spell page: 8 × 324 B plus four 8-byte spell-state arrays = **2,624 B**.
- Feature page: 8 × 234 B = **1,872 B**.
- Spell/Ritual Combat: 4,940 + 24 + 320 + 2,624 = **7,908 B**.
- Weapon Combat: 4,940 + 24 + 320 + 2,384 = **7,668 B**.
- Maximum 24 pending Grants: 24 × 148 B = **3,552 B**.
- Current 24-entry character/feat catalog block: **1,224 B**.
- Conservative grant/catalog overlap: 4,940 + 3,552 + 1,224 = **9,716 B**.

Weapon and Spell pages are not intentionally resident together. Combat section headings and level-up review presentation add no dynamic resident list.

### DNDInventory

- Fixed app block: **1,500 B**, reduced from the earlier full-character design by the resident Inventory projection/state.
- Eight-Item page: **2,384 B**.
- Normal resident project blocks: 1,500 + 2,384 = **3,884 B**.
- Ordinary transactional sidecar rewrite line buffer: **1,280 B**, yielding **5,164 B** while the page remains resident.
- Canonical-profile projection scan/rewrite uses one bounded **640 B** heap line, not a full resident character.
- Starting-inventory/regrant compatibility with the existing shared composition API uses one transient **3,976 B** `PocketCharacter`. A conservative regrant overlap with the resident page and 1,280 B collection line is 1,500 + 2,384 + 3,976 + 1,280 = **9,140 B**. The adapter is freed on every success/failure exit.

### DNDSpellbook

- Fixed app block: **1,456 B**.
- Eight-Spell page including four state arrays: **2,624 B**.
- Normal resident project blocks: **4,080 B**.
- The current shared spell-window API still receives a transient **3,976 B** canonical adapter while loading/saving a page. Load overlap: 1,456 + 2,624 + 3,976 = **8,056 B**.
- An ordinary save/rewrite can additionally own the **1,280 B** bounded collection line: **9,336 B** conservative overlap.
- Sorting uses at most 24 compact 36-byte keys = **864 B** plus the 1,280-byte line buffer; sort-with-page: 1,456 + 2,624 + 864 + 1,280 = **6,224 B**.
- Projection scanning itself uses one **640 B** bounded heap line and does not retain the full character.

### DNDAdventure

- Fixed app block: **856 B** with the 72-byte profile projection resident.
- Active scene: **865 B**, for **1,721 B**.
- Diagnostics releases the active scene before scanning. Its 64 × 24-byte scene-ID table is **1,536 B** and two simultaneously reachable 512-byte diagnostic line buffers are heap-owned, for a conservative project block of 856 + 1,536 + 1,024 = **3,416 B**. Moving those line buffers off the 4 KB stack and rendering one result row at a time lowers the source-estimated diagnostics stack peak.
- A 16-entry campaign-pack summary allocation is **1,168 B**, below the diagnostics case.
- Item reward bridging creates a transient 3,976-byte canonical adapter because the shared Inventory append/window API still uses that owner shape. When an Item page/rewrite buffer overlaps, the project peak can enter the **8–9 KB** range; that state is action-local and freed before returning to Adventure.

### DNDJournal

- Fixed app block: **1,352 B**.
- Index rewrite can own two **768 B** heap buffers simultaneously.
- Working set: 1,352 + 768 + 768 = **2,888 B**.

### DNDInitiative

- Fixed app block: **5,276 B**; navigation and opt-in encounter history add no new resident arrays or members.
- Explicit main-character profile synchronization can own two **768 B** heap buffers.
- Working set: 5,276 + 768 + 768 = **6,812 B**.
- Completed-history publication uses bounded path/header/member buffers on stack and one storage `File*`; source review raises the conservative peak to **~2.3 KB**, still below the exact 3 KB reservation. Each record is published atomically and no history index is retained.
- The manifest stack remains intentionally **3 KB**. Five-row roster/setup/combat/editor windows reuse existing selection/scroll fields.

### DNDBestiary

- Fixed app block: **1,528 B**.
- Main monster window: 15 × 172 B = **2,580 B**, for **4,108 B**.
- Encounter generation can own one 2,088-byte encounter plus a 16 × 172 B = **2,752 B** candidate sample.
- Encounter-generation project blocks: 1,528 + 2,088 + 2,752 = **6,368 B**.

## Projection and shared profile/handoff behavior

All seven FAPs link `dnd_profile_handoff.c`. Its active-profile reader uses a fixed 96-byte stack line and does not hydrate a character or collection.

Only Inventory, Spellbook and Adventure link `dnd_profile_projection.c`. It scans the canonical character by field name and leaves the canonical format unchanged. The parser/rewrite line is bounded to **640 B**, matching the canonical encoded-character line bound, and is heap-owned only during the stream operation. Inventory's projection writer patches only Vitals AC plus CombatFlags encumbrance/carry-capacity fields and transactionally publishes the rewritten canonical file. Spellbook and Adventure have no canonical-profile write API.

A failed projection load may invoke the existing backup-restoration path with one transient **3,976 B** `PocketSaveData`; this is recovery-only, not normal resident state.

## Draw-time, allocation and ownership audit

- Project canvas callbacks do **not** allocate project heap, perform storage I/O, hydrate pages, rewrite collections or poll storage.
- Storage/catalog/page/projection work occurs on app/screen entry, explicit input, cache boundaries or writes.
- Combat headings and Initiative visibility calculations use existing scalar state only.
- Collection pages/indexes/scenes/Bestiary blocks, projection lines, compatibility adapters and rewrite buffers have explicit normal/failure release paths.
- Cross-FAP launches quiesce callbacks/timers and release outgoing project-owned state before Loader starts the next FAP.
- No firmware `qsort` dependency is used.

## Hardware validation priorities

1. Measure stack high-water for all seven FAPs under the exact **6/4/4/4/4/3/6 KB** reservations, especially projection save/load paths on the three 4 KB companions.
2. Compare steady-state free heap before/after the projection change; Inventory, Spellbook and Adventure should show the reduced resident blocks above.
3. Stress Inventory initial grant/regrant, Spellbook page load/save/sort, and Adventure Item rewards while watching transient free-heap lows and fragmentation.
4. Repeatedly enter/exit Weapon, Spell and Ritual Combat and confirm indexes/pages are released.
5. Stress Initiative with the maximum roster through wraparound scrolling, edit, reorder, Resume, next-turn and Hold-Up previous-turn navigation.
6. Force projection/allocation/write failures and confirm the canonical profile remains intact and project heap returns to steady state.

A RogueMaster firmware build plus device free-heap, allocator-fragmentation and stack-high-water instrumentation remains the final authority for total runtime memory.
