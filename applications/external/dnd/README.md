<h1 align="center"><a href='https://rogue-master.net'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/rmlogo.png" width="40%"></a>
<br><a href='https://discord.gg/gF2bBUzAFe' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Discord.png" alt='Discord' title='Discord'></a>
&nbsp;<a href='https://github.com/RogueMaster/flipperzero-firmware-wPlugins/releases/latest' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Github.png"  alt='Firmware GitHub' title='Firmware GitHub'></a>
&nbsp;<a href='https://www.patreon.com/RogueMaster?filters[tag]=Latest%20Release' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Patreon.png"  alt='Latest PATREON Release' title='Latest PATREON Release'></a>
&nbsp;<a href='https://github.com/RogueMaster/awesome-flipperzero-withModules' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Resources.png"  alt='More Research / Assets' title='More Research / Assets'></a></h1>

# Dungeons & Dolphins

Dungeons & Dolphins 3.0.2 is an offline, 5E-compatible Flipper Zero toolkit for RogueMaster. One source tree and one `application.fam` build two independent FAPs with explicit source lists:

- **Dungeons & Dolphins** — character profiles, rules tracking, spells, equipment, notes, dice, combat, initiative, and campaigns.
- **Dolphin Bestiary** — the monster stat-block browser, custom monsters, diagnostics, and party-level encounter generation.

Both applications use only the screen, buttons, and SD card. No network or external hardware is required.

## Full feature list

### Character profiles and saves

- Dynamically indexed profiles with no fixed profile-count limit.
- One readable, versioned, checksummed text save per character in the FAP's persistent app-data `profiles/` directory.
- Save names use `ch_{id}_{characterName}_{characterLevel}.txt`.
- Main and newly created heroes remain visible in Characters even when the first write fails.
- Transactional new-character creation restores the previous active character after a failed write.
- Immediate autosave after character, stat, spell, feature, item, inventory, currency, journal, party, initiative, or resource changes.
- Atomic temporary-file publication, retained backup generation, checksum verification, and manual backup restore.
- First-run migration relocates legacy asset-path profiles into app data without replacing an existing profile ID or file. Each legacy source is deleted only after its app-data destination is safely present.
- Failed, corrupt, or unsupported profile loads are preserved and cannot trigger a blank-profile overwrite.
- Schema 2 is the compatibility baseline; future readers retain explicit older-schema branches and migrate only after a validated load.
- Rename, switch, duplicate, export, import, archive, delete, verify, and restore profile actions.
- Save failures show a persistent UNSAVED warning; Retry Save / Status retries SD access and otherwise confirms that autosave is current.

### Character sheet

- Name, player, species, background, a picker containing all nine alignments, experience, milestone leveling, and inspiration.
- Up to four classes with independent class level, subclass, Hit Point Die, Hit Dice pool, and feature ownership.
- Total level and Proficiency Bonus calculated across all classes.
- All six ability scores and saving throws with proficiency and miscellaneous modifiers.
- All 18 skills grouped by their governing STR, DEX, INT, WIS, or CHA ability, including proficiency, expertise, and miscellaneous modifiers.
- HP, temporary HP, Armor Class, speed, initiative, exhaustion, death saves, and class-specific Hit Dice spending.
- Pass. Perception, Pass. Insight, and Pass. Investigation remain fully visible and editable without abbreviating the governing attribute.
- Languages, Origin Feat, tools, armor training, weapon training, size, senses, and general proficiencies.
- The 103-name species/lineage catalog includes ancestry-specific Dragonborn, Dwarf, Elf, Genasi, Gnome, Goliath, Halfling, and Tiefling choices plus compatible standalone species and lineages.

### Classes, subclasses, features, and grants

- All 13 bundled classes have class-associated subclass choices: 139 selectable core, add-on, legacy-compatible, and alias records. Class filtering is the default and an All view remains available.
- Class-linked features retain the granting class and level gained.
- Feature uses support manual, Proficiency Bonus, or ability-modifier maximums.
- Recharge choices include turn, encounter, dawn, Short Rest, Long Rest, or manual recovery.
- Structured grants retain stable ID, source, option type, prerequisites, class association, level gained, payload, and apply/skip state.
- Species, background, feat, subclass, class-feature, item, and custom grants can be reviewed before application.
- Short OK opens an SD-backed picker; hold OK retains custom text entry where supported.

### Catalogs and low-memory browsing

- Packaged catalogs cover classes, subclasses, species, backgrounds, feats/features, spells, and items.
- Each catalog uses one streamed file. Additional names and annotated rows are appended to the matching normal catalog instead of loading a second overlay.
- On-device custom spell, species, class, subclass, background, feat, and item text is stored only in that character profile; assigning it never modifies a packaged catalog.
- The spell picker streams from SD and retains at most ten matching records in RAM; item and other catalog pickers retain at most 50.
- Left/Right changes the current bounded catalog page; buffers are released immediately when the picker closes.
- Catalog source files, long descriptions, and campaigns are not retained in steady-state RAM.
- Annotated spell rows can include level, class associations, school, ritual state, and source category.
- Navigation labels are compiled directly into the application; runtime translation tables are intentionally omitted to reduce startup work and heap use.

### Spellcasting

- Known, Prepared, Always Prepared, Ritual, and free-casts-per-rest state per spell.
- Spell grant source and grant name for species, background, feat, subclass, item, and custom grants.
- Per-class spellcasting ability, casting mode, cantrip limit, prepared limit, spellbook size, Pact slots, Mystic Arcanum mask, and spell points.
- Shared multiclass slots remain separate from each class's known and prepared spells.
- Spell attack modifier, Spell Save DC, editable miscellaneous modifiers, and slot spending.
- Long Rest recovery and a dedicated Arcane Recovery helper.
- Filters for class, level/cantrip, ritual, school, source category, and prepared status.
- Spell choices are presented by level and then alphabetically; only the current ten-record page is retained in RAM.
- Magic includes an explicit Back to Main Menu row in addition to the Back button.

### Inventory and currency

- Item name, notes, quantity, weight, container, equipped/attuned state, charges, and ammunition group.
- Weapon properties, attack ability, proficiency, magic bonus, damage dice/type, versatile damage, and riders.
- Armor base, Dexterity cap, shield bonus, and calculated Armor Class.
- Carried/equipped weight, Strength-based capacity, optional override, containers, and optional encumbrance.
- Attunement warning above three items.
- Copper, Silver, Electrum, Gold, and Platinum tracking with standard normalization.

### Combat, attacks, and dice

- Combat contains weapon attacks, attack templates, initiative, HP, temporary HP, rests, class Hit Dice, death saves, and exhaustion.
- Attack templates were moved into Combat; the redundant Combat Sheet screen was removed.
- Template fields cover weapon/unarmed/spell/save/custom actions, ability, attack modifier, save DC, damage, Mastery, riders, and recharge.
- Conditions, concentration, reaction state, temporary effects, resistances, immunities, vulnerabilities, senses, and movement remain character fields.
- Weapon attack and damage rolls use current character and item data.
- Critical damage, Advantage/Disadvantage, finesse/ranged selection, magic bonuses, versatile damage, ammunition, and extra riders.
- Generic animated rolls support d4, d6, d8, d10, d12, d20, d100, modifiers, Advantage, Disadvantage, and Guidance mode. Guidance adds a visible d4 result to a d20 roll.
- Roll Now remains unchanged while configuring a modifier and changes only after a roll is made.
- Multi-die results show every individual die, the dice sum, modifier, and final total.

### Initiative, journal, and campaigns

- Per-character party presets with name, initiative modifier, Armor Class, current HP, and maximum HP.
- Current-character insertion, temporary participants, manual/automatic rolls, sorting, tie reordering, rounds, and current turn.
- Per-participant name, roll, modifier, HP, Armor Class, and conditions, plus history/undo for turn, HP, and resource mistakes.
- Hold OK on an active combat participant to edit every tracked field or remove that participant with two-step confirmation; current HP accepts negative values for damage-before-total tracking.
- Quick, adventure, item, and milestone notes with completion and class-level advancement.
- Inventory items can be created from journal entries.
- Disk-backed campaign manifests, per-profile/per-campaign progress, checkpoints, quest flags, achievements, branches, skill checks, and rewards.
- Campaign diagnostics detect incompatible manifests, missing scenes, duplicate IDs, and broken links.

### Dolphin Bestiary

- Separate FAP and asset namespace, so the character tracker never loads monster tables or encounter state.
- 340 unique packaged monster records.
- Monster browser filters combine name, maximum challenge, creature type, source category, environment, and encounter role.
- Browser pages hold at most 20 summaries; full stat blocks are allocated only while one is open and released on exit.
- All packaged stat blocks share one streamed section file, reducing first-launch asset deployment and avoiding hundreds of simultaneous file entries.
- Stat blocks show challenge, XP, Armor Class, HP, type, source, role, size, movement, abilities, skills, defenses, senses, languages, traits, actions, and extra actions.
- Short OK on a browser result opens its complete, scrollable stat block.
- Short OK on a generated-encounter monster opens that monster's complete stat block; short OK on any stat row then opens the attribute in a full-screen wrapped reader.
- Low, Moderate, and High encounter budgets use the selected party level and size; generation spends as much of the selected budget as possible without exceeding it and keeps Moderate above Low and High above Moderate.
- Aquatic, Dungeon, Planar, Urban, Wilderness, or unrestricted encounter environments.
- Balanced, Horde, and Elite templates, repeated-creature control, and optional Leader, Controller, Skirmisher, Artillery, Brute, or Minion weighting.
- Custom monster creation and editing preserve stable IDs.
- A visible Delete Custom Monster row uses two-step confirmation; packaged records are read-only.
- Custom records use atomic `custom_index.txt` and `custom_statblocks.txt` files, while packaged `index.txt` and `statblocks.txt` remain untouched.
- Custom records are stored under persistent app data. A legacy asset-path custom pair is relocated on first launch without replacing an existing app-data pair, and its legacy files are deleted only after the persistent pair is safely present.
- Browser, filter, diagnostic, and encounter scans stream the packaged and custom layers together without retaining either complete table in RAM.
- Transaction recovery, backup, rollback, and orphan cleanup apply only to the custom layer.
- Pack diagnostics report valid/invalid records, pack versions, recovery results, and free heap using one buffered stat-block pass.
- Encounter generation performs one streaming eligibility pass, retains at most sixteen randomized candidates, limits total creatures to two per party member, and releases that pool immediately after generation.
- Each FAP has a final menu entry that exits cleanly and queues the other installed FAP.

### Interface and performance changes in 2.7

- Short Back from Classes, Spells, Features/Perks, and Inventory returns to the screen that opened that list instead of reopening the last record.
- Long Back returns directly to Main from ordinary application screens; long Back on Main exits.
- Selected long rows scroll horizontally instead of ending in an ellipsis.
- Currency values open a numeric editor with short OK; hold OK opens direct number entry for Vitals and other editable numerical settings.
- Both applications defer catalog counts and large-file scans until the relevant browser opens.
- Profile autosaves reuse the known profile path and avoid rescanning the profile directory after every change.
- Removed separate custom catalog overlay scans and consolidated monster stat blocks into the normal streamed pack files.
- Reworked encounter generation to avoid repeated full-index scans and interface stalls.

### Performance and inventory hotfixes in 2.7.1

- Replaced byte-at-a-time catalog and grant-file reads with buffered SD reads.
- Catalog pages stop scanning after the next page is proven to exist instead of counting every remaining record.
- A catalog uses one bounded heap block rather than nine independent allocations, reducing heap fragmentation.
- Catalog memory is released before grant lookup and autosave, lowering peak memory during item, class, species, background, and feat selection.
- Text and number input modules are allocated only when opened; both FAPs avoid their startup allocation.
- Unchanged-state save calls are skipped after a lightweight fingerprint check.
- Standard armor and weapon choices populate weight, damage, damage type, supported properties, ammunition group, versatile die, Armor Class base, Dexterity cap, and shield bonus.
- New characters default to True Neutral.
- Runtime translation loading and pre-release save migration code were removed.
- Bestiary reads now use 512-byte buffering, cached browse counts, early page completion, and one-pass diagnostics instead of repeated per-record scans.
- Character saves use schema 2; older pre-release schemas are intentionally rejected without migration.

### Bestiary reader hotfix in 2.7.2

- Short OK on any monster stat row opens that field in a dedicated full-screen reader.
- Long fields use word-aware wrapping with a five-line viewport; Up/Down scrolls one line and Left/Right pages through the text.
- Short OK or Back closes the reader and restores the exact stat row and list position.
- Custom-monster editing remains available through its own explicit row instead of intercepting stat-row selection.
- Character startup memory was reduced by moving spells, features, items, journal entries, and structured grants to grow-on-demand storage.
- An empty default character now reserves one item and one journal entry instead of every maximum-size record array, reducing the core save object from about 34.6 KB to about 6.3 KB.
- Direct launch and first-run character creation use the same bounded memory model as app-to-app launch while retaining the existing record limits and text-save format.
- Persistent text is copied into UI rows with explicit bounds, satisfying strict format-truncation checks while retaining complete detail fields for horizontal scrolling.
- The Bestiary reader's line-range display is sized for the complete range of its 16-bit counters under strict compiler checks.

### Catalog and Bestiary isolation in 3.0

- Generated encounters use the same two-step monster/stat-row reader and restore the original encounter selection when closed.
- Spell catalog pages retain ten records rather than 50 and are ordered by level and name, reducing the picker's bounded heap allocation by about 80 percent.
- The Bestiary releases its lazy text editor before allocating monster browsers, details, diagnostics, or encounters.
- Custom-monster writes are isolated from the packaged Bestiary, and custom deletion is exposed as a confirmed row on custom stat blocks.
- Custom character option text remains profile-local and never rewrites packaged character catalogs.

### Persistence and Bestiary repair in 3.0.1

- Moved writable character profiles, campaign progress, and custom monster packs to persistent app data so FAP asset deployment cannot erase them.
- Added non-overwriting legacy migration from the former asset locations while retaining the original files as recovery copies.
- Prevented failed or unsupported character loads from being replaced with a blank character during startup, switching, restore, import, autosave, or exit.
- Streamed packaged and custom monsters through the same complete result count with 20-record pages.
- Added custom-stat fallback for records created by older combined packs so their stat block and custom-only edit/delete controls remain reachable.

### Asset/data separation repair in 3.0.2

- Kept packaged catalogs, bundled campaigns, and packaged Bestiary tables exclusively in their read-only `APP_ASSETS_PATH` namespaces.
- Relocated only mutable legacy data: profiles, profile-owned campaign progress, complete custom campaign packs, and custom monster packs.
- Custom campaigns now stream from `APP_DATA_PATH` alongside bundled campaigns from `APP_ASSETS_PATH`.
- Custom monsters continue to stream as a second layer after every packaged monster, so adding a custom record never replaces or hides the bundled Bestiary.
- Removed each legacy mutable source only after its persistent destination was safely published; existing app-data records remain authoritative and are never overwritten.

## Controls

- Up/Down: move through rows.
- Left/Right: adjust a value or change the current bounded catalog page.
- Short OK: open, toggle, apply, save, or roll.
- Long OK: direct numerical entry on numeric rows, custom text on text rows, alternate spell/subclass view, or another screen-specific action.
- Short Back: return to the correct parent screen.
- Long Back: return to Main; long Back on Main exits.

## SD-card locations

Packaged reference files remain in the read-only FAP asset namespaces:

- Character assets: `/ext/apps_assets/dungeons_and_dolphins/`
- Bestiary assets: `/ext/apps_assets/dolphin_bestiary/`

Writable user state uses the firmware-managed persistent app-data namespaces:

- Character profiles, exports, archives, custom campaigns, and campaign progress: `/ext/apps_data/dungeons_and_dolphins/`
- Custom monster index and stat blocks: `/ext/apps_data/dolphin_bestiary/`

Catalog additions made manually on the SD card are appended to the matching normal asset file, such as `catalogs/spells.txt`, `catalogs/classes.txt`, or `catalogs/items.txt`. On-device custom character text remains inside its character save. Packaged monster summaries and stat blocks use asset files `monsters/index.txt` and `monsters/statblocks.txt`; on-device custom monsters use app-data files `monsters/custom_index.txt` and `monsters/custom_statblocks.txt`. Character saves retain the requested `ch_{id}_{name}_{level}.txt` format beneath the app-data `profiles/` directory.

The FAP assets contain the ready-to-use catalog and bestiary files; no duplicate SD-card tree is required in the source release.

## Build

Place this directory in RogueMaster's `applications_user` tree, then build both targets from the same manifest:

```sh
./fbt fap_dungeons_and_dolphins fap_dolphin_bestiary
```

The manifest uses explicit `sources=[...]` lists so each FAP excludes the other application's entry point and feature modules. Release ZIPs contain source, assets, and user documentation only. They exclude validation tests, development tools, `dist`, compiled FAPs, ELF files, and object files.

## Hardware status

The included host-side rules, parser, catalog, checksum, schema, filter, and packaging checks pass. This source-only archive does not claim a fresh RogueMaster compilation or physical-device result; those remain pending recipient testing.
