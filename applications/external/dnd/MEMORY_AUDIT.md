# Memory audit

This is a source-derived estimate, not an RTOS high-water measurement. The estimated project-visible peaks below track the largest known nested local-frame paths in project code; compiler prologue/alignment, firmware/framework calls, interrupts and RTOS bookkeeping can consume additional stack. Hardware high-water testing remains authoritative.

## Stack reservations and estimated use

| FAP | Reserved stack | Estimated project-visible peak | Estimated reservation used | Estimated headroom |
|---|---:|---:|---:|---:|
| DNDolphins | 6,144 B (6 KB) | ~2,904 B | ~47% | ~3,240 B |
| DNDAdventure | 4,096 B (4 KB) | ~2,768 B | ~68% | ~1,328 B |
| DNDJournal | 4,096 B (4 KB) | ~2,656 B | ~65% | ~1,440 B |
| DNDInitiative | 4,096 B (4 KB) | ~1,344 B | ~33% | ~2,752 B |
| DNDBestiary | 6,144 B (6 KB) | ~4,368 B | ~71% | ~1,776 B |

The current highest estimated stack pressure remains **DNDBestiary**, followed by **DNDAdventure** and **DNDJournal**. DNDolphins retains substantially more headroom after the Verify Profile temporary was moved off stack. The estimates are intentionally conservative project-code figures and should not be interpreted as free-stack guarantees.

## Known peak paths

### DNDolphins

- Current source-visible project peak: about **2,904 B**.
- Verify Profile is no longer the peak; its large temporary character state is checked transient heap state and is cleared/freed after parsing.
- Spell/item feature modules do not add persistent heap caches and use the same shared dice/storage primitives after the ownership split.
- Character spellbook and inventory workflows retain at most one eight-record page each rather than complete owned collections.

### DNDAdventure

- Current source-visible project peak estimate: about **2,768 B**.
- Campaign manifests/scenes are storage-backed and loaded on demand; bundled campaign content does not increase the scene-object maximum or create a resident campaign database.
- The additional bundled campaign is asset data only and does not add a new runtime cache or save structure.
- Campaign selection now keeps only five prepared 27-byte display rows (135 bytes inside the heap-owned app state) so the draw callback performs no storage reads; this does not increase the estimated stack peak.

### DNDJournal

- Current source-visible project peak estimate: about **2,656 B**.
- The list keeps only a bounded metadata cache; entry bodies are loaded when opened rather than retained for the whole Journal.
- Newest-first ordering is implemented without firmware `qsort` and without materializing the complete journal in RAM.
- Milestone-to-character rewrites use two transient 768-byte heap buffers only while copying the canonical character file; those buffers are freed before returning and do not increase the persistent Journal heap or the estimated stack peak.

### DNDInitiative

- Main-character refresh streams only the active profile's Name, AbilityScores and Vitals lines into fixed local buffers; no full DNDolphins character object or progression metadata is retained.
- Advantage/disadvantage rolling adds only two scalar d20 values and no persistent heap allocation.
- Canonical-character HP/AC synchronization and Turn/Encounter recharge rewrites use two transient 768-byte heap buffers. Oversized source lines abort the rewrite instead of being truncated, and the buffers are freed on every return path.
- No-character startup allocates only the normal fixed app/UI state, does not load a roster sidecar, and does not create `ch_0.txt`; the launch/exit prompt is rendered from fixed strings.

- Current source-visible project peak estimate: about **1,344 B**.
- Party/combat persistence is streamed named-field data rather than a whole-file temporary buffer.
- Initiative remains the largest relative stack reserve margin of the five FAPs.

### DNDBestiary

- Current source-visible project peak estimate: about **4,368 B**.
- The largest known visible path remains encounter named-field writing rather than normal browsing or the default custom-pack seed.
- Monster lookup uses bounded sparse/recent hints rather than complete offset/hash arrays; the bounded lookup cache is static and does not grow with catalog size.
- The browse window remains bounded rather than retaining the full result set.
- Fixed dispatcher/view/timer/input-service objects are reserved before custom migration/seed/recovery and pack-index work; asynchronous input/timer sources start only after that storage initialization completes.

## Heap and collection ownership

- Spellbook and inventory UI ownership pages hold at most eight records.
- Whole-collection spell/resource/item calculations stream one record at a time instead of assuming the resident page is the full collection.
- Character picker and Journal lists use storage-backed bounded metadata caches.
- Bestiary lookup/browse state is bounded; full packaged/custom/pack tables remain on storage.
- Companion apps are separate processes. Cross-FAP transitions quiesce callbacks/timers and free outgoing views, caches, services and dynamic state before launch.
- Adventure reserves its fixed dispatcher/view before character and campaign-index work so variable-sized campaign allocations cannot fragment the heap ahead of core UI allocation.
- The item/spell ownership split did not add persistent heap caches.
- Spell/item Add New prepares the actual final eight-record collection page, grows that resident page in RAM, increments the logical total, and immediately rewrites the authoritative live sidecar. Exact page boundaries load an empty next page before staging the new record; partially filled tail pages retain their existing resident records.
- Record List drawing uses cached Item/Spell access only. SD reads/writes, collection saves, page clears and page allocations occur in input/screen-transition handling before rendering, and sidecar list scrolling keeps each five-row viewport inside one resident eight-record page.
- Completed Item/Spell catalog choices release the bounded catalog allocation before the collection rewrite, reducing heap overlap. Text, numeric and left/right collection edits also commit immediately rather than waiting for the general character autosave timer.

## Starting inventory path

First Inventory open is the only normal starting-inventory initializer. If no live item sidecar exists it streams matching class/species/background equipment and one d100 trinket through bounded readers, writes the new sidecar and applies starting currency once.

The initialization path does not keep the source equipment catalogs resident. Failure restores in-memory currency and removes a newly-created live item sidecar so retry starts from a clean state.

## Default custom monster seed

The Bestiary seed adds no persistent runtime cache or heap allocation of its own. Its file-copy helper uses two `File*` objects plus a **256-byte stack copy buffer**, closes/frees both handles on all paths and publishes through temporary files.

That seed-copy frame is not the Bestiary stack peak. Seeding is attempted only when both live user custom files are absent; existing or partial user custom data is left untouched.

## Leak/ownership audit status

The item/spell refactor preserved the prior dynamic-allocation ownership pattern: moved rule functions themselves allocate no retained heap. Changed storage/copy paths close/free their `File*` objects and temporary buffers on success and failure branches.

Host sanitizer coverage has exercised the moved rule/item/spell paths and default custom seed without AddressSanitizer, UndefinedBehaviorSanitizer or LeakSanitizer findings. This does not replace device-level fragmentation/high-water testing.

## Hardware stress priorities

1. Repeated DNDolphins ↔ companion-FAP handoff loops.
2. DNDBestiary maximum-size encounter write/save/transfer paths.
3. Large character/profile, Journal, campaign and monster indexes.
4. Verify Profile followed by repeated spell/inventory page transitions.
5. First Inventory open with missing sidecar, including failure/retry cases.
6. Repeated Item/Spell Add New cycles across 7→8→9, 15→16→17 and restart/delete boundaries.
7. Repeated Bestiary startup with fresh, existing and partial custom-monster files.

A real RogueMaster build plus device stack high-water/fragmentation stress remains the final authority for reducing any reservation further.

## Progression metadata

Level progression does not keep the metadata catalog resident. It opens the metadata text only when **Grant Initial Traits** or a class level increase needs deterministic grants, streams rows sequentially, and closes it immediately. No checksum/hash table or persistent progression cache is used. Class numeric progression (Hit Dice, shared slots, Pact Magic, Mystic Arcanum, Sorcery Points, cantrip/prepared limits and Wizard spellbook minimums) is calculated directly from the character record.

### Initiative participant-roll delta

Each `InitiativeMember` now carries one byte of participant roll-mode state. With two bounded 24-member arrays (roster + combat), raw member storage increases by at most 48 bytes before normal compiler padding/alignment effects. No new unbounded collection or whole-file progression cache was added. Initial-trait review continues to use the existing bounded grant array.

### Level-choice workflow

The ASI/Feat workflow adds only four scalar fields to the heap-owned DNDolphins app state. It reuses the existing ability menu, feat catalog, feature collection and grant collection; it does not retain another catalog or progression table. Completed choices use existing grant records, so no new resident cache or character schema field is introduced.

Adventure pack removal reuses the existing fixed 16-record temporary registry allocation used by pack enable/install operations, then frees it before returning to the UI. No new persistent pack cache is added.


### Reliability audit adjustments

- Adventure Campaign Pack Controls keep a five-row, 26-character display cache in the heap-owned app state (about 135 bytes plus alignment). Registry/storage reads occur when entering, scrolling, installing, or toggling packs, not during canvas drawing.
- DNDolphins Character Profiles refresh their bounded profile window on screen entry or scroll-window changes; Inventory Resources refreshes its existing aggregate cache on screen entry. Their draw callbacks no longer initiate storage scans.
- Bestiary fixed row caches are explicitly bounded: filter and monster-pack registries expose at most 16 cached records plus their action row, and encounter rows at most the 17-row fixed buffer. This prevents registry size from overrunning the fixed state-row allocation.
- These changes do not add persistent caches, change save schemas, or increase FAP stack reservations.
