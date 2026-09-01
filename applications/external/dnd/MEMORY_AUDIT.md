# Memory audit

This document separates values that can be calculated exactly from project source from values that require firmware/device measurement.

- **Stack reservation** is exact from `application.fam`.
- **Fixed app block** and listed record/page sizes are exact for the current project structs under ARM32 alignment.
- **Project working set** is arithmetic over project-owned app blocks and bounded transient allocations. It excludes firmware/framework objects, `File`/GUI internals, allocator metadata and fragmentation.
- **Source-estimated stack peak** is a conservative call-path estimate from local arrays/frames. It is not a measured high-water mark; device stack instrumentation is authoritative.

## Per-FAP summary

| FAP | Stack reservation | Source-estimated stack peak | Fixed project app block | Representative bounded project working set |
|---|---:|---:|---:|---|
| DNDolphins | **6,144 B** | **~2,900 B** | **4,924 B** | **7,892 B** Spell/Ritual Combat; **7,652 B** Weapon Combat; **9,700 B** conservative grant/catalog overlap |
| DNDInventory | **4,096 B** | **~2,000 B** | **5,072 B** | **7,456 B** normal 8-Item page; **8,736 B** during bounded rewrite |
| DNDSpellbook | **4,096 B** | **~2,000 B** | **5,112 B** | **7,736 B** normal 8-Spell page; **9,016 B** ordinary rewrite; **9,880 B** sort-with-page |
| DNDAdventure | **4,096 B** | **~2,770 B** | **4,760 B** | **5,625 B** with active scene; **6,296 B** diagnostics scene-ID table |
| DNDJournal | **4,096 B** | **~2,660 B** | **1,352 B** | **2,888 B** during two-buffer index rewrite |
| DNDInitiative | **3,072 B** | **~2,000 B** | **5,276 B** | **6,812 B** during main-character two-buffer profile sync |
| DNDBestiary | **6,144 B** | **~4,370 B** | **1,528 B** | **4,108 B** main monster window; **6,368 B** encounter generation |

Approximate source-estimated stack headroom is therefore ~3,244 B, 2,096 B, 2,096 B, 1,326 B, 1,436 B, 1,072 B and 1,774 B respectively. Do not shrink a manifest stack solely from these estimates; verify high-water on hardware first.

## Exact current record sizes

ARM32 project layout used by the working-set arithmetic:

| Record | Size |
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

Small rule/index records used in bounded paths include `DndDolphinsSpellClassCounts` 8 B, `PocketAttackRoll` 10 B, `PocketDamageRoll` 92 B, `DndInventoryItemAggregate` 8 B, `DndInventoryCatalogEntry` 52 B and `DndSpellbookCatalogEntry` 56 B.

## Working-set derivation

### DNDolphins

- Fixed app block: **4,924 B**.
- Combat logical index: **24 B** maximum.
- Visible Combat row cache: 5 × 64 B = **320 B**; it is allocated with the active logical index rather than embedded in the fixed app block.
- Item page: 8 × 298 B = **2,384 B**.
- Spell page: 8 × 324 B plus four 8-byte spell-state arrays = **2,624 B**.
- Feature page: 8 × 234 B = **1,872 B**.
- Spell/Ritual Combat: 4,924 + 24 + 320 + 2,624 = **7,892 B**.
- Weapon Combat: 4,924 + 24 + 320 + 2,384 = **7,652 B**.
- Maximum 24 pending Grants: 24 × 148 B = **3,552 B**.
- Current 24-entry character/feat catalog block: **1,224 B**.
- Conservative grant/catalog overlap: 4,924 + 3,552 + 1,224 = **9,700 B**.

Weapon and Spell pages are not intentionally resident together. Combat pages/indexes are created on entry/input paths and released when leaving that combat family.

### DNDInventory

- Fixed app block: **5,072 B**.
- Eight-Item page: **2,384 B**.
- Normal project blocks: **7,456 B**.
- Transactional sidecar rewrite line buffer: **1,280 B** heap.
- Rewrite working set: **8,736 B**.

The initial-inventory grant streams assets and publishes Items, combined Currency and grant marker in one synced sidecar transaction. It does not require a second full collection in RAM.

### DNDSpellbook

- Fixed app block: **5,112 B**.
- Eight-Spell page including state arrays: **2,624 B**.
- Normal project blocks: **7,736 B**.
- Ordinary bounded rewrite adds **1,280 B**, for **9,016 B**.
- Sorting uses at most 24 compact 36-byte keys = **864 B** plus the 1,280-byte line buffer.
- Conservative sort-with-page working set: 5,112 + 2,624 + 864 + 1,280 = **9,880 B**.

Sorting is owned by `dndspellbook_collection.c`; it does not retain a second full Spell collection.

### DNDAdventure

- Fixed app block: **4,760 B**.
- Active scene: **865 B**, for **5,625 B**.
- Largest documented diagnostics table: 64 × 24-byte scene IDs = **1,536 B**, for **6,296 B** with the fixed app block.
- A 16-entry campaign-pack summary allocation is **1,168 B**, below the diagnostics-table case.

### DNDJournal

- Fixed app block: **1,352 B**.
- Index rewrite can own two **768 B** heap buffers simultaneously.
- Working set for that path: 1,352 + 768 + 768 = **2,888 B**.

Journal keeps bounded metadata/index state and loads entry bodies on demand.

### DNDInitiative

- Fixed app block: **5,276 B**.
- Explicit main-character profile synchronization can own two **768 B** heap buffers.
- Working set for that path: 5,276 + 768 + 768 = **6,812 B**.

The manifest stack reservation is intentionally **3 KB**, not 4 KB. Feature-recharge parsing has large local line buffers but does not keep a Feature collection resident.

### DNDBestiary

- Fixed app block: **1,528 B**.
- Main monster window: 15 × 172 B = **2,580 B**, for **4,108 B**.
- Encounter generation can own one 2,088-byte encounter plus a 16 × 172 B = **2,752 B** candidate sample.
- Encounter-generation project blocks: 1,528 + 2,088 + 2,752 = **6,368 B**.

Bestiary's **6 KB** stack reservation reflects its deeper streaming/parser paths and repeated 768-byte local reader buffers; this is not a candidate for reduction without measured high-water data.

## Shared profile/handoff memory behavior

All seven FAPs link `dnd_profile_handoff.c`. Its active-profile reader uses a fixed **96-byte stack line buffer** and a firmware `File` object; it does not scan for another character, hydrate a collection or allocate a project-owned heap buffer. Exact-profile validation/path lookup uses bounded stack path/name buffers and directory iteration.

`dnd_storage.c` remains responsible for full character/profile parsing and transactional character/sidecar persistence where linked. It no longer carries a second `Active=` metadata parser.

## Draw-time and ownership audit

- Project canvas callbacks do **not** allocate project heap, perform storage I/O, hydrate pages, rewrite collections or poll storage.
- Storage/catalog/page work occurs on app/screen entry, explicit input, cache boundaries or writes.
- Dynamic pages/indexes/scenes/Bestiary blocks and rewrite buffers have matching normal/failure release paths in the source ownership audit.
- Cross-FAP launches quiesce callbacks/timers and release outgoing project-owned state before Loader starts the next FAP.
- Inventory/Spellbook reserve their fixed dispatcher/main-view state before variable character/collection loading.
- No firmware `qsort` dependency is used.

The largest remaining companion optimization opportunity is profile projection: Inventory, Spellbook and Adventure each currently embed the **3,976 B** full character core. A narrower read/write projection could reduce heap, but it requires a separate storage/API change and hardware regression pass.

## Hardware validation priorities

1. Measure stack high-water for all seven FAPs under their exact 6/4/4/4/4/3/6 KB reservations.
2. Track Loader free heap and fragmentation across repeated DNDolphins ↔ Inventory/Spellbook/Bestiary/Initiative/Journal/Adventure handoffs.
3. Repeatedly enter/exit Weapon, Spell and Ritual Combat and confirm index/page blocks are released.
4. Stress Inventory/Spellbook 8-record boundaries plus transactional rewrites and Spellbook sorting.
5. Stress Adventure diagnostics/large packs, Initiative feature recharge/profile sync and Bestiary encounter generation/detail/pack parsing.
6. Force allocation/write failures and confirm project blocks return to the same steady-state range after the failing screen/action exits.

A RogueMaster firmware build plus device free-heap, allocator-fragmentation and stack-high-water instrumentation remains the final authority for total runtime memory. The exact project figures above intentionally exclude firmware/framework allocations that cannot be derived from these application structs alone.
