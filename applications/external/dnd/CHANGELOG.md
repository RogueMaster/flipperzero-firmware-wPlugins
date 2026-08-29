# Dungeons & Dolphins changelog

This changelog keeps recent architecture and behavior changes that still describe the current code. Troubleshooting experiments and features later removed are intentionally omitted.

## 3.2.15

- Removed the Verify Profile stack spike without changing character storage: the temporary 3,976-byte `PocketSaveData` is now checked transient heap state and is cleared/freed after the existing best-effort parser finishes.
- Recalculated DNDolphins' largest source-visible project stack chain at **3,000 B**, down from the former Verify Profile-dominated 5,456 B path; the Verify Profile input path itself is now about **1,472 B** project-visible.
- Reworked Bestiary encounter-state scans so the 512-byte reader and 768-byte line work area live in a checked transient 1,288-byte heap workspace instead of overlapping nested state-reader stack frames. Bestiary text formats are unchanged.
- Replaced Bestiary's geometric full-file index offset/hash and statblock offset arrays with a bounded fixed cache: 16 sparse index checkpoints plus 8 recent index hints and 8 recent statblock hints per bundled/custom/enabled path, with streamed fallback remaining authoritative. The complete bounded cache is **616 B static RAM** and allocates no persistent lookup-cache heap.
- Reduced the Bestiary browse working set from **35 summaries (6,020 B)** to **15 summaries (2,580 B)**, saving **3,440 B** of browse heap.
- With the packaged 340-monster catalog, the lookup/browser refactor saves about **6,996 B** of retained project RAM before a detail lookup and about **11,092 B** versus the old state after its bundled statblock-offset cache had been built.
- Recalculated Bestiary's largest source-visible stack chain at **4,368 B**; the former nested read path is no longer the peak. The current worst visible path is encounter named-field writing, so DNDBestiary remains at 6 KB pending hardware stress.
- Added host validation across sparse boundaries and the final 340-monster page; ordinal lookup, ID lookup, detail loading, 15-summary windows and cache reset all passed.
- Updated all five manifests to 3.2.15 with DNDolphins reduced to **6 KB** after the Verify Profile stack fix; DNDAdventure, DNDJournal and DNDInitiative remain at 4 KB and DNDBestiary remains at 6 KB.
- Fixed first-entry Add failures in Spellbook and Inventory after sidecar paging: page storage no longer preallocates all eight records when only one or a few records exist. Spell/item page capacity now grows **1 -> 2 -> 4 -> 8**, and a newly appended record is adopted directly into the resident page instead of immediately rereading the sidecar.
- Corrected the Record List failure message so `List is full` is shown only when the logical collection has actually reached its configured maximum; storage/allocation failures now report `Add failed - retry SD`.
- Fixed Combat weapon/spell discovery after the character sidecar split. Combat now rebuilds compact logical-index maps by streaming `ch_{id}_items.txt` / `ch_{id}_spellbook.txt` directly when entering Weapon Attacks or Spell Attacks, so an empty/stale UI page cache cannot make owned combat records disappear. Only the selected/visible record page is hydrated afterward.
- Added generic one-record-at-a-time sidecar visitors for owned items/spells. Combat keeps only up to 24 one-byte logical indexes per collection (48 bytes plus two counts in the app state) instead of using the paged collection itself as the authoritative discovery index.

## 3.2.14

- Split owned character spells and items out of the core character save into `/ext/apps_data/dndolphins/ch_{id}_spellbook.txt` and `ch_{id}_items.txt`.
- Replaced numbered `Spell0...SpellN` / `Item0...ItemN` persistence with one escaped line record per owned spell/item and no serialized count field.
- Made sidecar paging count only valid parsed records; malformed/manual `S|` or `I|` lines are preserved but cannot shift later page edit/delete targets.
- Made spellbook and inventory allocation workflow-scoped and **eight-record paged**: the core character loader keeps neither collection resident; Magic/spell and Inventory/weapon/resource workflows hold at most one aligned eight-record page and save/free it on page changes or workflow exit.
- Updated class prepared-count display, class deletion/remapping, long rests, granted spells, spell-combat filtering, aggregate item calculations, weapon enumeration, and Adventure item rewards to walk the sidecars through the same eight-record window instead of materializing all 24 records.
- Kept master spell/item catalogs as add/select sources only; owned records are self-contained in their character sidecar after selection.
- Removed all reading/writing of embedded `Spell*` and `Item*` fields from the core character parser/writer. No migration or fallback path is provided for old embedded spell/item data.
- Split dirty fingerprints so hydrating/releasing spellbook/items never marks the core character changed and sidecar-only updates never create `.shd` character history.
- Extended duplicate/delete/archive/export/import handling so current-format spellbook/items companions follow the character ID as a logical set.
- Refreshed all Markdown documentation for schema 5 and the bounded sidecar ownership model, and replaced the memory audit with a post-paging Cortex-M4 source audit. Maximum spell workflow collection heap is now 2,624 B (plus 1,280 B scan scratch) and maximum item workflow collection heap is 2,384 B (plus 1,280 B scan scratch).
- Audited Adventure-style line streaming across catalogs and companion data. Character/master spell-item readers, Adventure, Journal and Bestiary already stream source files rather than loading whole files; the largest remaining Bestiary heap opportunity is its persistent full monster offset/hash caches plus 35-summary browse window, not its line reader.
- Updated all five FAP manifests to this release. Current stack reservations are DNDolphins 8 KB, DNDAdventure 4 KB, DNDJournal 4 KB, DNDInitiative 4 KB and DNDBestiary 6 KB. Adventure and Initiative were reduced after the source-derived stack audit showed 2,768 B and 1,344 B project-visible peaks respectively; Journal remains at 4 KB with a 2,656 B project-visible peak.

## 3.2.13

- Reworked character loading to be best-effort by recognized `Key=Value` field. The character version is informational; unknown fields, reordered fields, missing fields, and malformed individual values no longer reject the entire readable character.
- Removed the global `pocket_d20_data_is_consistent()` load/save gate. Optional dynamic collections are protected at their point of use instead of making unrelated character fields unsavable.
- Fixed sanitizer NULL dereferences for absent legacy party, initiative, and encounter-history arrays. Missing companion-owned arrays now sanitize as empty.
- Restored character-only relocation from `/ext/apps_data/dungeons_and_dolphins/profiles/ch*.txt` into `/ext/apps_data/dndolphins/`. Files move unchanged, never overwrite an existing current character ID, and are interpreted afterward by the tolerant character loader.
- Active-profile metadata now loads by scanning for a valid `Active=` field rather than requiring a fixed first-line layout.
- Converted DNDInitiative persistence to streaming indexed named fields and removed its former 8 KB whole-file load buffer and packed-row load path.
- Converted DNDJournal entry metadata to independent named fields; the old compact `Data=` row is intentionally no longer loaded.
- Enforced `.shd` as write-only character history: shadows are never scanned, loaded, parsed, used for recovery, or used to reserve IDs.
- Removed the later `pocket_data_heap_valid()` global save veto as well; no whole-character heap predicate can reject an otherwise writable character.
- Character dirty fingerprints now hash logical character values and populated dynamic records, not heap pointer addresses or allocation capacities, so allocator churn cannot create false saves/shadow updates.
- Readable character files with valid `ch_{id}_{name}_{level}.txt` filenames use filename name/level as fallback metadata even when the body contains only unknown/future fields, preventing synthetic `New Hero` defaults from replacing a real filename identity.
- Wired `.shd` maintenance to actual changed-character saves, including Adventure character rewards, automatic spell-slot initialization and explicit backup restore; initial character creation/import remains shadow-free until the character is updated.
- A failed legacy character relocation now blocks fresh-character creation for that startup so unmoved old saves are preserved for a later retry.
- Fixed the storage-backed active-character traversal declaration/definition mismatch by using the single `pocket_d20_profiles_next_after()` implementation everywhere.

- Fixed DNDolphins-to-companion launches so Bestiary is no longer blocked by character load state and Journal/Adventure/Initiative can launch without a character argument when no active character is loaded. All launch targets remain explicit full `.fap` paths.
- Added direct-launch character resolution for DNDJournal, DNDAdventure and DNDInitiative: explicit handoff ID first, persisted DNDolphins active-profile ID second, character 0 fallback last.
- Hardened active-character recovery: a missing active ID advances to the next available character and wraps; an unreadable selected character advances through other real character files before default data can be exposed.
- Made failed profile switches reload the previously saved character immediately. Removed global heap-consistency save vetoes; optional dynamic character collections are now sanitized and serialized independently so unrelated missing allocations cannot block the rest of the character.
- Renamed the C source/header tree around the actual FAP ownership namespaces instead of the legacy `pocket_d20` filenames. DNDolphins-owned modules now use `dndolphins*`, DNDAdventure uses `dndadventure*`, DNDJournal uses `dndjournal.c`, DNDInitiative uses `dndinitiative.c`, and DNDBestiary uses `dndbestiary*`.
- Renamed all five application entry-point functions to match their app IDs: `dndolphins_app`, `dndadventure_app`, `dndjournal_app`, `dndinitiative_app`, and `dndbestiary_app`.
- Changed every cross-FAP handoff to use an explicit absolute `.fap` path under `/ext/apps/Games/`. App IDs are no longer used to derive or resolve Loader launch targets; Journal, Adventure, Initiative and Bestiary returns use the same full-path contract.
- Isolated the firmware Loader dependency in shared `dnd_handoff.c`; only that handoff implementation includes `<loader/loader.h>`, while individual app source files call the shared launcher after teardown.
- Renamed the DNDolphins home-menu item from `DNDBestiary` to `Bestiary`.
- Fixed character saves so `.shd` shadow-copy failure can never fail or mark a successful primary `.txt` save read-only.
- Audited all five FAP shutdown paths so Loader is opened only after the outgoing app has released its views, dispatcher, service records, dynamic character/campaign/monster state and static caches. DNDolphins and Bestiary now quiesce timers/pubsub callbacks as soon as a handoff is requested, and Adventure releases scene/cache allocations before returning.
- Renamed the two cross-FAP utility headers to neutral `dnd_fs.h` and `dnd_handoff.h` names. Character/rules/storage helpers shared with Adventure remain owned by the `dndolphins*` namespace because DNDolphins owns character data.
- Updated every manifest source reference and internal include to the renamed files without changing storage formats, save schemas, campaign ownership, paging behavior, or stack allocations.
- Preserved the bounded eight-record character cache, bounded eight-entry Journal metadata cache/body-on-open model, and visible-row Combat formatter introduced in the previous release.

## 3.2.12

- Removed two dead static helpers left behind by the picker/parser refactor (`pocket_parse_i8_strict` and `pocket_split_fields`) so firmware builds remain clean under `-Werror=unused-function`.
- Reworked the character picker into a storage-backed, ID-ordered list with a fixed eight-record metadata cache instead of a growable array containing every character. `.shd` files never participate in picker scans or profile-ID allocation.
- Reworked DNDJournal into a storage-backed newest-first list with a fixed eight-entry metadata cache. Page selection scans timestamp filenames and hydrates metadata for only the selected eight files; entry bodies are loaded only when opened. This removes the previous 24-full-entry list cap and roughly 6.6 KB worst-case Journal entry-array heap block.
- Replaced Journal's large encoded/value line scratch parsing with streaming key/value decoding through the existing bounded reader buffer.
- Removed the 1,056-byte `char rows[22][48]` Combat menu block and its pointer table. The Combat screen now formats only each visible 48-byte row on demand.
- Kept structured spell-combat metadata as the authoritative path for cast-level scaling, multiple attacks, secondary dice and special resolution. Notes `XdY` parsing remains a guarded fallback for simple unmapped cases rather than becoming the primary spell rules engine.
- Updated the source-based memory audit and physical-device stress matrix for high character counts, Journal counts above the former cap, paging boundaries and repeated Combat drawing.
- Refreshed all packaged Markdown documentation. App release numbers are kept in this changelog and `ROADMAP.md`; ordinary feature/schema documentation describes the current behavior without release-number labels.

## 3.2.11

- Added same-stem `.shd` character shadows beside character saves. The active level's shadow refreshes to the latest successful state and prior-level shadows remain as write-only history.
- Added the shared Combat `Normal` / `Advantage` / `Disadvantage` attack mode for weapon and spell attack d20s.
- Added minimum-XP floors when total character level rises, without reducing higher XP totals.
- Split DNDAdventure into its own FAP and made it the sole owner of campaign state and campaign-pack code.
- Removed Adventure fields from `PocketCharacter` and kept campaign progress under `/ext/apps_data/dndadventure/`.
- Character files are stored directly under `/ext/apps_data/dndolphins/`.
- Restored one-way Adventure milestone output to DNDJournal and retained teardown-before-launch companion handoffs.
- Simplified DNDAdventure persistence to one canonical destination for its active campaign, progress, registry and enabled index.

## 3.2.10

- Split Journal and Initiative into standalone FAPs with their own app-data namespaces and removed their resident state from DNDolphins.
- Kept the writer on the current character schema while removing broad schema-conversion migration logic.
- Moved Journal entries to timestamped per-character files and removed the firmware `qsort` dependency.
- Returned DNDolphins to an 8 KB stack after the source-based OOM cleanup.

## 3.2.9

- Reduced stack/path/reader pressure across character, campaign and pack storage and moved large working registries to checked transient heap allocations.
- Established timestamped per-character Journal files and removed Journal from the character payload.
- Advanced the character writer to schema 4 and removed embedded Adventure progress from character files.
- Made campaign startup lazy so campaign files and scene data were not loaded until Adventure was opened.

## 3.2.8

- Reduced dynamic character-array over-allocation and spell-storage growth pressure.
- Reduced catalog working pages and released large workflow allocations when leaving those workflows.
- Expanded Combat > Spell Attacks mappings and added guarded `XdY`/`XDY` Notes fallback parsing.

## 3.2.7

- Moved Dolphin Bestiary into the fixed Home menu and kept it launchable after character save failures.
- Added Combat > Spell Attacks with tracked spell/resource selection, ritual handling and mapped spell dice.
- Added stricter low-memory allocation checks and safer shutdown ordering.

## 3.2.5-3.2.6

- Restored Bestiary Save Encounter/Add to Initiative workflows and preserved existing character files on startup.
- Removed checksum enforcement so text saves remain manually editable.
- Added the current Roll for All / Back-to-previous-turn Initiative direction and Adventure skill-check presentation.

## Earlier releases

Earlier development established the character tracker, spell/inventory systems, rules helpers, streamed catalogs, custom monsters, encounter generation and pack formats. Git history remains the authoritative record for implementation details that no longer describe the current architecture.
