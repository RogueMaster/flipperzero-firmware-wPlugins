<h1 align="center"><a href='https://rogue-master.net'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/rmlogo.png" width="40%"></a>
<br><a href='https://discord.gg/gF2bBUzAFe' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Discord.png" alt='Discord' title='Discord'></a>
&nbsp;<a href='https://github.com/RogueMaster/flipperzero-firmware-wPlugins/releases/latest' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Github.png" alt='Firmware GitHub' title='Firmware GitHub'></a>
&nbsp;<a href='https://www.patreon.com/RogueMaster?filters[tag]=Latest%20Release' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Patreon.png" alt='Latest PATREON Release' title='Latest PATREON Release'></a>
&nbsp;<a href='https://github.com/RogueMaster/awesome-flipperzero-withModules' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Resources.png" alt='More Research / Assets' title='More Research / Assets'></a></h1>

# DNDolphins

Offline 5E-compatible character, campaign and encounter tools for Flipper Zero. The suite is split into seven independent FAPs so collection-heavy workflows do not have to share one resident application heap.

## Apps and features

### DNDolphins — character and combat hub

- **Home:** Characters, Character, Vitals, Abilities & Saves, Skills, Features & Perks, Inventory, Magic & Spells, Bestiary, Initiative, Combat, Dice Roller, Adventure and Journal.
- **Profiles:** multiple characters; create, switch/open, rename, duplicate, export, import, archive, delete, verify save and restore backup. Character files use tolerant named-field loading, automatic save/retry and write-only shadow history.
- **Character sheet:** name/player, species, background, alignment, languages/proficiencies, inspiration, multiclass levels/subclasses, total level, proficiency bonus, XP/milestone progression and Hit Dice; bundled class/subclass/species/background/alignment/feat catalogs remain available where those fields support catalog selection.
- **Abilities and skills:** six ability scores/modifiers, saving-throw proficiency/misc/total, all 18 skills with proficiency/expertise/misc/total, and passive Perception/Insight/Investigation.
- **Vitals:** current/max/temp HP, AC, speed/effective speed, initiative/misc, exhaustion, death saves and Hit Dice.
- **Level progression:** standard-array defaults, explicit ASI/Feat level choices, **Grant Initial Traits**, deterministic class/subclass/species progression grants, persistent applied-grant history and no automatic arbitrary spell/feat choice for the player. A bounded post-level **Level-Up Review** summarizes level/proficiency changes, spell-limit/slot changes, deterministic traits applied and any remaining spell or ASI/Feat choices without retaining progression metadata.
- **Magic rules:** Open Spellbook plus casting ability/mode, Spell Attack, Spell Save DC, spell modifiers, current/max level 1–9 slots, multiclass slot rules, half/third casters, Pact Magic, spell points, Mystic Arcanum, Arcane Recovery and rest recovery. Eldritch Knight and Arcane Trickster use third-caster progression and Wizard-list rules where applicable.
- **Features & Perks:** add/edit persistent paged features with uses, recharge cadence (Manual/Turn/Encounter/Dawn/Short-Long/Long), class metadata and resource formulas. Progression/grant feat selection opens in **Allowed** mode and Hold OK toggles Allowed/All; bundled prerequisite checks cover level, Grappler ability scores, Fighting Style/Spellcasting Feature gates, Epic Boons and already-owned non-repeatable feats without restricting unknown custom feats.
- **Combat hub:** Weapon Attacks, Spell Attacks, Rituals, Attack Templates, Initiative, HP/temp HP, Short Rest, Spend Hit Die, Long Rest, Conditions, Concentration, Reaction, temporary effects, resistances, immunities, vulnerabilities, senses, movement, death saves and exhaustion. The title groups rows as Attacks, Encounter, Recovery, Status and Defenses without adding resident menu state.
- **Weapon combat:** STR/DEX/finesse selection, proficiency, magic bonuses, advantage/disadvantage, ammunition, versatile damage, extra damage dice, riders and critical dice doubling.
- **Spell combat:** bounded Spellbook lookup, attack/save mappings, source-class casting modifiers, cantrip scaling, fixed and dice upcasting, multiple attack/roll instances, independent/alternative/later secondary effects, healing/vitality/temporary-HP/mitigation results, resource selection and bounded `XdY` Notes fallback for custom spells. Wizard Ritual Adept exposes known Wizard Ritual spells without preparation or resource consumption and reports the extra ritual casting time.
- **Attack Templates:** editable unarmed/spell-attack/saving-throw/custom templates with ability, save/DC, damage, rider and recharge settings.
- **Dice Roller:** d4/d6/d8/d10/d12/d20/d100, multiple dice, modifier, Normal/Advantage/Disadvantage/Guidance, individual results, sum and total.
- **Navigation:** returning from an internal submenu restores the same DNDolphins home-row selection. Companion returns can focus the matching home row.

### DNDInventory — inventory, equipment and currency

- Opens the exact active character directly to the bounded Inventory list with **+ Add New**. Character fields needed by Inventory are streamed into a narrow projection rather than keeping the full canonical character resident.
- Character-owned sidecar: `/ext/apps_data/dndolphins/inventory_<id>.txt`; opening a missing Inventory is read-only until the first real write/grant.
- Eight-record paging with only the active page resident; the first four Item rows remain visible with **+ Add New**.
- **Add/Edit/Delete Item:** blank record opens the full **36-field** editor immediately; Item Name can use the streamed catalog or a custom name. Item data covers quantity, weight, detail, equipped/attuned state, weapon/armor fields, damage, properties, ammunition, charges, containers and source metadata.
- **Catalog filters:** All, Weapons, Armor, Ammunition, Gear, Tools, Mounts/Vehicles, Potions, Rings, Rods, Scrolls, Staffs, Wands, Wondrous and Magic.
- Bundled generic **Spell Scroll (Cantrip)** and **Spell Scroll (Level 1–9)** entries use level-appropriate rarity.
- **Quick equip:** Hold OK on an Item row toggles Equipped and saves immediately.
- **Inventory Tools:** Currency, Inventory Resources and Grant Initial Inventory. Item editing includes explicit **Stack Qty** numeric entry (0–999); Hold Left/Right on the Inventory list adjusts the selected stack by one. Container display uses the cached container name, self-containment is blocked, and deleting/reordering records transactionally remaps container indexes so surviving items cannot silently point at the wrong container.
- **Currency:** CP/SP/EP/GP/PP is editable and stored only in the Inventory sidecar. Coin normalization is available from Inventory Resources.
- **Derived resources:** carried/equipped weight, carrying capacity, standard/variant encumbrance, attunement count and calculated armor/shield AC with an Apply AC action.
- **Starting inventory:** explicit one-time class/species/background grant; items, prior balance + granted currency and grant marker publish in one synced sidecar transaction. A deliberate one-time regrant preserves existing Items, adds the package/currency once and advances the marker. A d100 trinket is fallback only when normal composition yields neither items nor currency. Merely opening Inventory never seeds equipment.

### DNDSpellbook — spells, preparation and catalog

- Opens the exact active character directly to the bounded Spellbook list with **+ Add New**. Name/class spell-eligibility fields are streamed into a narrow projection rather than keeping the full canonical character resident.
- Character-owned sidecar: `/ext/apps_data/dndolphins/spellbook_<id>.txt`; a missing Spellbook remains empty until the first saved spell.
- Eight-record paging with only the active page resident; the first four spells remain visible with **+ Add New**.
- **Add/Edit/Delete Spell:** blank record opens the full **17-field** editor immediately; Name can use the streamed catalog or a custom name. Stored state includes source class, level, Known, Prepared, Always Prepared, Ritual, free casts and catalog/source/school/grant metadata.
- **Quick prepare:** Hold OK toggles Prepared and saves immediately; Always Prepared remains fixed. List marks show `A`, `P`, `K` or `-`, plus `F` when a free cast remains.
- Bundled 448-spell catalog with class, level, school, ritual, source, status and eligibility filtering.
- **Class filtering:** All Classes is the eligible union across the character's classes; individual class filters remain available. Eldritch Knight and Arcane Trickster use the Wizard spell list. Eligibility defaults to Allowed; All Spells is an explicit opt-in override.
- **Sorting:** saved spells remain level-ascending then case-insensitive alphabetical by name. Sorting uses bounded compact keys and rewrites only when order changed.
- Catalog and sidecar access are streamed/bounded; no collection or storage work occurs from canvas drawing.

### DNDAdventure — campaigns and campaign packs

- Requires the exact active canonical character and stores per-character campaign progress under `/ext/apps_data/dndadventure/`. Adventure streams only name, class levels, ability scores and skill proficiency/misc fields into its resident profile projection.
- Campaign scenes support narrative text, branching choices, skill checks, flags, achievements, checkpoints/milestones and Item rewards.
- Bundled campaigns include **Reef Wardens** and **Ghost Protocol**.
- Adventure milestones integrate with Journal; active campaign/scene state can be continued from the matching Journal milestone.
- Bundled declarative campaigns include **Reef Wardens**, **Ghost Protocol**, the Japan travel-fantasy **Torii Between Tides**, and the compact supernatural **Moonlit Market**; campaign branching remains data-driven with bounded scene/check/reward/milestone state.
- Item rewards append to the character Inventory but never initialize starting inventory or starting currency.
- **Campaign packs:** inbox preview/validation, manifest/index/content/entry-scene/compatibility/ID-conflict checks and explicit install.
- Installed packs stay registered and on storage. Hold OK toggles an existing pack active/inactive; short OK on an existing pack row is intentionally inert.
- Campaign reading uses bounded streaming/sparse hints rather than retaining whole campaign packs.

### DNDJournal — per-character notes and milestones

- Requires the exact active canonical character and stores Journal data under `/ext/apps_data/dndjournal/`.
- Timestamped per-character entries sorted newest first with bounded metadata paging and body loading only when opened.
- Entry categories include Quick, Adventure, Item and Milestone.
- Add/edit title and body; persistent completion state is supported.
- Adventure writes milestone/history entries without duplicating Adventure progress into the Journal.
- Milestone entries can select a class and apply the milestone level once.
- Item entries can create a character Inventory Item from the Journal entry.
- A matching active Adventure milestone can continue the current Adventure.

### DNDInitiative — roster and active combat

- Uses the persisted active character ID; ID `0` is chosen only when active-profile metadata is absent/unreadable. A present but stale `Active=<id>` is not silently replaced by another character.
- Per-character Initiative data is stored under `/ext/apps_data/dndinitiative/`.
- **Setup roster:** main character plus temporary/manual participants; editable name, initiative modifier/total, roll mode, AC, current/max HP and conditions.
- Main-character name, HP, AC and initiative modifier refresh from the canonical character. Initiative includes DEX, misc/exhaustion and supported feature effects such as Alert/Jack of All Trades.
- Normal/Advantage/Disadvantage initiative modes, individual rolls and **Roll for All**; typed totals remain user-entered and tie handling uses modifier order.
- Setup controls include initiative adjustment, participant reordering, Hold Up quick AC increase and Hold OK full participant editing.
- **Combat:** current participant plus compact round/turn position (`R# T#/#`), short Left/Right HP adjustment, Hold Down conditions, Hold Left/Right participant reordering and Hold OK full edit. Five-row roster/setup/combat/editor windows keep the selected/current participant visible as larger encounters are navigated.
- **Short Back** returns from active Combat to the Initiative main menu without ending the encounter; Resume continues it.
- **Completed encounter history is opt-in:** End Current Combat offers Save History, End Without History or Cancel. A saved encounter is a new timestamped Initiative-owned record containing end time, rounds, complete party HP/AC/conditions and surviving opponents; a failed history write leaves the combat active.
- **Hold Up** moves back one turn, including from the first participant of a later round to the previous round's last participant.
- **End Current Combat** opens the explicit Save History / End Without History / Cancel choice; only either End action clears active-combat state.
- Main-character HP/AC changes synchronize back to the canonical character, and Turn/Encounter feature recharge is applied at the corresponding cadence.
- Accepts Bestiary monster/encounter handoff data.

### DNDBestiary — monsters, packs and encounters

- Usable without a character profile. It uses the persisted character ID for display/Initiative transfer and defaults that ID to `0` only when active-profile metadata is absent/unreadable.
- Streamed monster catalog with bounded windows and filters for name, maximum CR, creature type, source, environment and role.
- Full monster detail includes CR/XP, AC/HP, type/source/role, size/alignment, speed, abilities, skills, defenses, senses, languages, traits, actions and extra text.
- Favorites and recent-monster state.
- Create, edit and delete custom monsters. Bundled Dolphin/Capybara records do not create custom files merely by browsing.
- **Encounter generator:** party level/size, Low/Moderate/High difficulty, environment, preferred role, repeats toggle and Balanced/Horde/Elite templates.
- Encounter output includes XP/difficulty simulation and composition warnings such as leader/support balance, exposed artillery and minion density.
- Saved encounters can be resumed, renamed, duplicated, archived, deleted and sent to Initiative.
- The bundled Bestiary contains **346** indexed/statblock-matched monsters, including original Dungeons & Dolphins creatures such as Tanuki Trickster, Kappa River Scout, Tsukumogami Umbrella, Kitsune Wayfinder, Karasu Tengu Warden and Lantern Onryo.
- Saved filter presets.
- **Monster packs:** packaged/custom/inbox handling, validation, install, active/inactive toggle, diagnostics/recovery and file-preserving deactivation.

## Shared behavior

- `dnd_profile_handoff.*` is linked by all seven FAPs and is the single shared contract for persisted active-character resolution, canonical profile lookup, FAP paths, launch arguments and parent-return handoff.
- `dnd_profile_projection.*` is linked only by Inventory, Spellbook and Adventure. It streams app-needed character fields by name from the unchanged canonical profile; Inventory may transactionally write back only AC/encumbrance/carry-capacity fields, while Spellbook and Adventure do not own canonical character fields.
- `/ext/apps_data/dndolphins/custom_active_profile.txt` stores `Active=<id>`. Companion launch arguments do not override that selection and companions do not discover/fall forward to another character.
- Inventory and Spellbook sidecars remain in the DNDolphins character root so Combat, progression, Adventure and Journal see one source of truth.
- Adventure, Bestiary, Journal, Initiative, Inventory and Spellbook show the resolved `[id]` at the top-right of their main screen only; the internal invalid-ID sentinel is never rendered.
- Cross-FAP launch paths are explicit `/ext/apps/Games/*.fap`. The outgoing app tears down views/callbacks/services/caches/owned heap before Loader starts the destination.
- On companion main screens, Short Back returns to DNDolphins when installed and restores the matching parent row; Hold Back exits to firmware. Subscreen Back remains local except for Initiative Combat's explicit behavior above.
- Collection readers use bounded pages/windows. Project canvas callbacks do not allocate project heap, perform storage I/O or rewrite collections.
- No firmware `qsort` dependency is used.

## Storage model

- Character profiles and character-owned sidecars: `/ext/apps_data/dndolphins/`
- Inventory: `inventory_<id>.txt`
- Spellbook: `spellbook_<id>.txt`
- Features: `feats_<id>.txt`
- Applied deterministic grants: `appliedgrants_<id>.txt`
- Journal: `/ext/apps_data/dndjournal/`
- Initiative: `/ext/apps_data/dndinitiative/`
- Adventure: `/ext/apps_data/dndadventure/`
- Bestiary/custom monsters/packs/encounters: `/ext/apps_data/dndbestiary/`

Character/profile parsing is best-effort by field name. Collection and progression sidecars are created only by a real write; opening a missing collection does not create it. Transactional rewrites publish only after successful sync, and campaign/monster content is preserved when marked inactive.

## Memory model

Each FAP is loaded independently; opening Inventory or Spellbook does not keep DNDolphins resident. Large collections/catalogs are streamed or bounded rather than retained whole. Exact manifest stack reservations, ARM32 project app-state sizes and source-derived working-set estimates are maintained in `MEMORY_AUDIT.md`; firmware/framework objects and allocator overhead are intentionally separate from those project-owned figures.

## Documentation

- `FEATURE_CHECKLIST.md` — implementation/behavior checklist
- `MEMORY_AUDIT.md` — stack reservations, project heap/state sizes and transient working sets
- `RULES_AUDIT.md` — 5E/rule behavior audit
- `SAVE_SCHEMA.md` — character and sidecar persistence contract
- `SOURCE_OWNERSHIP.md` — shared versus app-owned code/header policy
- `COMPATIBILITY.md` — migration and cross-FAP compatibility
- `DEVICE_TEST_MATRIX.md` — hardware regression/stress checklist
- `CAMPAIGN_PACK_SCHEMA.md` / `MONSTER_PACK_SCHEMA.md` — pack formats
- `CATALOG_POLICY.md` — bundled catalog policy
- `ACCESSIBILITY.md` — controls/readability conventions
- `ATTRIBUTION.md` — source/content attribution
- `CHANGELOG.md` — released history
- `ROADMAP.md` — future work

## Build

From a RogueMaster/Flipper firmware tree containing this directory:

```text
fbt fap_dndolphins fap_dndadventure fap_dndjournal fap_dndinitiative fap_dndbestiary fap_dndinventory fap_dndspellbook
```

Device builds and hardware stack/free-heap/fragmentation testing remain the final authority for firmware/framework memory behavior.
