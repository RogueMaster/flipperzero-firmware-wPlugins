# Dungeons & Dolphins changelog

## 3.0

- Enabled generated-encounter drill-down: short OK on an encounter monster now opens its complete stat block, and short OK on any stat row opens that attribute in the full-screen reader.
- Preserved the selected encounter monster, encounter scroll position, selected stat row, and stat-block scroll position while navigating into and out of full-screen details.
- Released the Bestiary's lazy text editor before allocating browser windows, generated encounters, diagnostics, and monster details to lower peak heap use.
- Isolated user-created monsters in atomic `custom_index.txt` and `custom_statblocks.txt` files so creating, editing, or deleting them never rewrites the packaged Bestiary tables.
- Streamed packaged and custom monster layers as one browser and encounter pool without loading either complete table into RAM.
- Added a visible, custom-only Delete Monster row with two-step confirmation; packaged monsters remain read-only.
- Kept custom spell, species, class, subclass, background, feat, and item text profile-local so assigning custom character options never rewrites a packaged catalog.
- Reduced the spell picker from 50 retained records to ten retained records per streamed page, cutting its bounded catalog allocation by about 80 percent.
- Sorted the packaged spell catalog by spell level and then alphabetically, with a bounded in-app page sort for catalog additions.

## 2.7.2

- Added a full-screen reader for every monster stat-block line item.
- Added word-aware 20-character wrapping with a five-line viewport, one-line Up/Down scrolling, and Left/Right page movement.
- Made OK and Back return from the reader to the exact previously selected stat row.
- Kept custom-monster editing on a separate explicit row so ordinary stat lines always open for reading.
- Replaced fixed maximum-size spell, feature, item, journal, and structured-grant arrays with grow-on-demand allocations.
- Reduced the empty character save object from about 34.6 KB to about 6.3 KB to prevent direct-launch and first-profile memory exhaustion.
- Preserved the existing record limits and text-save layout while adding allocation checks to every record creation and loading path.
- Replaced unsafe unbounded UI string formatting with explicitly bounded text composition so strict RogueMaster builds no longer fail on `-Wformat-truncation`.
- Expanded record-detail row storage so long spell, feature, item, and journal notes remain available to the horizontal scroller instead of being cut to a short display buffer.
- Enlarged the Bestiary full-screen reader position buffer for the maximum `start-end/total` range reported by its 16-bit line counters.

## 2.7.1

- Buffered catalog and structured-grant reads to eliminate hundreds or thousands of one-byte SD operations.
- Stopped catalog scans after filling the current 50-record page and detecting one additional match.
- Replaced nine catalog heap allocations with one bounded allocation and released it before autosave and grant lookup.
- Made text and number input modules lazy so they consume no heap during startup or catalog browsing until first used.
- Skipped redundant full-profile writes when the save-state fingerprint is unchanged.
- Added automatic standard armor and weapon stat population from the selected inventory name.
- Changed the new-character alignment default to True Neutral.
- Removed runtime translation loading and pre-release character-save migration paths.
- Expanded party presets with Armor Class, current HP, and maximum HP, and copied those values into new combats.
- Added a hold-OK initiative participant editor for name, roll, modifier, Armor Class, current/maximum HP, conditions, and armed deletion.
- Allowed negative current HP in active initiative combat and direct participant entry.
- Replaced Bestiary one-byte reads and repeated per-monster diagnostics with buffered reads and one-pass pack validation.
- Cached Bestiary filter totals across page changes and stopped page reads as soon as the requested window is full.
- Added reciprocal final-menu launch entries between Dungeons & Dolphins and Dolphin Bestiary.
- Clarified the manual save row as Retry Save / Status; ordinary changes continue to autosave.
- Updated encounter generation to target the selected party-level/size budget without exceeding it, distinguish adjacent difficulty tiers, and cap creatures at two per party member.
- Advanced the current pre-release character format to schema 2 for complete party presets; older schemas remain intentionally unsupported.

## 2.7

- Expanded the species/lineage selector to 103 names, including ancestry-specific Dragonborn, Dwarf, Elf, Genasi, Gnome, Goliath, Halfling, and Tiefling choices.
- Expanded the subclass catalog to 139 class-associated choices across all 13 bundled classes; the picker defaults to the selected class and retains its All view.
- Corrected list-parent navigation so Classes returns to Character, Spells returns to Magic, and Features/Perks and Inventory return to their opening screens instead of reopening the last record.
- Added long-Back navigation from ordinary screens to Main and long-Back exit from Main in both applications.
- Replaced selected-row ellipses with timed horizontal scrolling while keeping unselected rows allocation-free.
- Added short-OK numeric currency editing for Copper, Silver, Electrum, Gold, and Platinum.
- Added a nine-option alignment picker covering every Lawful, Neutral, and Chaotic combination with Good, Neutral, and Evil.
- Added reusable hold-OK number entry for Vitals, ability and save adjustments, skill adjustments, spell settings and slots, class and record fields, dice settings, and combat values.
- Extended long-Back handling through text and number entry modules so it still returns directly to Main.
- Added Guidance dice mode, which rolls and displays a d4 alongside a d20 and includes both dice in the total.
- Removed separate character-catalog overlay scans; additional records now extend the normal class, subclass, species, background, feat, spell, item, and metadata files.
- Consolidated 340 packaged stat blocks into one streamed section file and integrated editable custom monsters into the same index/stat-block framework with atomic two-file rollback.
- Deferred profile, catalog, and monster scans until needed, reused known profile paths during autosave, and released large catalog/detail allocations when their screens close.
- Reworked encounter generation into a single streaming eligibility pass with a bounded randomized candidate pool, eliminating repeated full-index scans that could stall the interface.
- Updated the feature documentation and future-only roadmap for the performance-focused release.

## 2.6

- Split the monster reference and encounter generator into the independent Dolphin Bestiary FAP while keeping both applications in one `application.fam` with explicit, mutually exclusive source lists.
- Expanded the packaged bestiary from 220 to 340 unique records and retained combinable browser filters, encounter roles, custom lifecycle controls, and diagnostics.
- Limited character catalogs and bestiary result windows to 50 records, with on-demand allocation and release when each picker, stat block, or encounter closes.
- Moved all runtime reads and writes to each application's `APP_ASSETS_PATH` namespace and prefixed user-maintained reference files with `custom_`.
- Fixed transactional character creation and profile-index refresh so a new Main or New Hero remains visible, while failed writes restore the prior active profile and display an unsaved warning.
- Added a dedicated Back to Main Menu row to Magic and retained normal Back navigation from spell lists and detail screens.
- Shortened passive-stat labels to `Pass.` without abbreviating their governing ability labels.
- Moved attack templates and all combat-state fields into Combat, then removed the redundant Combat Sheet, Character Builder, About, and embedded monster menu entries.
- Kept `Roll Now` stable while dice modifiers are edited and reset it only after a completed roll.
- Replaced the launcher art with a validated 10x10, 1-bit d20 icon showing 20.

## 2.5

- Added another 100 redistributable open-reference monster records, bringing the bundled total to 220.
- Added an on-device ten-cycle stress runner for catalog, campaign, monster, profile-index, and encounter allocation/release paths.
- Added SD write-failure detection, read-only fallback, persistent unsaved warnings, failure counts, and an explicit retry-and-save control.
- Published a case-by-case physical-device matrix covering controls, truncation, long sessions, power interruption, SD removal, and low-memory behavior without claiming unperformed hardware results.

## 2.4

- Added create/edit/delete controls for every attack-template field, including save actions, Mastery, damage riders, and recharge cadence.
- Added a structured-grant editor for stable ID, source, option type/name, prerequisites, class association, gained level, payload, and status.
- Preserved grant payloads when applying them so reviewed metadata remains editable.
- Added optional runtime language packs for navigation and field labels with a compile-time 2 KiB heap ceiling, live heap reporting, and English fallback.

## 2.3

- Added an on-device campaign selector for bundled and user packs.
- Added atomic per-profile/per-campaign progress files containing scene, checkpoint, quest flags, and achievements.
- Added versioned manifests and diagnostics for compatibility, missing files/entries, duplicate IDs, and broken links.
- Added a documented third-party campaign format and ready-to-copy SD-card starter template.

## 2.2

- Added full on-device editing for existing custom monsters while preserving stable IDs.
- Added armed two-step custom deletion and kept bundled records read-only.
- Added atomic user-index rewrites, transaction-journal recovery, orphan completion, and interrupted-edit rollback.
- Expanded the custom editor to include size/alignment, role, skills, defenses, and extra actions.

## 2.1

- Added 100 redistributable open-reference monster records, bringing the bundled total to 120.
- Added combinable source and environment filters to the monster browser.
- Added optional Leader, Controller, Skirmisher, Artillery, Brute, and Minion metadata with role-aware encounter weighting.
- Added diagnostic navigation that reports the exact record ID, stat-block filename, and first missing or invalid field.

## Unreleased documentation update

- Audited every completed roadmap claim against the implemented code, packaged assets, tests, and build records.
- Consolidated completed release history in this changelog and removed it from the roadmap.
- Replaced the roadmap with release-scoped future work containing exactly three testable features per release.

## 2.0

- Declared monster pack schema 1 stable and documented its compatibility contract.
- Made custom stat-block writes interruption-resistant by publishing the block before its index record.
- Added live free-heap and largest-block readings to Pack Diagnostics.
- Expanded the bundled compendium to twenty creatures with ten original, freely redistributable entries.
- Completed the five-release monster and encounter roadmap established after version 1.5.

## 1.9

- Added an on-device custom monster editor.
- Added editable challenge/XP, defenses, identity, environment, movement, abilities, senses, languages, traits, and actions.
- Added direct persistence into the versioned user monster pack with collision-resistant IDs.

## 1.8

- Added an explicit monster-pack format version.
- Added field-level validation for required stat-block sections.
- Added bundled/user version reporting and incompatible-version warnings to Pack Diagnostics.

## 1.7

- Added Balanced, Horde, and Elite encounter templates.
- Added weighted environment selection so themed creatures are preferred without making small packs unusable.
- Tuned target-budget usage and creature-count limits by template.

## 1.6

- Added case-insensitive monster-name search.
- Combined name, maximum-challenge, and creature-type filters in the streaming browser.

## 1.5

- Added on-device monster-pack diagnostics.
- Added checks for missing stat blocks and duplicate stable IDs.
- Added a ready-to-copy community monster-pack template with a complete example record.

## 1.4

- Added environment metadata and themed encounter generation.
- Added Aquatic, Dungeon, Planar, Urban, Wilderness, and unrestricted themes.
- Added a composition toggle for repeated creature types versus mixed-only groups.

## 1.3

- Added generated-encounter transfer into Initiative.
- Prefilled monster names, quantities, HP, Armor Class, and Dexterity-based initiative modifiers.
- Capped transfers safely at the initiative tracker capacity.

## 1.2

- Added challenge-rating and creature-type filters to the monster browser.
- Made filter changes immediately rebuild the disk-backed result count without retaining an in-memory catalog.

## 1.1

- Added a disk-backed monster compendium and on-device stat-block browser.
- Added party-level, party-size, and difficulty controls for random encounters.
- Added per-character XP budgets for Low, Moderate, and High encounters.
- Added safety limits for above-level creatures, oversized groups, and over-budget results.
- Added bundled monster assets and a documented SD-card extension format.
- Kept the monster index streaming and stat blocks lazy-loaded to protect heap memory.
- Updated the feature list, tests, roadmap, and build verification for the new release.

## 1.0

- Declared schema 1 stable for future forward migrations.
- Retained one prior successful save generation for every active profile.
- Added an explicit profile backup-restore action alongside checksum verification.
- Added zero-allocation translation hooks and a community translation template.
- Added compatibility, accessibility, catalog policy, stable schema, and reproducible-build documents.
- Added a release verification script that runs host tests and the RogueMaster FAP target.
- Finalized source-only release packaging with no compiled FAP or `dist` directory.

## 0.9

- Froze character save schema 1 and documented its compatibility contract.
- Replaced compiler-layout checksums with checksums over canonical serialized file bytes.
- Added automatic one-time migration from the 0.8 text layout.
- Added profile actions for rename, duplicate, export, first-valid-export import, archive, delete, and checksum verification.
- Kept duplicate, export, and archive operations chunked so they do not require a second character-sized allocation.
- Added host-side tests for calculations, dice bounds, catalogs, metadata IDs, parsers, checksums, migration acceptance, spell filtering, manifest fields, and release-document wording.
- Added a physical-device test matrix for hardware verification.

## 0.8

- Added a data-driven Adventure mode with compact sprite-and-text scenes.
- Added selectable choices, character-based skill checks, and success/failure branches.
- Added per-character story location, quest flags, achievements, and checkpoints.
- Added adventure inventory rewards and milestone journal rewards with duplicate protection.
- Added a bundled sample campaign and editable SD-card campaign override.
- Kept campaign scene memory lazy and released it when Adventure mode closes.

## 0.7

- Changed the internal application ID and SD namespace to `dungeons_and_dolphins`.
- Added FAP-packaged file assets for catalogs and metadata, with app-data overlays for user changes.
- Reworked catalog memory into lazy heap allocations that grow only while a picker is open and are released on exit.
- Replaced full-name diagnostic storage with compact hashes and removed the second full-character allocation during save.
- Added graceful allocation failure handling and catalog-memory status reporting.
- Added structured grants with a review/apply/skip screen.
- Added species, Origin Feat, tool proficiency, armor training, weapon training, size, and senses fields.
- Added annotated catalog metadata, strict import validation, and on-device catalog diagnostics.
- Added class-specific Hit Point Dice and pools for multiclass characters.
- Added per-class spellcasting modes, abilities, limits, spellbook size, Pact slots, Mystic Arcanum, and spell points.
- Added multiclass shared-slot calculation and spell filters.
- Added spell stable ID, source, school, ritual, grant type, and grant-name tracking.
- Added conditions, concentration, reactions, temporary effects, defenses, movement modes, and attack templates.
- Added initiative HP, Armor Class, conditions, encounter history, and undo for turn, HP, and feature-resource changes.
- Added containers, weights, carrying capacity, Armor Class formulas, attunement warnings, ammunition groups, and charges.
- Added resource formulas and recovery cadences beyond rests.
- Added coin normalization and optional encumbrance tracking.
- Counted container contents in total carried weight while retaining container organization.
- Added a community-maintainable metadata pack and generator.
- Updated the roadmap and full feature documentation.

## 0.6

- Added SD-card-limited profiles with dynamic profile indexing.
- Added profile filenames in `ch_{id}_{characterName}_{characterLevel}.txt` format.
- Expanded class, subclass, spell, item, background, and feature-name catalogs.
- Added complete README and changelog documents.
- Used the manifest version macro in the About screen.

## 0.3

- Added separate readable character save files, profile switching, custom catalogs, dice animation, autosave, and the 10×10 application icon.

## 0.2

- Added all saving throws and skills, miscellaneous modifiers, passive statistics, expanded spell states, and free-cast recovery.

## 0.1

- Added the initial character tracker, multiclass records, inventory, spells, journal, weapon rolls, party roster, and initiative tracker.
