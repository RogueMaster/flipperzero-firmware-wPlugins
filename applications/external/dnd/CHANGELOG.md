# Dungeons & Dolphins changelog

Released work only. Each released revision is retained as a concise summary; troubleshooting experiments and changes removed before a release are not expanded here.

## 3.2.31
- Added Record List Hold OK quick toggles for both sidecar collections: Spellbook toggles Prepared/Unprepared and Inventory toggles Equipped/Unequipped on the selected record. Always-prepared spells remain protected from manual unprepare.
- Quick prepare/equip changes immediately commit the active Spellbook/Inventory sidecar before showing the success acknowledgement; a collection write failure keeps the existing UNSAVED retry status instead of falsely confirming persistence.

## 3.2.30
- Corrected the Inventory/Spellbook tail-page state used after starting equipment or any existing collection. Add New now loads/prepares the actual final eight-record page, grows that resident page in RAM, increments the logical count, and immediately rewrites the live collection instead of manufacturing a one-record tail cache.
- Fixed the post-editor list focus bug that selected the newly added record while leaving the list scrolled to row zero. Returning from an Item/Spell editor now keeps the new record visible and keeps the five-row viewport inside one resident collection page.
- Removed collection page changes, SD reads/writes and item/spell buffer reallocations from Record List drawing. Page transitions now occur in input/screen-transition handling before the canvas callback, preventing render-time cache churn across eight-record boundaries.
- Item/Spell catalog memory is released before the completed catalog choice is committed to its collection, reducing peak heap overlap between the catalog page, the resident Item/Spell page and collection rewrite buffers.
- User edits to Item/Spell catalog choices, text fields, numeric fields and left/right adjustments now commit their collection immediately. Add New still writes immediately after staging the blank record; completed catalog population writes again with the final record contents. Quick spell preparation, free-cast consumption, spell casting with a free cast, and weapon-ammunition consumption also commit their sidecar immediately instead of waiting on the core autosave timer.
- Rechecked source stack frames for the affected paths: the collection item rewrite and item-page load remain well below the DNDolphins stack reservation, so the repeated-add MPU investigation is focused on page/cache/heap behavior rather than a single oversized local frame.

## 3.2.29
- Rebased manual Spellbook/Inventory **Add New** persistence on the proven pre-sidecar lifecycle: grow the resident record/count first, then write it, and advance the saved-state fingerprint only after the collection write actually succeeds.
- Removed the collection hot path's recursive parent-directory validation and now establishes `/ext/apps_data/dndolphins` with the same best-effort `storage_common_mkdir()` pattern used by the proven character save path before opening Inventory/Spellbook files. Starting-inventory creation and Journal Item creation use the same root-directory pattern.
- Consecutive adds no longer demand a successful save before every new record. Up to the resident eight-record page can grow in RAM while writes are retried; crossing to the next page requires the previous page to persist first. This restores the old 0→1→2 add/count behavior without abandoning bounded eight-record paging.
- Add New now immediately attempts the ordinary page rewrite after staging instead of running a separate pre-create/append transaction. The live `inventory_{characterId}.txt` / `spellbook_{characterId}.txt` file is verified to exist before a collection creation/copy operation is considered successful.
- Kept collection delete/edit paths on the same authoritative live files, so once the first page is published, subsequent edits, deletes, app-close flushes and catalog-loaded record changes share one save state instead of competing append and fingerprint states.

## 3.2.28
- Attempted to fix the remaining collection-file creation failure by replacing `FSOM_CREATE_NEW` with `FSOM_CREATE_ALWAYS`; device testing showed file creation still did not occur because the broader collection save state/path remained broken.
- Renamed live collection files to `inventory_{characterId}.txt` and `spellbook_{characterId}.txt`, making them structurally impossible to confuse with canonical `ch_{id}_{name}_{level}.txt` character profiles. Journal Item-entry creation now targets the same Inventory filename.
- Kept collection-local write failures retryable instead of permanently setting the whole DNDolphins session read-only; device testing showed repeated Add/Delete persistence was still blocked because the first collection file was not actually being published.
- Restored the Spellbook **All Classes** filter as the multiclass default. It now represents the union of spells currently eligible for every class on the character; choosing an individual class narrows the catalog to that class. Catalog selection resolves Source Class to the selected eligible class, or preserves/chooses an eligible class when All Classes is active.
- Added device-test coverage for first file creation, three consecutive adds, deletion, relaunch persistence, multiclass All Classes filtering, and retry-after-write-failure behavior.

## 3.2.27
- Fixed the remaining on-device Spellbook/Inventory **+ Add New** failure by restoring the proven editor-first interaction: a blank spell/item is staged in the resident collection window before storage I/O, so short or hold OK can enter the full editor even if the first persistence attempt fails.
- Replaced in-place `FSAM_READ_WRITE` collection appends with a synchronized snapshot/copy append path that uses ordinary read and write handles and a bounded 256-byte copy buffer. This also hardens progression/reward spell and item grants that share the append helpers.
- Extended paged collection window saves so a staged spell/item that lies beyond the current live end-of-file is appended correctly, including the first record, a partially filled page, and an eight-record page boundary.
- Kept the existing character schema, eight-record paging, header-only sidecar creation, and editor catalog workflow unchanged.

## 3.2.26
- Reworked the Spellbook and Inventory **+ Add New** path around header-only sidecars and direct editor-cache adoption; hardware testing subsequently showed the remaining storage-first transition still prevented Add New from opening on-device.
- Removed the post-append reread from Add New. The new spell/item is adopted directly into the resident editor cache, including an empty collection or an eight-record page boundary, avoiding the high-memory reader path immediately after append.
- Kept progression/reward spell and item appends resilient by ensuring the collection sidecar before writing and retaining rollback-to-original-length behavior on append failure.
- Restored Initiative full numeric editing for initiative total/modifier/AC/current HP/maximum HP, setup and active-combat participant editing, manual participant reordering, quick AC/condition controls, active-combat AC display, and explicit **End Current Combat**.
- Restored main-character HP/AC synchronization from Initiative to the canonical character file and automatic Turn/Encounter feature recharge. Character rewrites preserve unknown fields and abort rather than truncate an oversized line.
- Restored Journal milestone class selection, one-shot milestone level application, and Item-entry inventory creation while preserving Continue Adventure behavior.
- Kept character save structures unchanged; the restored companion features use existing character fields and app-owned sidecars.

## 3.2.25
- Storage/profile audit: verified the app-data roots for DNDolphins, Adventure, Journal, Initiative and Bestiary and documented the intentional Adventure-to-character/Journal bridges.
- Shared companion profile lookup now accepts only canonical `ch_<id>_<name>_<level>.txt` character files; sidecars and other `ch_*` files cannot be selected as profiles.
- If `custom_active_profile.txt` is missing or points to a deleted character, companion apps now fall forward to the next canonical character ID and wrap to the first, matching DNDolphins behavior.
- Journal and Adventure validate explicit handoff character IDs before using them. Journal no longer creates an orphan `ch_0` journal when no character exists.
- Initiative now falls back to the active/next canonical character when a stale explicit handoff ID is received, including Bestiary direct-launch transfers.
- Adventure can still browse/play campaigns without a character, but character progress, rewards and milestone Journal writes are not persisted until a real character is loaded.
- No save paths or save schemas were changed.

## 3.2.24
- Adventure campaign inbox installation now has a validation/details preview. Short OK on the inbox row validates the manifest/index and shows the campaign name/ID; Hold OK from the preview performs installation, and Back cancels without changing storage.
- Campaign installation remains non-destructive with respect to existing installed campaign content and preserves inactive registry entries.
- Progression and level-choice behavior remains explicit for player-selected spells, proficiencies, ASIs and feats; no automatic arbitrary choices or new persisted progression cache were added.
- Campaign pack controls retain cached rendering and transactional Active/Inactive toggles from the reliability pass.

## 3.2.23
- Reliability audit fixes: campaign pack Active/Inactive changes now roll back the registry if the enabled-campaign index rebuild fails.
- Campaign Pack Controls now cache visible rows outside the draw callback; rendering performs no registry reads or temporary pack-list allocations.
- Character Profiles refresh their storage window on screen entry/scroll changes rather than from the draw callback.
- Inventory Resources refreshes its aggregate on screen entry and renders from the cached aggregate instead of streaming the item sidecar during drawing.
- Bestiary filter/encounter/monster-pack row caches are bounded to their fixed buffers, preventing out-of-bounds writes when registries exceed the visible cache capacity.
- Campaign and monster pack controls remain non-destructive: Hold OK toggles Active/Inactive, installed content and registry entries are preserved.

## 3.2.22

- Matched Bestiary monster-pack controls to Adventure campaign packs: Hold OK toggles an existing pack Active/Inactive, short OK is a no-op on existing rows, and the inbox/install row keeps short OK. Pack registry entries and installed monster content are preserved when toggling state.
- Refined Adventure campaign-pack controls: Hold OK now toggles an existing pack Active/Inactive; short OK is intentionally a no-op on existing pack rows. The inbox/install row still uses short OK. Campaign registry entries and content files are never removed.
- Added a persistent **Level Choices** workflow that detects unclaimed ASI/Feat opportunities from current class levels without adding a new character save field.
- Added standard ASI choices: +2 to one ability or +1 to two different abilities, enforcing the normal score cap of 20.
- Added class-level ASI scheduling for the common levels 4/8/12/16/19 plus Fighter 6/14 and Rogue 10; completed choices are recorded through existing applied grant records so multi-level jumps and restarts remain recoverable.
- Added Feat selection through the existing Feats & Perks catalog; backing out cleans up the temporary feature record and leaves the level choice pending.
- Level-up status now prioritizes an outstanding ASI/Feat choice when one becomes available while continuing to report spell-choice and deterministic progression updates.
- Expanded verified deterministic SRD 5.2.1 progression metadata for Fighter, Barbarian, Bard, Rogue, and Wizard features while leaving choice-bearing proficiencies/masteries/spells for player selection.
- Changed Adventure campaign-pack management to be non-destructive. Campaign packs are never mark inactiveed or deleted by Adventure. Pack controls only mark a registry entry active/inactive; the registry entry and installed campaign content are retained, including files left by an interrupted/failed pack copy.
- Preserved lazy progression metadata reads, bounded campaign access, existing character save schema, and current FAP stack reservations.

## 3.2.21

- Hardened companion profile resolution so only canonical `ch_<id>_<name>_<level>.txt` character files qualify; Inventory and Spellbook sidecars can no longer be mistaken for a character profile.
- Kept character ID 0 fully valid by separating profile existence from the numeric ID value.
- Initiative now shows a dedicated no-character screen with **Launch DNDolphins** and **Exit Initiative** instead of entering combat state without a valid character.
- Initiative does not load or create an Initiative sidecar when no character exists, and unchanged main-character refreshes no longer cause an unnecessary SD-card rewrite.
- Bestiary now reserves its fixed UI/runtime allocations before migration, seed, recovery, and pack-index work; input subscription and marquee timer start only after storage initialization completes.
- Adventure now reserves its fixed dispatcher/view before variable-sized character/campaign cache work, reducing startup fragmentation risk.
- Re-audited the changed startup/failure/teardown paths without introducing a character-save schema change.

## 3.2.20

- Initiative roll behavior is participant-specific: each roster/combat member can use Normal, Advantage, or Disadvantage while the menu Roll setting remains the default for newly created members.
- Roll for All and individual generated rolls honor each participant's roll mode; direct numeric d20 entry remains exactly the entered die result plus that member's modifier.
- Main-character initiative refresh recognizes current Dexterity, Initiative Misc, exhaustion, the Alert origin/feature bonus, and Jack of All Trades when present.
- Initiative ties now use initiative modifier as the first deterministic tie-breaker while preserving stable order beyond that.
- Initiative editor exposes per-member roll mode and keeps monster/temp modifiers independent from the refreshed main character.
- Grant Initial Traits now stages species, background, class, and subclass grants into Grant Review before applying them, reducing accidental initial-build grants.
- Level-up progression continues to apply deterministic numeric/class progression, but reports when spell/cantrip allowances increase so the player chooses spells rather than receiving arbitrary spell selections.
- Level-up status reports newly applied deterministic traits or numeric rules refreshes.
- Removed a duplicate Initiative save call found during the 3.2.20 audit.
- No DNDolphins character-save schema revision was introduced; Initiative adds tolerant per-member RollMode fields to its app-owned sidecar.

## 3.2.19

- Promoted the corrected recovery build after a full source/asset consistency audit.
- Fixed Adventure campaign selection labels by preparing the five visible rows outside the draw callback, eliminating filesystem reads during rendering and preventing blank campaign names on-device.
- Fixed a duplicated Campaign Pack control branch introduced during recovery that could prevent a clean compile.
- Hardened progression grant application so unsupported or failed grants remain skipped instead of being incorrectly recorded as applied.
- Guarded proficiency-bonus calculation against malformed zero-level character data while preserving normal level-based scaling.
- Revalidated bundled campaign scene links, metadata row shapes/stable IDs, class progression references, Initiative restored controls, standard-array defaults, and bounded/lazy progression behavior without changing save schemas.
- Refreshed the main character's Initiative roster/combat modifier from the current character profile on Initiative launch using Dexterity + Initiative Misc - exhaustion penalty, while also refreshing that character's name, HP and AC without altering monster/temporary-member modifiers.
- Added persistent Initiative roll mode (Normal / Advantage / Disadvantage). Roll for All and individual automatic rolls honor the mode; hold-OK numeric d20 entry remains a direct entered die result plus the character modifier and is not rerolled or transformed.
- Added persistent main-character identity to Initiative state so character renames update the same roster/combat participant instead of creating a duplicate.
- Removed a duplicated Initiative helper declaration left by recovery reconstruction.

## 3.2.18

- Recovered the post-baseline progression and UI work: standard-array new characters, explicit Grant Initial Traits gating, deterministic level synchronization/grants, restored Initiative setup/edit controls, Adventure campaign-name display cleanup, and 26-character Bestiary full-line wrapping.

- Fixed the long-standing Spellbook/Inventory add flow: short OK on **+ Add New** now opens the relevant catalog and appends the selected complete record directly; hold OK keeps the blank/custom-entry workflow.
- Replaced whole-sidecar rewrites for simple spell/item appends with rollback-safe in-place appends that restore the original file length on write/sync failure, reducing temporary memory and SD write work.
- Fixed Inventory first-open behavior so default-equipment initialization writes the missing live sidecar directly and Inventory still opens if default seeding fails, allowing retry/manual recovery instead of appearing unresponsive.

## 3.2.17

- Added the bundled **Ghost Protocol** fictional authorized-security-audit campaign using existing Adventure checks, branching, rewards, flags, achievement and milestone support.
- Added bundled default custom monsters **Dolphin** and **Capybara**; Bestiary seeds them only when neither user custom file exists and never overwrites existing/partial custom data.
- Kept the five-FAP save structures, paging model and stack reservations unchanged.

## 3.2.16

- Split inventory/equipment/weapon behavior into `dndolphins_items.*` and spellcasting/resource behavior into `dndolphins_spells.*`, keeping shared character/dice mechanics in `dndolphins_rules.*`.
- Reconnected and regression-checked moved rule functions; made Inventory the only normal starting-inventory initializer and added the hidden d100 trinket.
- Removed Adventure's duplicate starting-equipment assets/initialization while preserving sidecar formats and bounded paging.

## 3.2.15

- Finalized declarative class/species/background starting equipment, direct live sidecars plus level/name `.swd` history snapshots, and streamed whole-collection spell/resource calculations.
- Fixed low-count eight-record page growth and Combat discovery from sidecars; renamed Bestiary `Difficulty Simulator` consistently to **Difficulty**.
- Reduced DNDolphins stack pressure by moving Verify Profile temporary state off stack and replaced Bestiary full-file lookup/browse caches with bounded sparse/recent caches and a smaller working page.

## 3.2.14

- Split owned spells/items into per-character sidecars with one escaped record per line and eight-record workflow paging; embedded spell/item fields stopped being read or written.
- Updated class/spell/item/combat/rest/Adventure workflows to stream or page the sidecars and separated core-character dirty tracking from sidecar-only changes.
- Preserved self-contained owned records while reducing resident collection memory and tightening companion-app stack reservations.

## 3.2.13

- Made character loading best-effort by recognized field name, removed whole-character consistency/heap vetoes and restored non-overwriting relocation of legacy character files.
- Enforced `.shd` as write-only history and tied shadow updates to actual changed-character saves.
- Hardened direct-launch active-character fallback, renamed source/entry-point ownership around the five FAPs, and standardized explicit absolute `.fap` handoff paths.

## 3.2.12

- Reworked the character picker and Journal into storage-backed bounded metadata caches; Journal bodies load only when opened and are no longer limited by a full-entry resident array.
- Removed the large resident Combat row block and formatted only visible rows on demand.
- Kept structured spell-combat mappings authoritative with guarded Notes dice fallback and removed dead helpers for strict firmware builds.

## 3.2.11

- Added same-stem write-only character shadows, shared Combat Normal/Advantage/Disadvantage attack mode and minimum-XP floors on level increases.
- Split DNDAdventure into its own FAP and made it sole owner of campaign state/progress while keeping one-way milestone output to Journal.
- Kept character files directly under the DNDolphins app-data root and preserved teardown-before-launch handoffs.

## 3.2.10

- Split Journal and Initiative into standalone FAPs with independent app-data namespaces and removed their resident state from DNDolphins.
- Moved Journal entries to timestamped per-character files, restored newest-first ordering without firmware `qsort`, and removed broad old-schema conversion code.
- Returned DNDolphins to an 8 KB stack after the source-based OOM cleanup.

## 3.2.9

- Reduced DNDolphins to an 8 KB stack through bounded paths/readers, lazy Adventure/Journal ownership and tighter shutdown/startup memory handling.
- Added automatic spell-slot/Pact initialization, direct slot editing and completed a large structured Combat Spell Attacks mapping pass with multi-part/healing/special dice handling.
- Kept Notes `XdY` fallback bounded while hardening malformed spell/dice input and reducing startup/transition heap fragmentation.

## 3.2.8

- Reduced dynamic character-array and spell-storage growth pressure, reduced catalog working pages and released workflow allocations when leaving those workflows.
- Removed full-character duplicate allocations from verification/migration paths.
- Expanded structured Spell Attacks mappings and added guarded `XdY`/`XDY` Notes fallback for otherwise unmapped spells.

## 3.2.7

- Moved Bestiary into the fixed Home menu and added Combat > Spell Attacks with slot/Pact/point/free/ritual resource handling and mapped spell rolls.
- Hardened low-memory startup/shutdown, publication rollback and manually editable numeric parsing; set the main app stack to 9 KB.
- Extended Bestiary initiative handoff metadata and fixed low-memory monster-cache/migration recovery paths.

## 3.2.6

- Added the dedicated Adventure skill-check result screen showing natural d20, modifier, total, DC and pass/fail until OK is pressed.
- Added explicit **Start Adventure**, expanded the scene-text layout and made Retry Save visible only after a save failure.

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
