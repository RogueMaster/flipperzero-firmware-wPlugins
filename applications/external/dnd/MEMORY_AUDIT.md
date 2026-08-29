# Dungeons & Dolphins source-based memory audit — 3.2.15

This audit is recalculated from the **current 3.2.15 source** after the Verify Profile stack fix, Bestiary state-reader workspace change, Bestiary bounded/sparse lookup-cache refactor, and the reduction from a 35-summary to a 15-summary browse window. It is a **source/ABI planning audit, not an on-device high-water measurement**. No runtime heap probes, stack watermarking, diagnostic timers, or save-format instrumentation were added.

Exact structure sizes below were checked for Cortex-M4 / ARM EABI with Clang 17. Stack frames were compiled for Cortex-M4/Thumb at `-Os -fstack-usage` against opaque Flipper API stubs, then optimized direct-call edges were walked from app entry/callback paths. The reported **source-derived planning peak** is the largest simultaneously live chain of project frames visible to this audit. Firmware/framework/library calls, interrupt context, allocator metadata, and differences in the actual RogueMaster toolchain remain outside the calculation.

## Manifest stack reservations

| App | App ID | Source-derived planning peak | Allocated stack | Source-visible headroom* |
|---|---|---:|---:|---:|
| DNDolphins | `dndolphins` | **3,000 B / 2.93 KiB** | 6 KiB | **3,144 B / 3.07 KiB** |
| DNDAdventure | `dndadventure` | **2,768 B / 2.70 KiB** | 4 KiB | **1,328 B / 1.30 KiB** |
| DNDJournal | `dndjournal` | **2,664 B / 2.60 KiB** | 4 KiB | **1,432 B / 1.40 KiB** |
| DNDInitiative | `dndinitiative` | **1,352 B / 1.32 KiB** | 4 KiB | **2,744 B / 2.68 KiB** |
| DNDBestiary | `dndbestiary` | **4,368 B / 4.27 KiB** | 6 KiB | **1,776 B / 1.73 KiB** |

\* `Allocated stack - source-derived planning peak`. This is not guaranteed free runtime stack because opaque firmware/framework/library frames are not expanded.

Current manifest reservations therefore remain:

- DNDolphins: **6 KiB**
- DNDAdventure: **4 KiB**
- DNDJournal: **4 KiB**
- DNDInitiative: **4 KiB**
- DNDBestiary: **6 KiB**

## Recalculated peak paths

| App | Largest source-visible chain | Peak |
|---|---|---:|
| DNDolphins | `pocket_input_callback -> pocket_handle_catalog -> pocket_catalog_load_page -> pocket_catalog_load_external -> pocket_catalog_load_path -> pocket_catalog_process_line -> pocket_catalog_add_item -> pocket_catalog_add_metadata -> pocket_spell_cache_ensure -> pocket_save_spellbook_if_changed -> pocket_d20_storage_save_spellbook_window -> pocket_d20_rewrite_spellbook -> pocket_d20_write_spell_record -> pocket_d20_writef` | **3,000 B** |
| DNDAdventure | `adventure_input -> pocket_campaign_pack_install_inbox -> campaign_pack_load_registry -> campaign_pack_read_line` | **2,768 B** |
| DNDJournal | `journal_input -> journal_open_detail -> journal_entry_at -> journal_scan_cache -> journal_hydrate_cache -> journal_entry_path` | **2,664 B** |
| DNDInitiative | `dndinitiative_app -> initiative_save -> initiative_write_member_named -> initiative_write_named` | **1,352 B** |
| DNDBestiary | `bestiary_input -> pocket_bestiary_encounter_archive -> pocket_bestiary_encounter_delete -> state_write_encounter -> state_write_named -> state_write_line` | **4,368 B** |

The Bestiary worst path has moved away from nested **read** workspaces. Its current largest visible path is encounter **writing**, where the named-field writer still has large temporary text buffers. Keep the 6 KiB Bestiary reservation until hardware stress confirms the new margin and/or that writer path is reduced.

## Cortex-M4 ABI anchors

| Structure | Size | Notes |
|---|---:|---|
| `PocketSaveData` / `PocketCharacter` | **3,976 B** | Core character state; app-owned on heap in normal runtime |
| `PocketSpell` | **324 B** | One owned spell record |
| Spell state bytes | **4 B/spell** | known / always-prepared / free-current / free-max |
| `PocketItem` | **298 B** | One owned item record |
| `PocketFeature` | **234 B** | Dynamic core collection |
| `PocketGrant` | **148 B** | Dynamic core collection |
| `PocketProfileEntry` | **32 B** | Character picker metadata |
| `JournalEntryMeta` | **84 B** | Journal cache metadata |
| `JournalEntry` | **276 B** | One opened Journal entry |
| `InitiativeMember` | **106 B** | Initiative roster/combat member |
| `PocketMonsterSummary` | **172 B** | Bestiary browse/encounter summary |
| `PocketMonsterDetail` | **1,544 B** | Opened Bestiary detail |
| `PocketMonsterEncounter` | **2,088 B** | Generated encounter object |
| `DndAdventureScene` | **865 B** | Current Adventure scene |
| Bestiary `StateReadWorkspace` | **1,288 B** | 512-byte reader + 768-byte line, now checked transient heap for encounter-state scans |
| Bestiary bounded `MonsterCache` | **616 B** | Fixed static sparse/recent lookup state for bundled/custom/enabled sources |

## DNDolphins Verify Profile stack fix

Prior 3.2.14 code instantiated a full **3,976-byte `PocketSaveData`** on the Verify Profile stack path. 3.2.15 changes only the ownership of that temporary object:

- `PocketSaveData` is allocated with checked `calloc()` for verification;
- the existing best-effort character parser is still used;
- any dynamic records created by parsing are cleared through the normal character cleanup path;
- the temporary character is freed before returning;
- allocation failure makes verification fail cleanly;
- **no character save field or format changed**.

The `pocket_d20_storage_verify_profile()` function frame is now about **112 B** instead of carrying the character object itself. Its full visible subchain is approximately **1,408 B**, and the registered input path through Verify Profile is approximately **1,472 B**. Verify Profile is therefore no longer the DNDolphins planning peak.

DNDolphins' new largest visible project chain is **3,000 B**. With the 3.2.15 manifest set to **6 KiB**, that leaves **3,144 B / 3.07 KiB** of source-visible space before opaque firmware/library/interrupt frames. This is still a planning margin rather than a measured runtime high-water result.

## Core character residency and eight-record sidecars

The schema-5 core loader does not hydrate owned spells/items. Normal DNDolphins character residency therefore carries the fixed **3,976-byte** character plus only dynamic core collections that actually exist.

Compared with retaining maximum owned collections permanently, the sidecar architecture avoids up to **15,024 B** of raw persistent heap during ordinary non-spell/non-item operation:

- 24 spells: `24 * (324 + 4)` = **7,872 B**
- 24 items: `24 * 298` = **7,152 B**

### Spellbook sidecar

`/ext/apps_data/dndolphins/ch_{id}_spellbook.txt` uses a logical cache ceiling of **8 records**, but the allocation now grows only with the records actually present in the current page: **1 -> 2 -> 4 -> 8**. An empty spellbook therefore allocates no spell page, and the first owned spell allocates capacity for one record rather than preallocating all eight.

- 1 spell + state quartet: **328 B** resident after the first add/load
- 2-record capacity: **656 B**
- 4-record capacity: **1,312 B**
- maximum 8-record page: **2,624 B**
- temporary sidecar line buffer while scanning: **1,280 B heap**
- worst-case full page + scan scratch: **3,904 B raw heap**
- normal in-app append to the currently resident page adopts the just-written record directly, avoiding a 1,280-byte readback buffer merely to reload the record that was just persisted

A 24-spell full hydration would have required 7,872 B for the collection alone, so the eight-record ceiling still saves **5,248 B** at a full working page while low-count characters now consume substantially less than that ceiling.

### Item sidecar

`/ext/apps_data/dndolphins/ch_{id}_items.txt` uses the same **up-to-eight-record** cache with **1 -> 2 -> 4 -> 8** growth. The first item therefore reserves only one `PocketItem` instead of all eight.

- 1-item capacity: **298 B** resident after the first add/load
- 2-item capacity: **596 B**
- 4-item capacity: **1,192 B**
- maximum 8-record page: **2,384 B**
- temporary sidecar line buffer while scanning: **1,280 B heap**
- worst-case full page + scan scratch: **3,664 B raw heap**
- normal in-app append to the current page adopts the just-written item directly and does not reread the sidecar solely to hydrate that new record

A 24-item full hydration would have required 7,152 B, so the eight-record ceiling still saves **4,768 B** at a full working page while small inventories now scale their heap allocation with their actual page population.

Owned sidecar records remain self-contained; master catalogs are add/select sources only. Old embedded `Spell*` / `Item*` fields are not migrated or recovered.

## DNDAdventure

Adventure continues to stream campaign/scene text rather than loading whole adventure files. Scene scanning uses bounded read/line buffers and retains only the active scene plus its choices. An eight-line scene cache would not reduce memory relative to this one-line streaming model.

Adventure item rewards use the same character-item sidecar paging contract as DNDolphins and release that temporary item state after the reward operation.

Current source-visible peak: **2,768 B**, leaving **1,328 B** under the 4 KiB reservation before opaque firmware/library frames.

## DNDJournal

Journal remains storage-backed and bounded:

- eight `JournalEntryMeta`: **672 B**
- one opened `JournalEntry`: **276 B**
- entry bodies are loaded only when opened
- the entry list does not retain every journal body

Current source-visible peak: **2,664 B**, leaving **1,432 B** under the 4 KiB reservation. Keep Journal at 4 KiB.

## DNDInitiative

`InitiativeMember` is **106 B**. The roster/combat arrays are inside the heap-owned app object, so increasing the logical roster to 15 participants does not linearly increase stack consumption.

Current source-visible peak: **1,352 B**, leaving **2,744 B** under the 4 KiB reservation. The required device test remains repeated 15-participant Roll-for-All/sort/save/combat cycling.

## DNDBestiary 3.2.15 lookup-cache refactor

### Previous retained lookup acceleration

With the bundled **340-monster** data set, the prior geometric cache grew to capacities of 512:

- index offsets: `512 * 4` = **2,048 B heap**
- index hashes: `512 * 4` = **2,048 B heap**
- statblock `{hash,offset}` entries: `512 * 8` = **4,096 B heap**
- total bundled lookup-cache heap after statblock cache construction: approximately **8,192 B**, excluding allocator overhead

The old static pointer/counter cache structure itself was **76 B**.

### Current bounded/sparse cache

3.2.15 removes all full-file offset/hash heap arrays. Each of bundled/custom/enabled now keeps only:

- up to **16 sparse index checkpoints**, one every 32 valid records;
- up to **8 recent index `{hash,offset}` hints**;
- up to **8 recent statblock `{hash,offset}` hints**;
- the streamed full-file fallback remains authoritative when no hint applies.

The complete fixed `MonsterCache` for all three source paths is **616 B static RAM** and allocates **0 B of persistent lookup-cache heap**.

For the current 340 bundled summaries, only eleven sparse checkpoint positions are logically needed (`0, 32, ... 320`), though the fixed array capacity remains 16. `pocket_monster_at()` starts from the nearest sparse checkpoint and streams forward. `pocket_monster_find()` first checks the small recent-ID hints then streams on a miss. Monster detail does the same with recent statblock hints. Cache misses therefore remain correct even after arbitrary text reordering because the stored text format and streamed parser remain authoritative.

Comparing cache structures alone after the old bundled statblock cache had been built:

- old lookup heap: **8,192 B**
- extra static RAM in the new cache versus the old 76-byte pointer structure: `616 - 76` = **540 B**
- approximate net retained-RAM reduction attributable to lookup acceleration: **7,652 B**, before allocator overhead

If custom/enabled packs contain enough records to grow their old geometric caches, the saving is larger because the new total cache remains fixed at 616 B.

### 15-summary browse window

The Bestiary browse allocation is reduced from 35 to **15 `PocketMonsterSummary` records**:

- previous: `35 * 172` = **6,020 B heap**
- current: `15 * 172` = **2,580 B heap**
- browse-window saving: **3,440 B**

For the common bundled-only catalog before opening a monster detail, the prior index cache + browse window + old 76-byte cache structure represented about **10,192 B** of raw retained project RAM. The new 616-byte bounded cache + 15-summary window is about **3,196 B**, a reduction of approximately **6,996 B**.

After the old bundled statblock-offset table had also been built, the comparable reduction is approximately **11,092 B** of retained project RAM.

These figures exclude allocator headers, `File` internals, GUI objects, and opened-detail/encounter allocations.

## DNDBestiary encounter-state reader stack change

Bestiary state files keep their existing text format exactly. The change is ownership of encounter scan work areas.

Previously, encounter helpers declared a **512-byte `StateReader` buffer and 768-byte line** in project stack frames. Nested encounter operations could therefore stack multiple large reader frames.

3.2.15 introduces a checked transient **1,288-byte heap `StateReadWorkspace`** for encounter-state scans. The workspace contains the reader and line buffer, is allocated only while a scan is active, and is freed before the caller continues. The named-mask scan and subsequent encounter scan do not retain overlapping workspaces.

Compiler-frame examples after this change:

- `state_encounter_named_mask`: about **48 B** project frame
- `state_encounter_count_path`: about **472 B**
- `state_encounter_at_path`: about **488 B**

The former reader-dominated Bestiary peak is gone. The new largest visible Bestiary chain is the encounter **writer** path at **4,368 B**, leaving **1,776 B** under the 6 KiB reservation before opaque firmware/library frames.

## Bestiary streaming remains format-preserving

The monster index and statblock files still use the same line-oriented text formats. The refactor changes only lookup acceleration and working-set size:

- index text is still streamed and parsed one line at a time;
- statblock text is still streamed and parsed one line at a time;
- sparse/recent offsets are volatile only and are never written to files;
- no checksum/version gate was added;
- custom/enabled/bundled text formats are unchanged;
- streamed fallback remains available whenever a cached hint is absent or does not verify the requested ID.

## Cross-FAP teardown boundary

All five FAPs retain teardown-before-Loader behavior. Relevant memory ownership at handoff:

- DNDolphins releases spell/item pages and catalog/core dynamic state.
- DNDAdventure releases scene/reward-item working state.
- DNDJournal releases its bounded metadata/current-entry state.
- DNDInitiative releases the heap-owned roster/combat app object.
- DNDBestiary releases the 15-summary browse allocation, detail/encounter allocations, and resets bounded lookup hints before handoff.

No companion is intentionally resident while another FAP runs.

## Static/host validation for 3.2.15

Changed DNDolphins/Bestiary translation units compile cleanly for Cortex-M4/Thumb at `-Os -Wall -Wextra -Werror` against the audit Flipper API stubs.

A filesystem-backed Bestiary test using the packaged **340-monster** assets verified:

- catalog count remains 340;
- ordinal reads work across sparse boundaries including 31/32/33 and the final record 339;
- ID lookup succeeds and agrees with ordinal results;
- statblock detail loading succeeds after streamed lookup and after recent-offset reuse;
- 15-summary query windows return 15 records at starts 0 and 15;
- the final window at start 330 returns the expected 10 records;
- cache reset followed by recount still returns 340.

Test result:

`BESTIARY SPARSE CACHE PASS: 340 monsters, 15-summary paging, ordinal/find/detail/reset`

The pre-existing 24-record character spell/item paging host tests remain applicable; this release does not alter their storage formats or paging contract.

## Audit conclusion

3.2.15 addresses the three targeted memory issues without changing character or Bestiary text formats:

1. **Verify Profile:** moves the 3,976-byte temporary character off the stack; DNDolphins source-visible peak falls to **3,000 B** and Verify Profile itself is no longer the peak.
2. **Bestiary encounter state reads:** moves the 512-byte reader + 768-byte line pair to checked transient heap storage; Bestiary's prior nested reader peak disappears.
3. **Bestiary retained lookup/browser heap:** replaces ~8 KiB of bundled full-file lookup arrays with a fixed **616 B** sparse/recent cache and reduces the browse window from **6,020 B to 2,580 B**.

For the bundled 340-monster catalog, the new Bestiary design reduces retained project RAM by roughly **7.0 KiB before detail lookup** and roughly **10.8 KiB / 11.1 KB after the old statblock cache would previously have been built**, depending on whether values are expressed in KiB or decimal bytes. The exact decimal comparison is **6,996 B** before detail and **11,092 B** after old statblock-cache construction.

Hardware stress should confirm the new **6 KiB DNDolphins** reservation and the existing companion reservations against real firmware/library frames. The next Bestiary stack target, if further reduction is needed, is the large encounter named-field **writer** buffers rather than the now-heap-owned encounter reader workspace.

### Combat sidecar discovery

Combat no longer treats the resident eight-record spell/item page as the authoritative discovery list. On entry to Spell Attacks or Weapon Attacks, DNDolphins streams the corresponding character sidecar once and retains only logical indexes for qualifying records. The fixed maps cost **48 bytes** for 24 spell indexes + 24 item indexes, plus two one-byte counts; ARM record layout increases `PocketD20App` from **4,932 B to 4,980 B** (**+48 B** after alignment).

The streamed discovery scan uses the existing **1,280-byte heap line buffer** and one parsed record at a time. Cortex-M4 `-Os` stack frames are **752 B** for `pocket_d20_storage_visit_spells()` and **720 B** for `pocket_d20_storage_visit_items()`; the spell eligibility callback is **56 B**. These paths remain below the existing DNDolphins source-derived planning peak, which remains **3,000 B** with the **6 KB** reservation. The transient line buffer is freed immediately after the index map is rebuilt. Visible/selected records then use the normal up-to-eight-record page cache.

This also isolates Combat from stale collection totals/page state: successful scans refresh `spellbook_total` / `items_total` from the actual sidecar contents.
