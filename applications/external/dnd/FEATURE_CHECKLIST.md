# Feature checklist

4.20.3 source/host feature coverage for eleven FAPs plus five FALs. Checked features are implemented; physical-device acceptance is tracked separately in DEVICE_TEST_MATRIX.md.

## DNDolphins

- [x] DNDolphins startup shows one randomly selected 128×64 image from the original plus fourteen supplied splashes, with animated hourglass. No-argument launch retains a two-second minimum; argument/deep-link launches show loading without the minimum. DNDolphins privately owns the Graphical Home 25×25 icon assets while companion FAPs do not link them.
- [x] Multiple character profiles with create, switch, rename, duplicate, archive, delete and save-check actions; user-facing backup/export/restore is owned by the standalone DNDBackup & Restore FAP.
- [x] Character actions launch **DNDBackup & Restore** for user-facing SHD operations. Its native `.shd` browser restores the exact selected active-character snapshot plus the matching Inventory, Spellbook, Feature, applied-grant, Language, Proficiency and v3 bag companions transactionally.
- [x] Character identity, species, background, alignment, multiclass levels/subclasses, XP/Milestone leveling, languages, proficiencies and Inspiration.
- [x] Bundled class selection configures the class Hit Die and spellcasting mode; recognized older/imported classes can receive missing class-derived spellcasting setup.
- [x] Subclass catalog defaults to the selected class and supports a class-filtered / All toggle.
- [x] Standard-array defaults plus six abilities, saving throws, 18 skills and passive Perception/Insight/Investigation.
- [x] Fresh-character defaults include the standard starting profile state and five editable starter attack templates: Unarmed Strike, Grapple, Shove, Spell Attack and Saving Throw Action.
- [x] Vitals including current/max/temp HP, AC, speed, initiative, exhaustion, death saves and Hit Dice.
- [x] Fixed-average HP gain and Hit Dice refresh when class level increases.
- [x] Explicit ASI/Feat level choices with dedicated ASI +2 and ASI +1/+1 application.
- [x] Increasing class level raises XP to at least the minimum threshold for the resulting total level.
- [x] **Grant Initial Traits** and **Apply Level Grants** save/tear down DNDolphins and launch standalone **DNDGrants**, which stages eligible grants into a mandatory review/choice screen; 24-record batches resume by byte cursor and dependency scans are bounded. The post-level review adds **Hold OK** as a direct Apply Level Grants handoff while preserving Short OK ASI/Feat flow.
- [x] Grant Review supports Apply All plus Pending/Skipped grant records, editing unresolved grants and adding custom grant records.
- [x] Character-owned Feature/Perk records with uses, recharge cadence, source class and resource formulas.
- [x] Progression Feat picker defaults to **Allowed**; Hold OK toggles **Allowed / All**.
- [x] Class-level spellcasting progression remains character-owned: casting mode/ability, class limits, Pact data and level-derived spell progression stay with the character even though Magic & Spells management is now DNDSpellbook-owned.
- [x] Home **Magic & Spells** saves/unloads DNDolphins and launches DNDSpellbook directly on its Magic view, removing that management UI from the hub runtime.
- [x] General Dice Roller with d4/d6/d8/d10/d12/d20/d100, quantity, modifier, Advantage, Disadvantage and Guidance.
- [x] Dice Roller Guidance automatically adds 1d4 to supported d20 rolls; Advantage/Disadvantage return to Normal when the dice setup no longer supports those modes.
- [x] Home **Settings** after Journal with shared **Skip Dice Loading**, **Debug**, **Get Elevated**, **Catalog**, **Homebrew: Yes/No**, and **Menu Type: Text/Graphical** settings persisted under the DNDolphins data root; Catalog defaults SRD, Homebrew defaults Yes and Menu Type defaults Text. Graphical Home uses the native 25×25 DNDolphins icon set with the selected menu name in the header.
- [x] **Skip Dice Loading** bypasses DNDolphins roll animations for Dice Roller, Hit Dice, weapon and spell-combat rolls; Adventure/Initiative read the same setting and already present their roll results without a loading animation.
- [x] Launch/return integration with Grants, Combat, Inventory, Spellbook, Bestiary, Initiative, Adventure and Journal.

## DNDGrants

- [x] Standalone Grant Initial Traits and Apply Level Grants review/choice workflow with bounded 24-record batches and resumable metadata cursors.
- [x] No-grants, completed review and Short Back return to DNDolphins; Hold Back exits.
- [x] Catalog All availability is checked on DNDGrants launch only when the saved Catalog mode is All.

## DNDCombat

- [x] Standalone 26-action Combat menu with weapon attacks, attack-roll Spell Attacks, **Combat Utility Spells**, Rituals, attack templates, HP/rest/Hit Die recovery and combat-state controls.
- [x] Attack/utility spell lists use bounded eight-record windows with independent `uint16_t` totals; host regression reaches weapon #300 and spell #300, so menu count is not an attack cap.
- [x] Spell Combat rows show the resolved source-class casting ability/attack modifier, e.g. `(WIS/+5)`. Attack Templates keeps **Unarmed Strike / Grapple / Shove** adjacent; Unarmed Strike uses `1 + ability modifier` damage and Grapple/Shove use the SRD save-DC calculation without removing editable custom templates.
- [x] **Jump to Initiative** launches DNDInitiative with an explicit Combat source handoff; Short Back returns to DNDCombat only for that launch source.
- [x] Short Rest retains Wizard Arcane Recovery spending/undo behavior; moving Magic management to DNDSpellbook does not remove the Combat recovery workflow.


## DNDCharacter Sheet

- [x] Character Sheet FAL opens inside the Hub Character menu; standalone **DNDCharacter Sheet** (`dndcharactersheet.fap`) remains a thin direct-launch wrapper.
- [x] Exact active-profile loading; no cross-character fallback.
- [x] Native 128×64 read-only graphical sheet with ten pages and boxed character-sheet-style hierarchy.
- [x] Shows character identity/class/level/XP, all six ability scores/modifiers, all six saving throws, all 18 skills, Proficiency Bonus, Inspiration, passive Perception/Insight/Investigation, AC, Initiative, Speed, current/max/temp HP, Hit Dice, Death Saves, spellcasting ability/attack/DC/slots, conditions, defenses, senses and movement.
- [x] Short Back returns to DNDolphins with Character focused; Hold Back exits to firmware.

## DNDInventory

- [x] **Bag Mover** supports Short OK checkboxes and Hold OK anywhere to choose the destination; terminal Move Selected and Back cancellation remain available. Hold OK on normal Item rows remains Equip/Unequip.
- [x] Item Editor Container cycles Main/Group/custom bags and immediately moves the Item without another screen; editing continues at its destination, preserving fields and ignoring directional Repeat.

- [x] Opens directly to the active character's Inventory with **Bag** first, **+ Add New** second and **Currency** third; owned Items follow, then Inventory Resources, Grant Initial Inventory and final **Bag Mover**.
- [x] Explicit **Review inventory grant** confirmation before the normal **Grant Initial Inventory** transaction, plus a reviewed one-time Hold OK regrant override; empty Inventory launch never auto-grants.
- [x] Eight-record bounded Inventory paging with `PgX<>` in the upper-right when owned Items exceed one page.
- [x] Full Item editor with quantity, weight, equipment/attunement, weapon, damage, ammunition, charges, armor, container and source fields.
- [x] Item Name catalog with All, Weapons, Armor, Ammunition, Gear, Tools, Instruments, Trinkets, Mounts/Vehicles, Potions, Rings, Rods, Scrolls, Staffs, Wands, Wondrous and Magic filters; Hold OK opens the selectable filter-category list.
- [x] Item catalog keeps the 579 SRD rows plus 36 project Homebrew/DNDolphins rows in the normal streamed file; Homebrew filtering is applied while browsing and owned Inventory remains unaffected.
- [x] Inventory **Bag** selector sits above + Add New; Main/Group are permanent, custom bags are add/remove capable, selected-bag pages remain streamed, Resources aggregate all bags, and duplicate/archive/export/import/SHD preserve bag separation.
- [x] Recognized bundled weapon/armor selections populate useful mechanical presets including weight, damage, properties, Versatile die, ammunition family, armor AC/DEX cap and shield bonus.
- [x] Generic Spell Scroll entries for Cantrip and Levels 1–9 with level-appropriate rarity.
- [x] Musical-instrument variants and the d100 trinket set are cataloged in their own categories; Tea Set is available as mundane Gear.
- [x] Fallback d100 trinkets use the SRD 5.2.1 table and remain catalog-backed.
- [x] Every exact Item name used by bundled starting-equipment grants is searchable in the Item catalog; project naming aliases are labeled DNDolphins rather than SRD.
- [x] Hold OK quick Equip/Unequip.
- [x] Hold Left/Right on an owned Item changes Stack Qty by -5/+5; the Item editor retains normal single-step Left/Right and full numeric Hold OK entry.
- [x] CP/SP/EP/GP/PP currency editing and coin normalization.
- [x] Carried/equipped weight, capacity, Standard/Variant encumbrance, attunement count and calculated armor/shield AC.
- [x] Normal carrying capacity uses Strength × 15 lb with an explicit carrying-capacity override.
- [x] Attunement counts against the normal three-item limit and warns when exceeded without forcibly removing attunement.
- [x] Container references with safe index remapping when Items are deleted.

## DNDSpellbook

- [x] Opens directly to the active character's Spellbook with **+ Add New**.
- [x] Eight-record bounded Spellbook paging.
- [x] **Magic & Spells** view owns casting ability, Spell Attack/DC misc, Known/knowable/free-granted summary and shared slot current/max editing; DNDolphins can launch directly into this view and the terminal **Magic & Spells** row at the end of the Spellbook opens it.
- [x] Magic & Spells shows Arcane Recovery status without duplicating recovery spending; Short Rest / Arcane Recovery execution remains in DNDCombat.
- [x] Full Spell editor with source class, level, Known/Prepared/Always Prepared/Ritual, free casts and catalog/source/school/grant metadata.
- [x] Blank **+ Add New** spells and catalog-selected spells are automatically marked Known.
- [x] Hold OK quick Prepare/Unprepare for Known spells.
- [x] Owned Spells kept in level-ascending, case-insensitive alphabetical order.
- [x] One canonical 355-spell SRD catalog owned by DNDolphins and streamed by both Character and Spellbook, sorted by level then name, with every fixed SRD spell grant required to resolve in it.
- [x] Filters for Level, Class, Ritual, School, Source, Status and Eligibility.
- [x] **Character Classes** default class filter, plus **Any Class** and all bundled SRD class filters.
- [x] **Allowed** character eligibility and **All Spells** catalog browsing modes.

- [x] Catalog selection resolves a compatible owned Source Class and marks the selected spell Known.
- [x] Catalog selection copies level, Ritual, School, Source and Stable ID metadata into the owned Spell record.

## DNDAdventure

- [x] Per-character campaign progress.
- [x] Declarative scene text, branching choices, skill checks, flags, achievements, milestones and Item rewards.
- [x] Adventure skill checks use the active character's real skill modifier, including proficiency/expertise and miscellaneous modifiers.
- [x] Guarded Item and milestone rewards are one-shot so revisiting the same rewarded branch does not duplicate them.
- [x] Adventure Item rewards append directly to the active character's Inventory.
- [x] Hold OK full-scene text viewer.
- [x] Hold Left load checkpoint and Hold Right save checkpoint.
- [x] Explicit current-adventure restart with confirmation.
- [x] Journal milestone integration and Journal-to-Adventure continuation.
- [x] Bundled Reef Wardens, Ghost Protocol, Torii Between Tides and Moonlit Market campaigns.
- [x] Installable campaign-pack format with compatibility/content checks before installation.

## DNDJournal

- [x] Per-character timestamped entries sorted newest first.
- [x] Quick, Adventure, Item and Milestone categories.
- [x] Editable title/body and completion state.
- [x] Milestone class selection and one-time milestone-level application.
- [x] Item entries can create Inventory Items.
- [x] Matching Milestone entries can continue the active Adventure.

## DNDInitiative

- [x] Persistent Party Roster plus temporary combat participants.
- [x] Main-character refresh from canonical name, HP, AC and initiative rules.
- [x] Opening Initiative adds or refreshes the active character in the persistent Party Roster without creating duplicate copies.
- [x] Main-character initiative includes DEX, Initiative misc, supported Alert proficiency-bonus and Jack of All Trades half-proficiency behavior, plus Exhaustion penalty.
- [x] Normal, Advantage and Disadvantage initiative modes plus **Roll for All**.
- [x] Full numeric editing for initiative total/modifier, AC and HP.
- [x] Manual participant reordering.
- [x] Hold Up quick AC adjustment during setup.
- [x] Active-combat HP controls, Hold Down Conditions, Hold Left/Right reordering, Hold OK full edit and Hold Up previous turn.
- [x] Round/turn display and Resume without ending combat.
- [x] Initiative ties use initiative modifier as a tie-breaker.
- [x] Explicit **End + Save History**, **End Without History** and Cancel choices.
- [x] Saved encounter history includes end time, rounds, party state and surviving opponents.
- [x] Main-character HP/AC synchronization and Turn/Encounter Feature recharge.
- [x] Single-monster and complete-encounter handoff from Bestiary.

## DNDBestiary

- [x] Streamed bundled monster catalog with Search, Max CR, Type, Source, Environment and Role filters; Source includes **Homebrew** for custom-pack monsters, and Left on filter rows resets to default/Any.
- [x] Monster Search uses case-insensitive substring matching.
- [x] Full stat blocks with abilities, defenses, senses, traits, actions and additional text.
- [x] Favorite and Recent monster lists.
- [x] Opening a monster automatically records it in Recent Monsters.
- [x] Custom monster create/edit/delete.
- [x] Party Level, Party Size, Difficulty, Encounter Environment, Encounter Role, Repeat Types and Balanced/Horde/Elite templates.
- [x] Party Level and Party Size persist between Bestiary sessions.
- [x] Encounter generation with simulation and advisory composition warnings such as leader support, exposed artillery and high minion density.
- [x] **Custom Encounter** starts empty, accepts monsters by Hold OK directly on Monster Catalog rows or the detail action, supports Difficulty Simulator/composition review, saves named encounters and hands the assembled encounter to Initiative.
- [x] Saved encounters with Resume, Send to Initiative, Rename, Duplicate, Archive and Delete actions.
- [x] Saved filter presets.
- [x] Bundled and custom monster-pack loading.
- [x] Pack Diagnostics is shown only when shared **Debug** is On; DNDBestiary loads only the two-byte Debug/Homebrew projection, and Homebrew Off excludes custom-pack monsters from browse/filter, Favorites/Recents, generated/custom encounters and saved-encounter execution while leaving user-created Custom monsters available.
- [x] 346 indexed/statblock-matched bundled monsters.

## Shared behavior

- [x] Active character is stored in `/ext/apps_data/dndolphins/custom_active_profile.txt` as `Active=<id>`.
- [x] Character-owned Inventory, Spellbook and Feature sidecars remain under `/ext/apps_data/dndolphins/` for cross-FAP access.
- [x] Companion main-screen Short Back returns to DNDolphins when installed; Hold Back exits to firmware.
- [x] Returning from a companion FAP refocuses the DNDolphins Home option that launched it.
- [x] Companion main screens display the resolved character ID in brackets where applicable.
- [x] Collections and catalogs use bounded streaming/paging rather than whole-file resident loading.
- [x] Editable campaign/monster text packs do not require checksums.
- [x] Shared Settings and small suite registries use synchronized temporary-file publication with rollback; Adventure campaign-pack installation cleans only uncommitted copied artifacts so an interrupted install can retry from the unchanged inbox.

## Scalable collections and training

- [x] Owned Items/Spells/Features extend beyond the former 24/24/20 limits with eight-record resident windows and logical indexes above 255.
- [x] First-create Inventory/Main, named-bag and Feature sidecars clean up incomplete files after failed initial writes, while preserving any pre-existing non-file collision that prevented creation.
- [x] Spellbook single-owner page allocation preserves Known/Prepared/Ritual/Always Prepared/free-cast associations through add/delete/sort/reload; manual additions are not limited by spells-known allowances.
- [x] Spellbook rows show a right-aligned three-letter class or actual Origin/Feat/grant source tag (`WIZ`, `HIG`, `MAG`, etc.).
- [x] Bounded Combat weapon/spell/ritual index windows, indexed Feature pages, and `Spellbook` / `Features` headers with `PgX<>`.
- [x] **DNDCombat** is a standalone FAP with 26 Combat actions, **Jump to Initiative**, source-aware Initiative return, and streamed attack totals validated beyond the menu count (300 weapons / 300 spells in host regression).
- [x] Scalable Languages and typed Proficiencies lists with add/replace/delete; a lazy four-record owned-list page and eight-row streamed catalog page reduce RAM while totals remain unbounded (300-record host regression); Proficiencies defaults to Allowed with Hold OK for All.
- [x] Primary-class saving-throw proficiencies are staged as reviewed starting grants; no additional starting pair is silently granted from multiclassing.
- [x] Get Elevated Off/420, default 420; full random bundles, deferred until normal equipment exists, immediate active-character grants on Off-to-420. The 18 420 catalog rows are gated only by Homebrew, not Get Elevated.
- [x] Six sidecars included in duplicate/delete/export/import/archive and SHD snapshots/restore, with older history compatibility and no legacy language/training migration.
- [x] SRD grant-coverage gate: 28 species, 4 backgrounds, 12 classes, 12 subclasses and 19 feats all have progression metadata; stable persisted grant IDs are collision-checked.
- [x] Grant choices support Language, Spell, Feat/Perk, Skill, Skill/Tool, weapon/armor/tool Proficiency and Size, including constrained catalogs and dependent feature grants.
- [x] Magic shows Known / knowable / free-granted spell totals; the aggregate is loaded outside Canvas draw callbacks.
- [x] Item catalog displays compact Source tags and validates non-empty source attribution.
- [x] Release compliance cross-checks **475 fixed SRD grant payloads** against SRD spell/feat/language/skill/save/proficiency/size/resistance/sense/speed vocabulary instead of trusting source labels alone.
- [x] Every one of the **72 packed source asset paths** is explicitly classified by the release audit; adding a new bundled file fails the gate until its catalog or UI-artwork provenance is reviewed.
- [x] Compiled Class/Background/Species/Alignment/Feat fallback lists exactly mirror the SRD picker files.
- [x] Spellbook SRD filter UI exposes only SRD-valid class and source selectors.
- [x] Settings reads are buffered and best-effort per line, catalog availability is cached outside draw paths, and Spellbook status filtering uses a fixed 128-byte negative prefilter with exact-match fallback.
- [x] Host release audit checks all direct draw helpers for heap/storage work; the current 3.6.8 audit tree checks **76** direct static draw helpers.


- [x] DNDSpellbook Favorite state + DNDCombat Favorite Spells reuse existing cast path.
- [x] Standalone DNDBackup & Restore FAP owns user-facing SHD bundle export/restore; DNDolphins only launches it.

## Refactor integration validation

- [x] Shared-core Hub, Combat and Grants builds reject redundant declarations with `-Wredundant-decls -Werror`.
- [x] Common input and profile/collection/Settings paths tolerate absent optional runtimes.
- [x] Language/Proficiency first-use and row-kind switching reacquire released cache state and recover from allocation failure.
- [x] Catalog OK/Back preserves return state before releasing the Catalog descriptor.
- [x] Favorite Spells builds its bounded index on entry and returns through the casting workflow.
- [x] Selected Hub Feats are saved before DNDGrants reviews dependent grants.

## 4.20.3 FAL and loading integration

- [x] Character Sheet and Journal UI FALs shared by the Hub and standalone wrappers.
- [x] Journal return reloads the canonical character and invalidates stale collections, with failed-reload save protection.
- [x] Focused Monster Turn FAL shared by Initiative/Bestiary, with first-use assets/custom-pack visibility and parent-state preservation.
- [x] Combat spell damage FAL loads before casting resource consumption and unloads outside spell workflows.
- [x] DND-owned splash/hourglass FAL, enqueue-before-teardown, destination-path readiness and callback-safe cleanup through public firmware APIs. Failure/cancellation/timeout restores drawing; one inactive cache is reclaimed on the next DND readiness/handoff. All changes stay within the app family.
- [x] Fifteen lossless splash choices and two nonresident file-asset bundles; one 1,024-byte bitmap shared across Hub/local/handoff lifetimes; original/text fallback without extra image buffers; no draw I/O or allocations.
- [x] Twenty host regressions, eleven entry lifecycles, five shared-module links and supplied API 88.7 source-contract audit pass.
