<h1 align="center"><a href='https://rogue-master.net'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/rmlogo.png" width="40%"></a>
<br><a href='https://discord.gg/gF2bBUzAFe' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Discord.png" alt='Discord' title='Discord'></a>
&nbsp;<a href='https://github.com/RogueMaster/flipperzero-firmware-wPlugins/releases/latest' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Github.png"  alt='Firmware GitHub' title='Firmware GitHub'></a>
&nbsp;<a href='https://www.patreon.com/RogueMaster?filters[tag]=Latest%20Release' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Patreon.png"  alt='Latest PATREON Release' title='Latest PATREON Release'></a>
&nbsp;<a href='https://github.com/RogueMaster/awesome-flipperzero-withModules' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Resources.png"  alt='More Research / Assets' title='More Research / Assets'></a></h1>

# DNDolphins

Offline 5E-compatible tools for Flipper Zero, split into five FAPs so each workflow owns its data and runtime memory.

## Apps

| App | Purpose | Writable root |
|---|---|---|
| DNDolphins | Characters, spells, inventory, dice and character combat | `/ext/apps_data/dndolphins/` |
| DNDAdventure | Campaigns, checks, rewards, milestones and campaign packs | `/ext/apps_data/dndadventure/` |
| DNDJournal | Per-character notes and Adventure milestone history | `/ext/apps_data/dndjournal/` |
| DNDInitiative | Party roster, initiative, rounds, HP/AC and conditions | `/ext/apps_data/dndinitiative/` |
| DNDBestiary | Monsters, custom monsters, packs and encounters | `/ext/apps_data/dndbestiary/` |

Cross-app launches use explicit `/ext/apps/Games/*.fap` paths. The outgoing app tears down its views, callbacks, services, caches and heap state before Loader starts the next FAP. Journal and Adventure use the active character when launched directly. Initiative validates the primary character file and, if none exists, offers to launch DNDolphins instead of creating characterless Initiative state. Bestiary remains usable without a character.

## Features

- **Level Choices:** explicit ASI/Feat choices are detected from class level, survive restarts through existing grant records, and never silently choose a feat for the player.
- **Adventure pack controls:** installed campaign packs can be enabled/disabled; installed campaign packs remain registered in the campaign pack list; Hold OK toggles a selected pack active/inactive, short OK does nothing on existing pack rows, and campaign files remain on storage.

### Characters

- Multiple independent character profiles stored as readable, manually editable text files.
- Best-effort named-field loading: unknown, reordered, missing or individually malformed fields do not invalidate otherwise usable character data.
- Automatic saves plus import, export, duplicate, rename, archive, delete, retry and write-only shadow history.
- Multiclass characters with class levels, subclasses, Hit Dice and class-linked feature handling.
- Ability scores, saving throws, skills, proficiency/expertise, passive scores and miscellaneous modifiers. New characters use the standard array; level-1 class/species/background traits are applied only when **Grant Initial Traits** is selected.
- HP, temporary HP, AC, speed, initiative, exhaustion, death saves, inspiration, XP and milestones.
- Species, background, alignment, languages, feats, tools, armor/weapon training, size, senses and proficiencies.

### Magic & spells

- Known, Prepared, Always Prepared, Ritual and free-cast spell states.
- Per-class casting ability/mode, prepared limits, spellbook size, Pact Magic, Mystic Arcanum and spell points.
- Shared multiclass slots, automatic empty-slot initialization, Spell Attack, Spell Save DC, slot spending and rest recovery.
- Spell filtering by class, level, ritual, school, source and prepared state. Hold OK on a Spellbook row toggles Prepared and saves immediately; always-prepared spells remain fixed.
- Streamed spell catalog plus custom spell support; short or hold OK on **+ Add New** creates a blank spell and opens the full editor immediately. Catalog selection remains available from the Name field inside that editor.
- Combat Spell Attacks use structured spell mappings for supported attacks, saves, scaling, healing and multi-part effects; unmapped custom spells can use the first valid `XdY`/`XDY` expression in Notes as a bounded fallback.
- Character-owned spellbooks use eight-record sidecar paging instead of keeping the whole collection resident.

### Inventory, equipment & items

- Inventory quantities, weight, containers, equipped/attuned state, charges and ammunition groups. Hold OK on an Inventory row toggles Equipped and saves immediately.
- Weapon attack modifiers, damage dice, versatile damage, riders, proficiency, magic bonuses and weapon properties.
- Armor/shield values, calculated AC, carrying capacity and currency normalization.
- Copper, Silver, Electrum, Gold and Platinum tracking.
- Starting inventory is created only when Inventory is first opened and no live inventory sidecar exists. Class/species/background defaults are streamed from assets and one hidden d100 trinket is added.
- Resources, Weapon Attacks and Adventure do not silently seed starting equipment.
- Character-owned inventory uses eight-record sidecar paging and streamed whole-collection calculations; short or hold OK on **+ Add New** creates a blank item and opens the full editor immediately. Catalog selection remains available from the Name field inside that editor.

### Dice & combat

- Animated d4, d6, d8, d10, d12, d20 and d100 rolls.
- Advantage, Disadvantage, modifiers, multiple dice and Guidance mode.
- Individual results, dice sum, modifier and final total display.
- Weapon Attacks and Spell Attacks use the character's owned records and current resources.
- HP, temporary HP, rests, Hit Dice, conditions, concentration, reactions, exhaustion, resistances, immunities, vulnerabilities, senses and movement support.

### Initiative

- Standalone per-character Party Roster with name, initiative modifier, AC, current HP and maximum HP.
- Initiative setup provides Roll for All, individual automatic rolls, temporary members, short/repeat left/right initiative adjustment, long left/right manual participant reordering, and hold OK full participant editing.
- Initiative automatically refreshes the active character's current name, HP, AC and initiative modifier from the canonical primary character profile when opened; Inventory/Spellbook sidecars cannot be mistaken for the profile, character ID 0 remains valid, and other roster members keep their independently assigned modifiers.
- Initiative Roll mode can be set to Normal, Advantage or Disadvantage. Roll for All and individual automatic rolls use the selected mode; the full editor accepts direct numeric initiative total, modifier, AC, current HP and maximum HP values.
- Round/current-turn tracking; short Back moves to the previous turn and hold Back returns to the Initiative screen. The menu can explicitly end the current combat.
- During active combat, AC is shown beside HP; left/right changes HP, hold Up raises AC, hold Down opens quick condition editing, hold left/right manually reorders participants, and hold OK opens the full participant editor.
- HP/AC edits for the tracked main character synchronize back to the canonical character profile. Turn- and Encounter-recharge features are restored automatically at their corresponding Initiative cadence.
- Participant editing, temporary participants, negative HP, conditions and removal.
- Bestiary Add to Initiative can append selected monsters or encounter members to the active character roster before opening Initiative.

### Journal

- Standalone per-character timestamped text entries stored outside the character save.
- Newest-first storage-backed listing with bounded metadata caching and body-on-open loading.
- Adventure milestone history is written directly to Journal. Milestone entries can select a character class and apply that milestone level exactly once.
- Item-category entries can create an inventory item from the Journal title/body without embedding Journal data in the character save.
- Journal can return to DNDolphins or launch Adventure to continue after a milestone; Journal does not create Adventure progress.

### Adventure

- Declarative campaign manifests/scenes with branching choices, skill checks, flags, achievements, item rewards, milestones and checkpoints.
- Per-character campaign progress owned by DNDAdventure rather than embedded in the character file.
- Bundled **Reef Wardens** aquatic adventure.
- Bundled **Ghost Protocol**, a fictional authorized security-audit adventure using a Flipper Zero as an in-world inspection tool; it contains game checks and branching outcomes rather than real exploit instructions.
- Adventure item rewards use shared item storage behavior but never initialize starting equipment.
- Installable campaign-pack support with bounded storage-backed loading.

### Bestiary

- Large bundled monster catalog with streamed browsing, filtering, detail views and encounter generation.
- Search/filter support for monster metadata such as name, challenge, type, source, environment and encounter role where present.
- Favorites, recents, saved filters, diagnostics and named saved encounters.
- Saved encounter resume, rename, duplicate, archive, delete and Add to Initiative workflows.
- Custom monster create/edit/delete without rewriting packaged monster assets.
- Installable monster packs remain separate from packaged and direct-custom records.
- **Monster pack controls:** existing installed packs remain registered and preserve their files; Hold OK toggles Active/Inactive, while short OK does nothing on existing pack rows. Short OK remains the install action on the separate inbox row.
- If no custom monster files exist, Bestiary seeds a small bundled custom pack containing **Dolphin** and **Capybara**. Existing or partial user custom files always win and are never overwritten by the seed.
- Full stat-block rows can be opened in the scrolling full-screen reader.

### Storage, memory & resilience

- Character core saves, spellbooks, inventories, Journal entries, Adventure progress and Bestiary state are owned by the FAP that uses them.
- Character shadows are write-only history and are never used as live recovery input.
- Packaged catalogs/campaigns/monsters remain in app assets; mutable user data remains in app data.
- Large lists are streamed, paged or bounded rather than loaded wholesale.
- Cross-FAP handoffs free outgoing runtime state before launching the next app.
- Text data remains manually editable; no checksum match is required for ordinary character/custom-pack loading.

## Controls

- **OK** — select, open, confirm or roll.
- **Hold OK** — alternate action such as custom text, numeric entry or participant editing where supported.
- **Back** — previous screen; during active combat, previous turn.
- **Hold Back** — return toward the parent/main workflow; during active combat, return to Initiative.
- **Left / Right** — change pages or values where supported.
- **Up / Down** — move through rows and lists.

## Memory model

- DNDolphins: 6 KB stack
- DNDAdventure: 4 KB
- DNDJournal: 4 KB
- DNDInitiative: 4 KB
- DNDBestiary: 6 KB

See `MEMORY_AUDIT.md` for current source-derived stack-use estimates and remaining device stress targets.

## Documentation

- `CHANGELOG.md` — released history from the retained release line onward, summarized per revision.
- `ROADMAP.md` — future work only.
- `SAVE_SCHEMA.md` — persistence ownership and paths.
- `CAMPAIGN_PACK_SCHEMA.md` / `MONSTER_PACK_SCHEMA.md` — declarative pack formats.
- `MEMORY_AUDIT.md` — stack reservations, estimated source-visible usage and memory ownership.
- `DEVICE_TEST_MATRIX.md` — device validation checklist.

### Initiative roll behavior

Initiative refreshes the active character from the current profile when launched. The player modifier includes Dexterity, Initiative Misc, exhaustion, and recognized initiative-related features currently mapped by the app. Each participant can independently use Normal, Advantage, or Disadvantage for generated rolls; a directly edited initiative total remains exactly the entered total until the participant is rolled again.

### Initial traits and level progression

`Grant Initial Traits` stages the initial species/background/class/subclass grants into the Grant Review screen before applying them. Deterministic level progression updates numeric resources/features automatically, while spell choices remain player-selected; when a level increases the spell/cantrip allowance the app prompts the player to choose spells.
