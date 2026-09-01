# Dungeons & Dolphins changelog

Released work only. Normal releases are retained as concise summaries; closely related recovery spans may be consolidated when the individual troubleshooting chronology would obscure the released outcome.

## 3.3.8 — DNDolphins home-menu ordering and named focus indices

- Reordered the DNDolphins Home menu into character sheet, character resources, encounter tools and campaign-play groups: Characters, Character, Vitals, Abilities & Saves, Skills, Features & Perks, Inventory, Magic & Spells, Bestiary, Initiative, Combat, Dice Roller, Adventure and Journal. The encounter workflow is intentionally Bestiary → Initiative → Combat.
- Replaced raw numeric Home-menu cases and companion-return focus positions with the `DndolphinsHomeIndex` enum. Home dispatch now uses named cases, and companion return arguments translate to the matching named index before selection/scroll restoration.
- Preserved same-row return from DNDolphins submenus and cross-FAP return focus while making future menu reordering resistant to index drift.

## 3.3.7 — Documentation, profile handoff ownership and manifest audit

- Consolidated the suite-wide active-profile reference and cross-FAP Loader handoff contracts into `dnd_profile_handoff.*`, linked by all seven FAPs. Removed the duplicate `Active=` parser from shared storage so active-character metadata has one implementation while full profile/sidecar persistence remains in `dnd_storage.*`.
- Alphabetized every FAP manifest `sources` list for auditability without changing runtime behavior or link ownership.
- Rebuilt README as an app-by-app feature inventory covering DNDolphins, Inventory, Spellbook, Adventure, Journal, Initiative and Bestiary, with suite-wide behavior kept in a separate shared section.
- Recalculated current ARM32 project memory documentation from the actual app structs/manifests. Correct fixed app blocks are 4,924 B DNDolphins, 5,072 B Inventory, 5,112 B Spellbook, 4,760 B Adventure, 1,352 B Journal, 5,276 B Initiative and 1,528 B Bestiary; stack reservations remain 6/4/4/4/4/3/6 KB. Updated derived working-set arithmetic and clearly separated exact project sizes from source-estimated stack peaks and firmware/framework overhead.
- Tightened compatibility, save-schema, feature, source-ownership and device-test documentation to match the unified profile/handoff contract and current memory figures.

## 3.3.6 — Inventory currency grants and navigation focus

- Made normal **Grant Initial Inventory** persist the existing balance plus class/species/background starting currency in the same synced `inventory_{id}.txt` creation as the granted Item records and `InitialInventory=1`, removing the second metadata rewrite that previously separated Item creation from final currency persistence. The exact committed balance is mirrored back into the Inventory app state.
- Made the one-time Inventory regrant return the exact combined `Currency=` balance committed by its transactional sidecar rewrite, so the UI no longer derives the displayed result from potentially stale in-memory currency after the regrant. Existing Items and the one-time `InitialInventory=2` protection remain unchanged.
- Changed active Initiative Combat navigation so **Short Back** returns to the Initiative main menu without ending the active encounter; Resume returns to that combat. **Hold Up** now moves the current turn backward, including crossing to the previous round's last participant when applicable.
- Preserved the Initiative quick-AC shortcut by making Hold Up on a selected participant in the pre-combat Setup list raise AC; active Combat retains short Left/Right HP changes, Hold Down condition editing, Hold Left/Right reordering and Hold OK full participant editing.
- DNDolphins now remembers the selected home-menu row when entering an internal submenu and restores that same row/scroll position when Back returns Home instead of resetting focus to the first option. The added `uint16_t` focus field consumes existing ARM32 alignment padding, so the measured fixed DNDolphins app-state size remains unchanged.

## 3.3.5 — Return focus, bounded paging, catalog filtering and Spellbook ordering

- Companion Short Back returns to DNDolphins with the corresponding home-menu row focused: Inventory, Magic & Spells for Spellbook, Journal, Adventure, Bestiary or Initiative. Hold Back remains a force-exit to firmware and never requests a parent handoff.
- Restored DNDInitiative's dark title bar while keeping `[characterId]` main-menu-only and reserving the combat title-bar right side for the round counter. The `UINT32_MAX` profile sentinel remains guarded so it cannot render as `[4294967295]`; valid character ID `0` remains supported.
- Restored Item Name-catalog presentation to the established compact format: category initial without brackets, `*` for magic entries, bare names for Other items, and `Page N <>` only in explicit catalog paging. Spell Name catalog uses the same explicit `Page N <>` convention; normal owned lists remain free of persistent paging glyphs.
- Accelerated Inventory/Spellbook sidecar paging with three cached 32-bit aligned page offsets per collection at the 24-record cap. After the initial bounded scan, page changes can seek directly to records 0/8/16 instead of rescanning from byte zero.
- Accelerated Item/Spell Name catalogs with a bounded rolling 64-page (256-byte) filtered offset map plus a 128-byte buffered reader. This reduces repeated SD rescans/single-byte reads without loading either catalog into RAM.
- Recorded Spellbook entries are kept in deterministic **spell level, then case-insensitive alphabetical name** order. Sorting uses at most 24 compact transient keys and rewrites the sidecar only when it is actually out of order; it does not retain a second full Spell collection. The sorter is owned directly by `dndspellbook_collection.c`; shared `dnd_storage.c` no longer contains or exports Spellbook-only sorting code, preventing unrelated FAPs that link shared storage from carrying that implementation.
- Expanded the Item Name-catalog Hold-OK filter cycle to All, Weapons, Armor, Ammunition, Gear, Tools, Mounts/Vehicles, Potions, Rings, Rods, Scrolls, Staffs, Wands, Wondrous and Magic. The broad Magic option remains an aggregate non-Mundane filter without increasing the catalog cache size.
- Bundled a compact generic Spell Scroll set under the Scroll category: **Spell Scroll (Cantrip)** and **Spell Scroll (Level 1)** through **Spell Scroll (Level 9)**, with `SRD5.2.1` source metadata and level-appropriate rarity (Common L0–1, Uncommon L2–3, Rare L4–5, Very Rare L6–8, Legendary L9).
- Re-audited stack and project-owned heap after the paging/sort changes. Existing stack reservations remain appropriate; Inventory/Spellbook fixed app state rises only for the bounded offset maps, and the memory audit includes the transient Spellbook sort peak.
- Completed a follow-up source-ownership/header pass without changing save formats or behavior. DNDolphins-only roll-mode/value-recording dice moved out of shared rules into `dndolphins_dice.*`; Initiative/effective-speed helpers moved into `dndolphins_rules_character.*`; spell class-count derivation moved into `dndolphins_spells.*`; Inventory equipped/weight aggregation moved into `dndinventory_collection.c`; the Eldritch Knight/Arcane Trickster Wizard-list alias test moved into Spellbook; and sidecar-creation helpers that never leave `dnd_storage.c` are now private. Shared transactional sidecar/profile parsing stays centralized where splitting it would duplicate parsers or rewrite logic.
- Removed the stale DNDolphins spell-save-DC wrapper left behind by the ownership cleanup, fixing the `-Werror=unused-function` build failure while retaining the active save-DC implementation in `dndolphins_spells.c`.

## 3.3.4 — Ritual Adept and stability audit

- Added **Combat → Rituals** for Wizard Ritual Adept. The list is built from the active character's known Wizard spellbook entries that have the Ritual tag; preparation is not required. Ritual casting uses the existing spell result flow, consumes no spell slot/Pact slot/spell points/free cast, and reports the additional 10-minute ritual casting time.
- Hardened companion character-ID headers so the internal `UINT32_MAX` sentinel can never render as `[4294967295]`; valid ID `0` remains supported.
- Completed a render-path stability pass across all seven FAPs. Canvas drawing now uses resident RAM state only: profile windows, class spell counts, Journal windows and Combat Item/Spell rows are prepared from screen-entry/input paths instead of performing storage reads or allocations from draw callbacks. Unrelated row preparation was also removed from Attack Templates navigation.
- Re-audited project-owned allocation ownership and failure cleanup. Collection pages, combat indexes, Bestiary windows/details/encounters, Adventure scenes, Journal scan buffers and shared storage temporaries remain bounded and have matched release paths; Bestiary startup failure cleanup now also releases any partially created dynamic blocks.
- Recomputed the app-state/collection memory audit for that release; the current audit later replaced those early layout figures with compiler-checked ARM32 sizes and derived working sets.
- Updated all seven FAP manifests to release version 3.3.4 and refreshed rules, compatibility, feature, test, accessibility, save-schema, roadmap and memory documentation for the retained behavior.

## 3.3.3 — Combat spell eligibility, one-time inventory regrant and UI consistency

- Corrected Combat → Spell Attacks for Wizard characters. Wizard cantrips remain normally available; level-1+ Wizard spells appear only when Prepared/Always Prepared or when a Free Cast remains. An unprepared Wizard spell exposed solely by a Free Cast offers only that Free Cast and cannot spend ordinary slots, Pact slots or spell points from the combat picker. Other classes retain their existing Known/Prepared rules, and the existing combat damage mappings, scaling, upcasting and resource consumption remain intact.
- Added a deliberate one-time override for **Grant Initial Inventory**. Short OK keeps the normal one-shot protections. Hold OK on an already granted Inventory appends the starting class/species/background package again, adds its starting currency again, preserves existing Items, and changes the Inventory marker from `InitialInventory=1` to `InitialInventory=2`. A successful override cannot be repeated.
- Made routine successful collection/save feedback transient. `Saved`, catalog-save, add, Equip/Prepare and grant/regrant confirmations clear on the next real input, while failures such as `UNSAVED` remain visible. DNDolphins likewise clears ordinary `Saved`/`Already saved`/catalog-save notices on the next real input.
- Kept the companion navigation contract consistent across Adventure, Bestiary, Journal, Initiative, Inventory and Spellbook: Short Back from a companion main screen returns to DNDolphins when present; Hold Back exits to firmware without launching DNDolphins. Sub-screen Short Back remains local navigation, with Initiative combat retaining its Short-Back previous-turn behavior.
- Updated all seven FAP manifests to release version 3.3.3 and refreshed documentation/test coverage for the retained behavior.

## 3.3.2 — Independent Inventory/Spellbook collections and companion profile handling

- Finalized standalone DNDInventory and DNDSpellbook collection UIs with independent source/assets ownership, direct entry to `+ Add New`, full 36-field Item and 17-field Spell editors, immediate persistence, Delete, custom-name editing, Hold-OK Equip/Prepare, A/P/K/F spell state marks and bounded eight-record collection paging.
- Restored collection/catalog parity: Item and Spell Name catalogs retain explicit `P# <>` paging, Item Catalog marks magic entries with `*`, Spell Class defaults to **All Classes** across a multiclass character, and the separate opt-in **All Spells** eligibility override remains available. Normal Inventory/Spellbook lists do not show persistent `<>` hints.
- Standardized active-character lookup on `/ext/apps_data/dndolphins/custom_active_profile.txt` (`Active=<id>`). Companion FAPs do not accept launch arguments as character selectors or discover a substitute character. Adventure/Journal require the exact persisted profile; Inventory/Spellbook load that exact ID through normal character storage; Bestiary/Initiative use ID `0` only when active-profile metadata itself is absent/unreadable.
- Added `[characterId]` at the top-right of the six companion **main screens only**. Detail/editor/tool/combat/result screens retain their own headers.
- Optimized Inventory/Spellbook startup and list interaction for constrained memory: fixed UI state is reserved before variable collection loading, drawing is cache-only, no timer/background worker runs during scrolling, no-op input events avoid redundant redraws, and storage reads occur only at startup, real cache-page boundaries, explicit catalog paging, edits and tool actions.
- Preserved DNDolphins Combat access to character-owned Inventory/Spellbook sidecars. Weapon Combat retains attack/damage/ammunition behavior; Spell Combat retains the mapped spell-damage table, Notes `XdY` fallback, cantrip scaling, upcasting and class-specific casting modifiers.
- Normal DNDolphins profile load no longer displays a redundant persistent `Loaded` notice.
- Standardized companion main-menu navigation so Short Back returns to DNDolphins when present and Hold Back exits without a parent handoff; redundant normal-menu Return/Open-DNDolphins rows were removed while no-character recovery prompts remain available.

## 3.3.1 — Inventory ownership, bounded campaign discovery and Journal continuation

- Moved Currency, encumbrance/carrying resources and starting-equipment controls into DNDInventory. Inventory is the sole normal owner of `Currency=` metadata; other Item appenders preserve that metadata and never synthesize it.
- Replaced automatic first-open equipment seeding with explicit **Grant Initial Inventory**. An absent Inventory stays absent until a real Inventory write/grant; a currency-only sidecar can still receive the normal grant; existing manual Items block the normal grant. Class/species/background assets remain the primary seed and a random d100 trinket is fallback-only when the normal seed yields no equipment/currency.
- Kept character-owned Inventory, Spellbook, Feature and applied-grant state centralized under `/ext/apps_data/dndolphins/` while allowing the companion FAPs to own their interfaces and asset packs.
- Added bounded campaign discovery/index handling and preserved install/activation controls without loading the full campaign catalog into memory.
- Journal → Adventure continuation now passes only continuation intent; Adventure resolves the persisted active character and resumes that character's stored campaign progress itself.
- Completed source-ownership/API cleanup so app-specific functions stay under their owning module and shared rules/storage interfaces remain neutral.
- Retained 4 KB stacks for Inventory/Spellbook after full character parsing showed the smaller reservation lacked safe margin; Initiative remains 3 KB.

## 3.3 — Inventory/Spellbook FAP split and loader-memory reduction

- Split Inventory and Spellbook out of DNDolphins into standalone FAPs while retaining DNDolphins' lightweight streamed access needed by Combat, progression and cross-feature integration.
- Preserved the recovered collection feature set: eight-record paging, Add New, full Item/Spell editors, immediate Add/Edit/Delete persistence, Equip/Prepare quick actions, custom names, catalogs, free-cast state, multiclass **All Classes**, and spell metadata filtering.
- Moved Item Catalog/starting-equipment assets to DNDInventory and the full Spell Catalog to DNDSpellbook; neither app loads the other collection's assets.
- Kept `inventory_{charID}.txt` and `spellbook_{charID}.txt` in the shared DNDolphins character-data root so all FAPs see the same authoritative records.
- Reduced DNDolphins loader/runtime pressure by removing the collection-heavy UIs from the main FAP without shrinking its 6 KB stack.

## 3.2.32–3.2.35 — Progression, multiclass casting and spell-catalog update

- Moved persistent Features and applied progression history from the core character file into lazy `feats_{charID}.txt` and `appliedgrants_{charID}.txt` sidecars. Features use bounded eight-record paging; applied-grant IDs are streamed during progression checks.
- Added deterministic supported species progression by total character level and normalized one-shot grant IDs. Ordinary player-selected class spell choices remain explicit rather than being auto-selected.
- Added Item Catalog filtering for All, Weapons, Armor, Ammunition, Gear, Tools and Magic while preserving Inventory-list Hold OK as Equip/Unequip.
- Corrected Eldritch Knight and Arcane Trickster casting to use Third-caster progression, Intelligence and Wizard-list eligibility, including correct multiclass slot contribution while Pact Magic remains separate.
- Completed the bundled streamed Spell Catalog metadata contract with Level/Class/School/Ritual/Source/Status filtering and bounded matching pages.
- Reduced progression memory pressure by removing continuously resident Feature/Grant collections and avoiding allocation for missing/empty progression sidecars.

## 3.2.6–3.2.31 — Inventory/Spellbook regression and recovery period

- This range is intentionally summarized as one extended recovery period. Inventory and Spellbook Add/Edit/Delete persistence regressed after 3.2.5/3.2.6-era changes and remained unreliable across many intermediate builds while the app was simultaneously being split into companion FAPs, converted to sidecar storage, and optimized for Flipper memory limits.
- Adventure, Bestiary, Journal and Initiative were progressively separated/hardened during this period, including standalone Journal/Initiative/Adventure FAPs, cross-FAP handoffs, bounded readers/caches, character best-effort loading, write-only shadows, saved encounters, spell-combat support, campaign/monster packs and the restored Initiative/Journal gameplay controls.
- Inventory and Spellbook moved from embedded character arrays to per-character sidecars with bounded eight-record paging. Multiple intermediate append/create/fingerprint/cache approaches were attempted; device testing showed that several builds could enter a blank editor but still failed to create/publish the collection file, expose newly added records in-session, support repeated adds/deletes, or safely cross an eight-record page boundary.
- The recovery ultimately returned Add New to a RAM-first lifecycle: stage/grow the resident collection first, enter the editor independent of storage success, then immediately rewrite the authoritative sidecar and only mark state saved after the write succeeds. Live files became `inventory_{characterId}.txt` and `spellbook_{characterId}.txt`.
- Final recovery work removed render-time SD I/O/cache mutation, kept the newly added record visible after editing, made user edits/catalog selections/deletes commit immediately, released catalog memory before collection rewrites, and fixed repeated-add/page-boundary behavior. Starting equipment again seeds only an empty Inventory, and Spellbook regained the multiclass **All Classes** filter.
- Hold OK quick actions were restored by the end of the period: Spellbook toggles Prepared/Unprepared and Inventory toggles Equipped/Unequipped, with immediate sidecar persistence and protected always-prepared spells.
- The same period also restored/expanded class progression, proficiency/slot/resource updates, item/spell catalog workflows, spell-attack mappings and related low-memory behavior. The detailed troubleshooting chronology for the broken Inventory/Spellbook path is intentionally not repeated per intermediate revision here because it spanned 3.2.6 through 3.2.31.

## 3.2.5

- Restored Bestiary Save Encounter/Add to Initiative for individual monsters, generated encounters and saved encounters; transferred monsters are appended to the active character roster before Initiative opens.
- Protected existing characters from accidental blank initialization, removed checksum-based rejection and expanded the saved Party Roster to 23 members.
- Added the low-memory Initiative launch path and current Roll for All/manual-roll/Back navigation behavior, plus wider scrolling labels.

## 3.2.4

- Added named saved encounters with resume, rename, duplicate, archive, delete and Add to Initiative actions.
- Added the encounter Difficulty simulator/composition warnings and tightened streaming/allocation release around large Bestiary encounter workflows.
- Added the transactional Bestiary-to-Initiative encounter handoff foundation later refined by the following releases.

## 3.2.3

- Changed Bestiary transfer into a one-button automatic launch after validation and release of large Bestiary buffers.
- Full encounters opened Initiative setup; single-creature transfers opened setup or active combat as appropriate while preserving name, quantity, initiative modifier, AC and HP data.
- Removed the extra prompt/Home step and retained transfer state safely when launch failed.

## 3.2.2

- Ensured required parent directories exist before writes and isolated/hardened the RogueMaster deferred-loader launch path.
- Added single-monster Add to Initiative and transferred AC/current/max HP; active combats could receive appended creatures without resetting round/turn state.
- Added stricter persistence, handoff and allocation-failure coverage.

## 3.2.1

- Persisted Bestiary party level/size immediately and hardened saved-encounter/Send-to-Initiative paths against allocation failures and NULL dereferences.
- Streamed initiative-modifier lookup and made unresolved saved-encounter transfers fail atomically instead of partially applying.
- Added sanitizer/fault-injection coverage for the hardened paths.

## 3.2

- Fixed the prior startup MPU/stack-overflow path by moving large save/migration temporaries off stack and restored larger Bestiary result pages.
- Added the initial saved-encounter management/encounter-to-Initiative workflow with composition warnings and validation.
- Added persistence/handoff validation and memory-release work that subsequent 3.2.x revisions refined.

## 3.1

- Advanced character saves to schema 3 with verified schema-2 rollback snapshots and an on-device rollback action.
- Added transactional campaign/monster pack installation with enable/disable controls and stable-ID conflict validation.
- Added Bestiary favorites, recents, named filter presets, saved encounters and faster stable-ID lookup with host regression coverage.

## 3.0.3

- Replaced byte-at-a-time profile/campaign/Bestiary reads with buffered readers and reusable record-offset caches.
- Added faster campaign/monster lookup while keeping packaged/custom data streamed from disk.
- Coalesced rapid edits into delayed autosaves and added heap-fragmentation/repeated-app-switch stress coverage.

## 3.0.2

- Restricted asset-to-data migration to mutable profiles, custom campaigns/progress and custom monsters while keeping packaged catalogs/campaigns/monster tables in app assets.
- Preserved the combined packaged-plus-custom Bestiary view and migrated legacy mutable files only after their app-data destinations were safely present.
- Kept existing app-data records authoritative and never overwrote matching destinations/profile IDs during migration.

## 3.0.1

- Moved mutable profiles/exports/archives/campaign progress/custom monster data into persistent app data with first-launch migration that never overwrote existing destinations.
- Prevented failed/corrupt/unsupported profile loads from autosaving a blank replacement and established schema 2 as the compatibility baseline.
- Reduced Bestiary result pages and hardened custom-monster legacy/open/write/recovery behavior while keeping packaged tables read-only.

## 3.0

- Added generated-encounter drill-down and full-screen stat-row reading while preserving browser/encounter selection state.
- Isolated custom monsters in atomic custom index/statblock files and streamed packaged/custom monster layers together without loading whole tables.
- Added confirmed custom-monster deletion, kept custom character text profile-local, reduced spell-picker memory and sorted packaged spells by level then name.
