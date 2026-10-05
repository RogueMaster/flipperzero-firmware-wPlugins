## 4.19.1
- **2026-10-04 compile/integration correction:** removed duplicate editor-release forward declarations; corrected missing lazy-runtime checks in common input, profile, Language/Proficiency and Settings paths; reacquired cache descriptors after final-page release; preserved Catalog selection before descriptor teardown.
- **Companion correctness:** Favorite Spells now builds/owns its bounded index and page on entry; Combat retains session Roll mode through Magic statistics; selected Hub Feats save before dependent review hands off to DNDGrants.
- **Current-source validation:** retired the inactive `.inc` and three source wrappers, updated test/audit/layout consumers and mode-specific object caching, added actual-input/lifetime/allocation-failure regressions, and passed eleven strict manifest builds/lifecycles plus fourteen ASan/UBSan regression executables. Regenerated current memory/executable/stack proxies and documented outstanding ARM/device gates.
- **Runtime memory/lifetime optimization:** replaced the monolithic shared-source include pattern with a normal `dnd_app_core.c` plus explicit Hub/Combat/Grants entry headers/translation units; reduced the 32-bit common app-state proxy from 4,676 B to **3,416 B** (1,260 B / 26.9%) by making profile browsing, catalog state, roll/dice state, grant-review state, collection cache/index state, page-offset tables and text-edit storage transient and moving Combat working state into a 248 B Combat-only runtime. Autosave/input hooks/editors are first-use/last-use resources; teardown now releases the catalog descriptor itself, streamed collection/cache descriptors are released as soon as their last page/index consumer exits, Catalog Back preserves return state before releasing that descriptor, and Bestiary no longer holds the global input-event subscription outside TextInput.
- **Build compatibility:** replaced bounded `%s` copy helpers that could trigger GCC `-Wformat-truncation` under `-Werror`, including the Bestiary text-edit completion path.
- **Roadmap cleanup:** removed unapproved and already-completed feature proposals so the roadmap contains no speculative feature list.
- **Reliability and save safety:** hardened Settings, Inventory bags, Features/Perks, Journal-to-Inventory handoffs, progression sidecars, Adventure campaign installation/registry updates, and Backup/Restore copies so interrupted SD reads/writes preserve the last complete data and clean up only files created by the failed operation.
- **Backup & Restore:** strengthened external SHD copy/restore staging, read-error detection, rollback behavior, backup-folder settings, and retry safety. Backup & Restore remains its own FAP and DNDolphins only launches it.
- **Adventure and Journal:** campaign-pack installation is transactional and retry-safe; milestone Journal creation and Journal-created Inventory Items now roll back safely on storage failures.
- **Collection safety:** fixed malformed short-line parsing, Language/Proficiency page ownership when switching record types, and first-create handling for Inventory/Main, named bags, starting equipment, and Feature/Perk sidecars.
- **Bestiary and shared settings:** party settings and other small authoritative settings/registry files now publish transactionally instead of truncating live files in place.
- **Audit and regression coverage:** expanded failure-injection tests for interrupted reads/writes, malformed Settings, path collisions, restore rollback, campaign-pack retry, and large Spellbook/Inventory/Feature/Language/Proficiency collections. The host ASan/UBSan suite remains green across all 11 FAPs.
- **Documentation and packaging:** reconciled feature ownership and current memory/evidence reports, corrected the 11-FAP documentation, refreshed the feature-evidence audit, and established that source/audit/release ZIPs never contain a `dist/` directory. Compiled FAPs are external build outputs.


## 3.6.7
- **Full 3.6.7 bug audit:** fixed DNDBackup & Restore's seven-row menu rendering beyond the 128x64 screen by converting it to a five-row scrolling window.
- Tightened Backup Folder validation to accept only `/ext` or `/ext/...`; strings that merely begin with `/ext` are no longer accepted.
- Clone Active Character now rolls the clone back if the refreshed profile index cannot be saved, preventing hidden partially-published clones.
- The DNDBackup profile-ID row is informational rather than a selectable no-op; focus starts on the first actionable row.
- Fixed Bestiary → Initiative imports being appended to persistent **Party Roster**, which caused monsters to be tagged as party members in history and blocked Monster Turn Tools. Imported monsters now enter the current encounter/combat list.
- Bestiary transfers now open Initiative's current combat/setup directly, preventing **Start New Combat** from overwriting a just-imported monster group.


- **Debug-only Progression Diagnostics menu:** DNDGrants now shows **Progression Diagnostics** as a normal Grant Review option only when the shared DNDolphins **Debug** setting is enabled. The previous hidden hold-Right shortcut was removed.

- Added **Clone Active Character** to DNDBackup & Restore. It allocates the next local profile ID and reuses the existing transactional profile-duplication path, including Spellbook, Inventory/Main and named bags, Features, applied grants, Languages and Proficiencies. The clone does not replace or switch the active character automatically.
- Added **Validate Character** to DNDBackup & Restore. It reuses the canonical profile verifier/parser and adds semantic checks for class levels, ability-score bounds and HP sanity without modifying the character.
- Added **Progression Diagnostics** to DNDGrants. From Grant Review, hold Right to inspect Applied, Pending, Pending Choice and Skipped/manual-review states. Up/Down selects a staged grant and OK exposes its stable ID/payload summary. No new progression storage or duplicate grant state was introduced.
- Removed the rejected/stale 3.6.7 roadmap section so the roadmap remains future-only.

## 3.6.6

- Added **Campaign Reward Preview** to DNDAdventure. Choices carrying an Item reward, milestone, quest flag, or achievement now show the pending reward/state changes before the choice is committed. **Apply Choice** runs the existing Adventure choice/reward path; **Cancel** returns without changing progress or character data.
- Audited editable TXT catalogs, metadata/grant sources, and Adventure campaign packs for content hash locking. Production loaders do not compare these files against shipped SHA/checksum/digest values. Existing Bestiary and Spellbook name/ID hashes are runtime lookup/index helpers, not content-integrity locks, so editing the TXT data remains supported.
- Removed the unused 3.6.6 Rules/customization/extensibility roadmap section. TXT editing remains the intended content-customization path.

## 3.6.5
- Journal Body/Notes now opens a full-screen wrapped note editor instead of the single truncated detail row editor.
- The full-screen note editor exposes an insertion cursor: Left/Right moves by character, Up/Down moves by wrapped line, and OK opens the full-screen keyboard to insert text at that exact cursor position.
- Hold Left deletes the character before the cursor; Hold Right deletes the character at the cursor. Back returns to the Journal entry without changing the Journal schema.

- Added **New Session Log** to DNDJournal. It creates an Adventure-category entry dated from the Flipper RTC with structured editable note sections for Party State, Milestones, NPCs / Monsters, Loot / Rewards, and Notes, using the existing Journal entry format.
- Added **Search Journal**. Search requires at least **3 characters**, performs case-insensitive partial-string matching, and checks both the entry title and the complete editable Journal note/body.
- Journal search streams entries from the active character's existing journal directory and retains only a bounded 24-result window; it does not sort or load the whole journal into RAM.
- Added a DNDJournal landing menu so Entries, Search, Session Log creation, and Return to DNDolphins remain distinct workflows.
- Removed completed/rejected Adventure/Journal QoL ideas from the future roadmap. Campaign Reward Preview remains the only 3.6.5 roadmap item.

## 3.6.4

- Added **Monster Turn Tools** to DNDInitiative combat participant editing. Non-party participants can launch directly into the matching DNDBestiary stat block by name and return to the same active Initiative combat.
- DNDBestiary adds a **Monster Turn Tools** action on stat blocks. It parses up to four common `Attack Roll:` entries from the monster's existing Actions text, rolls the d20 attack with the listed bonus, and rolls up to two listed damage dice expressions from the matching Hit text.
- Added **Combat History** to DNDInitiative. It browses the completed encounter files already written by **End + Save History**, newest first, and shows date/time, round count, party state, and surviving opponents without creating a second history format.
- Removed the rejected 3.6.4 roadmap items: Encounter Round Notes, Encounter Group Inserts, Initiative Condition Duration, Encounter Loot Packages, Party Damage/Healing Actions, and Reinforcement Waves.

## 3.6.3

- **Architecture audit:** moved external SHD bundle operations out of shared `dnd_storage.c` into `dnd_backup_storage.*`, linked only by DNDBackup & Restore.
- Removed the now-unused legacy `dnd_storage_export_profile()` and `dnd_storage_import_first()` APIs.
- DNDBackup & Restore now uses the native Flipper file browser for `.shd` restore selection instead of requiring a typed restore path.
- Added the packaged **10×10 1-bit SHD browser icon** at `dndbackup_images/shd_sword_10x10.png`; it reuses the correctly sized sword-style FAP artwork and is passed to the native browser for `.shd` files.
- External SHD bundles are validated and transactionally staged with rollback protection before the shared internal SHD restore engine updates live data.
- Backup destinations now support recursive `/ext/...` directory creation.

- Added **Favorite** state to canonical Spell records. DNDSpellbook sets/unsets favorites; DNDCombat adds **Favorite Spells** and routes selections through the existing casting/resource workflow. Favorite state is persisted in the Spellbook sidecar and therefore travels with normal SHD spell snapshots.
- Added standalone **DNDBackup & Restore** FAP. It stores a user-selected backup folder, creates a coherent current-level SHD bundle there, validates an external core SHD before restore, imports its matching companion SHDs, and uses the existing transactional SHD restore/rollback engine.
- Removed DNDolphins profile actions for **Export**, **Import First Export**, **Restore Backup**, and **Restore from SHD**, along with the embedded SHD restore screen/handler. DNDolphins now only launches DNDBackup & Restore for the active character.
- Suite now contains **11 FAPs**.

## 3.6.2 WIP

- Added Inventory and Spellbook name search with case-insensitive partial-string matching.
- Search requires at least 3 characters; shorter entries are rejected and do not scan.
- Search preserves the existing Inventory and Spellbook ordering and uses bounded lightweight result indexes rather than sorting/loading full records.
- Spellbook catalog search combines with the existing level/class/ritual/school/source/status filters.

## 3.6.1

- Added integrated Quick Rolls for abilities, saving throws and skills using the existing d20 roller. Abilities & Saves now visibly selects Check or Save with Left/Right, and OK rolls the selected type.
- Consolidated Character grant access into one Grant Review entry that launches the DNDGrants FAP.
- Added selectable loose-ammunition stacks when multiple matching stacks are available.
- Removed redundant planned/experimental Quick Bar, Concentration Helper, Rest Preview, Reaction Turn Sync, and Character Summary Card features.


## 3.6 memory/ownership cleanup

- Added a **2-second DNDolphins launch splash** using the native 128×64 monochrome project logo. It is shown only when DNDolphins is launched with no arguments; return-focus/deep-link launches skip it. DNDolphins now also owns the Home-menu 25×25 icon assets used by Graphical Menu Type; companion FAPs do not link those private graphics.
- Full post-integration audit corrected DNDGrants parent return routing so Grants now relaunches DNDolphins with `focus=character` instead of a null argument; returning from Grants therefore bypasses the no-argument startup splash just like the other companion FAPs. Graphical-menu validation now checks the final supplied artwork rather than obsolete placeholder-byte equality.
- Added standalone **DNDCharacter Sheet** FAP, launched from **DNDolphins → Character → Character Sheet**. It renders ten native 128×64 graphical pages covering identity/class, all abilities, saves, all 18 skills, combat/health, passives, spellcasting, defenses, senses and movement; Short Back returns with Character focused and Hold Back exits to firmware.
- Added DNDBestiary **Custom Encounter**: starts empty, adds monsters from the catalog/detail workflow, provides Difficulty Simulator/composition review, Save Encounter and Add to Initiative.
- Bestiary now enforces shared **Homebrew** across all custom-pack selection paths, including Source: Any, Favorites/Recents, generated/custom encounters and saved-encounter resume/Initiative; the startup projection remains slim at Debug + Homebrew only.
- Added DNDInventory **bags** with a Bag field above + Add New. Main remains `inventory_<id>.txt`, Group is permanent at `invGroup_<id>.txt`, named bags use `inv<BagName>_<id>.txt`, Resources aggregate all bags, and profile duplicate/archive/export/import plus SHD v3 preserve Inventory by bag.
- Added final-list **Bag Mover** with current-bag multi-select and destination-bag selection. Hold OK on a normal Item still toggles Equipped. Bulk moves publish source/destination together and repair container indexes.
- Added **Combat Utility Spells** above Rituals; Spell Attacks now contains only available attack-roll spells, while the utility list contains available non-attack spells without imposing a total-spell cap.
- Combat spell rows now show the resolved casting ability and attack modifier after each spell name, e.g. `(WIS/+5)`.
- **Unarmed Strike**, **Grapple**, and **Shove** are now separate persisted Attack Templates. Grapple/Shove use independent editable save-DC templates instead of synthetic rows derived from Unarmed Strike; older profiles gain the missing templates without overwriting existing ones.
- Spellbook owned rows now show a right-aligned three-character source tag from the class or actual Origin/Feat/grant name (`WIZ`, `HIG`, `MAG`, etc.).
- DNDBestiary now loads only the shared Debug/Homebrew projection at startup, hides Pack Diagnostics unless Debug is On, and exposes custom-pack monsters through a **Homebrew** Source filter when Homebrew is enabled.
- Added **Hold OK: Apply Level Grants** to the post-level review while preserving the existing Short OK ASI/Feat flow.
- Moved DNDolphins Home **Magic & Spells** management into DNDSpellbook; direct Magic launch and a visible terminal **Magic & Spells** row at the end of the Spellbook open the new view, while Arcane Recovery execution remains in DNDCombat.
- Reduced Languages/Proficiencies to a lazy four-record owned-list page and eight-row catalog page with no total-record cap; the 300-record host regression remains passing.
- Reduced the 32-bit DNDolphins fixed app-state proxy from 4,892 B to **4,616 B** after removing the permanently embedded Language/Proficiency page buffer and reserving two additional persisted Attack Template slots for Grapple/Shove migration.

## 3.6 — Catalog compliance, grants, performance and documentation

- Strengthened SRD catalog/grant validation across Character, Spellbook, Inventory, proficiencies, starting equipment, abilities/features and trinkets.
- Expanded SRD grant coverage and cross-checked **475 fixed SRD grant payloads** against SRD-valid spell, feat, language, skill, save, proficiency, size, resistance, sense and speed values.
- Kept DNDInventory as the exclusive owner of starting-equipment writes and added explicit **Review inventory grant** confirmation; primary-class saving throws and starting languages use reviewed grants.
- Consolidated Character and Spellbook onto one canonical **355-spell SRD catalog**, retained the Any/Cantrip/Level 1–9 Spellbook filter, and added Known/knowable/free-granted totals.
- Restored the 36 Homebrew/DNDolphins Item rows to the normal Item catalog, renamed user-facing **Extra Items** to **Get Elevated**, and retained the legacy `ExtraItems=` key only for save compatibility. Get Elevated controls randomized bundle granting; Homebrew controls catalog visibility.
- Corrected generic **Unarmed Strike** math: attack = ability modifier + Proficiency Bonus, damage = `1 + ability modifier`, and Grapple/Shove DC = `8 + ability modifier + Proficiency Bonus` before applicable attack-only modifiers.
- Reduced avoidable storage/heap work: Settings reads in 128-byte chunks with best-effort per-line recovery, DNDolphins defers the complete `Catalog: All` availability check until Settings is opened, Spellbook status filtering uses a fixed **128-byte negative prefilter** before exact matches, and the release gate keeps direct heap/storage work out of the audited draw helpers (currently **76 direct static draw helpers**).
- Completed the active Pocket-era namespace migration across types, enums, helpers and constants; only isolated read-only legacy file-format aliases remain for compatibility. New saves/pack manifests use DND naming. DNDInitiative remains at a **4 KB stack reservation**; its ~5.28 KB app state is heap-owned, not stack-owned.
- Reworked the 3.6 memory audit around Loader residency and allocation lifetimes. The earlier shared-shell checkpoint measured **4,892 B** after the persisted Menu Type addition; the later memory/ownership cleanup above removes the resident Language/Proficiency page buffer and reduces the current hub proxy to **4,616 B**. Debug mode retains heap checkpoints for startup diagnostics.
- Shared and Bestiary party settings remain small reconstructable state, but the 3.6.8 audit now publishes them transactionally through `.tmp`/`.bak` companions so interrupted writes cannot truncate live settings.
- Split **DNDCombat** and **DNDGrants** into standalone FAPs so DNDolphins tears down before either high-cost workflow starts. Combat adds **Jump to Initiative** with source-aware return routing: Initiative returns to DNDCombat only when launched from Combat; other Initiative launches return to DNDolphins.
- Added persisted **Settings → Menu Type** with **Text** (default) and **Graphical**. Graphical Home renders a 4×2 native icon grid, supports Up/Down/Left/Right navigation, frames the selected icon and places the selected menu name in the header. All 15 Home entries use supplied native 25×25 monochrome artwork with stable per-entry filenames for future independent replacement.
- Combat attack browsing remains streamed and unbounded by the 25-action menu; host regressions exercise **300 spells and 300 weapons** and reach the final record.
- Updated README, feature checklist, catalog/grant audits, memory audit and roadmap to reflect completed behavior and remaining work.

## 4.19.0a–c — Scalable collections, shared settings and table usability

- Removed former small owned Spell/Item/Feature ceilings while retaining bounded eight-record pages and stable collection ownership.
- Added shared Settings, SHD bundles/restore with rollback support, and scalable catalog-backed Languages and Proficiencies sidecars.
- Improved Combat casting information, Inventory ordering/quantity/filter controls, starting-equipment coverage and Spellbook sorting.
- Preserved bounded storage behavior and compatibility with older snapshots.

## 4.19 — Spell filters

- Added **Character Classes** as the default Spellbook class filter plus **Any Class** and bundled class filters.
- `Allowed` applies character spell-list/level eligibility; `All Spells` browses the selected catalog class directly.

## 3.5.x — Grants, choices, Inventory and Adventure

- Added progression Feat **Allowed/All** filtering, Level Choices, explicit deterministic grant actions and stronger grant persistence/status handling.
- Added completed-encounter history, Stack Qty editing, safer container deletion/remapping and improved Inventory paging.
- Added loose ammunition matching and consumption, Adventure full-scene viewing/checkpoint controls and additional bundled project content.
- Added fixed-average level-up HP/Hit Dice refresh and Journal milestone leveling integration.

## 3.4.x — Profile projections and navigation

- Added bounded Level-Up Review, narrow streamed profile projections and expanded structured spell-combat mappings.
- Improved Initiative large-roster navigation and DNDolphins Home/Combat menu organization.

## 3.3.x — Split FAP ownership and bounded collections

- Consolidated active-profile handoff and companion return-focus behavior.
- Moved Inventory/Currency and Spellbook ownership into their standalone FAPs with bounded paging, editors and immediate persistence.
- Added one-time starting-inventory regrant, generic Spell Scroll rows, Rituals combat support and draw-path stability work.
- Moved app-specific helpers out of shared modules and tightened allocation cleanup.
