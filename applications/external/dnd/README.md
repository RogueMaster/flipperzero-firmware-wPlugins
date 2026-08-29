<h1 align="center"><a href='https://rogue-master.net'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/rmlogo.png" width="40%"></a>
<br><a href='https://discord.gg/gF2bBUzAFe' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Discord.png" alt='Discord' title='Discord'></a>
&nbsp;<a href='https://github.com/RogueMaster/flipperzero-firmware-wPlugins/releases/latest' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Github.png"  alt='Firmware GitHub' title='Firmware GitHub'></a>
&nbsp;<a href='https://www.patreon.com/RogueMaster?filters[tag]=Latest%20Release' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Patreon.png"  alt='Latest PATREON Release' title='Latest PATREON Release'></a>
&nbsp;<a href='https://github.com/RogueMaster/awesome-flipperzero-withModules' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Resources.png"  alt='More Research / Assets' title='More Research / Assets'></a></h1>

# Dungeons & Dolphins

Dungeons & Dolphins is an offline 5E-compatible toolkit for Flipper Zero. The source tree builds two independent applications:

- **Dungeons & Dolphins** — character management, spells, inventory, dice, combat, initiative, journal, and campaigns.
- **Dolphin Bestiary** — monster browsing, custom monsters, encounters, saved encounters, and initiative handoff.

## Features

### Character management

- Multiple independent character profiles stored as readable, manually editable text files.
- Automatic saving with backup, validation, recovery, import, export, duplicate, rename, archive, and delete controls.
- Multiclass characters with up to four classes, subclasses, class levels, Hit Dice, and class-linked features.
- Ability scores, saving throws, all 18 skills, proficiency, expertise, miscellaneous modifiers, and passive scores.
- HP, temporary HP, AC, speed, initiative, exhaustion, death saves, inspiration, experience, and milestones.
- Species, background, alignment, languages, feats, tools, armor training, weapon training, size, senses, and proficiencies.

### Magic & spells

- Known, Prepared, Always Prepared, Ritual, and free-cast spell states.
- Per-class spellcasting ability, casting mode, prepared limits, spellbook size, Pact Magic, Mystic Arcanum, and spell points.
- Shared multiclass spell slots, Spell Attack, Spell Save DC, slot spending, Long Rest recovery, and Arcane Recovery.
- Spell filtering by class, level, ritual, school, source, and prepared state.
- Streamed spell catalog with low-memory paging and custom spell support.

### Features, grants, inventory & equipment

- Class, subclass, species, background, feat, item, spell, and custom feature grants.
- Feature uses with manual, proficiency-based, or ability-based maximums and configurable recharge rules.
- Inventory quantities, weight, containers, equipped/attuned state, charges, and ammunition groups.
- Weapon attack data, damage dice, versatile damage, riders, proficiency, magic bonuses, and weapon properties.
- Armor and shield values with calculated AC and optional encumbrance tracking.
- Copper, Silver, Electrum, Gold, and Platinum currency tracking.

### Dice & combat

- Animated d4, d6, d8, d10, d12, d20, and d100 rolls.
- Advantage, Disadvantage, modifiers, multi-die rolls, and Guidance mode.
- Individual dice results, dice sum, modifier, and final total display.
- Weapon attacks, attack templates, spell/save/custom actions, damage, riders, Mastery, and recharge fields.
- Combat tracking for HP, temporary HP, rests, Hit Dice, conditions, concentration, reactions, exhaustion, resistances, immunities, vulnerabilities, senses, and movement.

### Initiative

- Saved Party Roster with name, initiative modifier, AC, current HP, and maximum HP.
- **Roll for All** when starting combat, plus individual automatic initiative rolls.
- Hold OK on a participant for manual d20 initiative entry or participant editing.
- Round and turn tracking with Back moving to the previous turn.
- Hold Back during combat to return to the Initiative screen.
- Temporary participants, negative HP, conditions, and participant removal.
- Bestiary transfers are appended to the active character's saved Party Roster before Initiative opens.

### Journal & campaigns

- Character notes, adventure notes, item notes, and milestones.
- Data-driven Adventure mode with branching choices, checks, rewards, flags, achievements, sprite tags, checkpoints, and 3-second skill-check result displays.
- Campaign pack support with per-character progress and transactional installation.

### Dolphin Bestiary

- 340 bundled monster records with streamed browsing and stat-block viewing.
- Search and filtering by name, challenge, type, source, environment, and encounter role.
- Favorites, recent monsters, saved filters, and diagnostics.
- Custom monster creation, editing, and confirmed deletion without modifying packaged monster data.
- Transactional third-party monster pack installation with enable/disable controls and stable-ID conflict validation.
- Party-level encounter generation with difficulty targeting and role-aware weighting.
- Named saved encounters with resume, rename, duplicate, archive, delete, and **Add to Initiative** actions.
- **Add to Initiative** works from individual monsters, generated encounters, and saved encounters.
- Generated encounters can be saved for later use.
- Full stat-block rows can be opened in a scrolling full-screen reader.

### Interface & performance

- Designed for the Flipper Zero screen and buttons with no network or external hardware required.
- Long selected labels scroll horizontally where space permits.
- Long Back returns toward the main screen; normal Back returns to the previous screen.
- Catalogs, profiles, monsters, and campaign data are streamed or loaded on demand to reduce RAM use.
- Large record collections use bounded pages and release temporary allocations after use.
- Initiative-only launches defer unnecessary campaign and pack initialization to reduce cross-FAP memory pressure.
- User data is kept separate from packaged application assets.

## Controls

- **OK** — select, open, confirm, or roll.
- **Hold OK** — alternate action such as custom text, numeric entry, or participant editing where supported.
- **Back** — return to the previous screen; during combat, move to the previous turn.
- **Hold Back** — return toward the main screen; during combat, return to Initiative.
- **Left / Right** — change pages or values where supported.
- **Up / Down** — move through menu rows and lists.

## Data locations

- Dungeons & Dolphins writable data: `/ext/apps_data/dungeons_and_dolphins/`
- Dolphin Bestiary writable data: `/ext/apps_data/dolphin_bestiary/`
- Character profiles use `profiles/ch_{id}_{characterName}_{characterLevel}.txt`.
- Packaged catalogs, campaigns, and monster tables remain in each FAP's asset namespace.

See `CHANGELOG.md` for version history and `ROADMAP.md` for planned work.
