<h1 align="center"><a href='https://rogue-master.net'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/rmlogo.png" width="40%"></a>
<br><a href='https://discord.gg/gF2bBUzAFe' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Discord.png" alt='Discord' title='Discord'></a>
&nbsp;<a href='https://github.com/RogueMaster/flipperzero-firmware-wPlugins/releases/latest' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Github.png" alt='Firmware GitHub' title='Firmware GitHub'></a>
&nbsp;<a href='https://www.patreon.com/RogueMaster?filters[tag]=Latest%20Release' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Patreon.png" alt='Latest PATREON Release' title='Latest PATREON Release'></a>
&nbsp;<a href='https://github.com/RogueMaster/awesome-flipperzero-withModules' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Resources.png" alt='More Research / Assets' title='More Research / Assets'></a></h1>

# DNDolphins

DNDolphins is an offline 5E-compatible character, combat, campaign and encounter suite for Flipper Zero. It is split into seven FAPs so character management, Inventory, Spellbook, Adventure, Journal, Initiative and Bestiary can each keep their own working set small while sharing the same active character where appropriate.

## Common controls

- **Up / Down:** move through menu rows, records or choices. Held navigation repeats where the screen supports repeat input.
- **Left / Right:** adjust the selected value, change a page or cycle a filter when that screen supports it.
- **OK:** open, select, edit or perform the highlighted action.
- **Hold OK:** performs the screen-specific alternate action documented below, such as opening profile actions, quick-equipping an Item, quick-preparing a Spell, opening filters or entering a full numeric editor.
- **Back:** returns to the previous screen. On the main screen of a companion FAP, Short Back returns to DNDolphins when it is installed.
- **Hold Back:** exits the current FAP to firmware instead of returning to DNDolphins.

When a companion FAP returns to DNDolphins, Home automatically refocuses the option that launched it, so returning from Inventory, Spellbook, Adventure, Journal, Initiative or Bestiary does not drop the cursor at an unrelated menu item.

## DNDolphins — character and combat hub

### Home options

The Home menu is presented in this order:

1. **Characters** — Opens the character list. Short OK opens/switches to a character or creates **+ New Character**; Hold OK on an existing character opens Profile Actions.
2. **Character** — Opens identity, classes, leveling mode, languages, progression choices and explicit grant actions for the active character.
3. **Vitals** — Opens HP, AC, speed, initiative, exhaustion, death saves, Hit Dice and passive score values.
4. **Abilities & Saves** — Edits the six ability scores and their saving-throw proficiency/misc modifiers while showing the calculated save totals.
5. **Skills** — Edits all 18 skill proficiency/expertise states and misc modifiers while showing calculated totals.
6. **Features & Perks** — Opens the character-owned Feature list for class features, feats, perks, limited-use resources and recharge settings.
7. **Inventory** — Saves the active character and launches DNDInventory for that character.
8. **Magic & Spells** — Opens casting statistics, spell-slot resources, Arcane Recovery and the DNDSpellbook launcher.
9. **Bestiary** — Launches DNDBestiary. Bestiary can also operate when no character is available.
10. **Initiative** — Saves the active character and launches DNDInitiative.
11. **Combat** — Opens weapon attacks, spell attacks, rituals, attack templates, recovery actions and combat-state controls.
12. **Dice Roller** — Opens the general-purpose dice roller.
13. **Adventure** — Saves the active character and launches DNDAdventure.
14. **Journal** — Saves the active character and launches DNDJournal.

### Characters and Profile Actions

Hold OK on an existing character to open these actions in order:

1. **Switch / Open** — Makes the selected profile active and opens it.
2. **Rename Active** — Renames the profile when it is the active character; switch to it first when necessary.
3. **Duplicate** — Creates a new local character from the selected profile, including its Inventory, Spellbook and Features when those sidecars exist.
4. **Export** — Writes an export set for the selected character, including its current Inventory, Spellbook and Feature sidecars when present.
5. **Import First Export** — Imports the first compatible exported character set as a new local profile and makes it active; matching exported Inventory, Spellbook and Feature sidecars are restored with it when present.
6. **Archive** — Archives a non-active character together with its current Inventory, Spellbook and Feature sidecars so the character is removed from the active character list as one set.
7. **Delete** — Deletes the selected character plus its DNDolphins-owned Inventory, Spellbook, Feature and grant-state sidecars. If another character exists, a survivor becomes active; deleting the final profile leaves **+ New Character**.
8. **Verify Save** — Reads the selected profile and reports whether the character can be loaded.
9. **Restore Backup** — Restores the active character from its available recovery copy when one exists.

### Fresh-character defaults and automatic setup

A new character starts as **New Hero**, Human, Adventurer, True Neutral, Fighter 1 with Milestone leveling, Common, 10 HP, AC 10 and the standard ability array assigned as STR 15, DEX 14, CON 13, INT 12, WIS 10 and CHA 8. Three editable attack templates are also created: Unarmed Strike, Spell Attack and Saving Throw Action.

Choosing a bundled class automatically applies that class's normal Hit Die and spellcasting classification, and choosing a subclass defaults to choices compatible with the selected class. In the subclass catalog, **Hold OK** toggles the normal class-filtered list and **All** subclasses; Hold OK on the Class or Subclass field itself opens custom text entry. When an older/imported character contains a recognized class but its spellcasting setup or normal slot pool is unset, opening DNDolphins can fill the missing class-derived spellcasting data instead of requiring the user to rebuild it manually.

### Character options

The Character screen is presented in this order:

1. **Name** — OK edits the character name.
2. **Player** — OK edits the player name.
3. **Species** — OK opens the bundled Species catalog; Hold OK enters a custom Species name.
4. **Background** — OK opens the bundled Background catalog; Hold OK enters a custom Background name.
5. **Alignment** — OK opens the bundled Alignment catalog; Hold OK enters a custom Alignment value.
6. **Classes** — Opens the multiclass list. Class records store class name, level, subclass, Hit Die and class-specific progression data.
7. **Total Level / Proficiency Bonus** — Displays the calculated total character level and proficiency bonus.
8. **XP** — OK adds 100 XP, Left/Right changes XP by 100, and Hold OK opens full numeric entry.
9. **Leveling** — Toggles XP or Milestone leveling.
10. **Languages** — Opens the character language list.
11. **Other proficiencies** — Edits the free-form proficiency field.
12. **Inspiration** — Toggles Inspiration.
13. **Level Choices** — Opens the next pending ASI/Feat choice. ASI +2 and ASI +1/+1 apply the ability change directly; progression feat choices open the Feat catalog in **Allowed** mode.
14. **Grant Initial Traits** — Applies missing deterministic level-1 species/background/class/subclass traits and spells. The action reports **Updated** when character state changes or **No changes** when all applicable starting grants are already present.
15. **Apply Level Grants** — Applies missing deterministic grants through the character's current levels. Increasing a class level does not run this action automatically.

When a class level increases, HP grows by the class's fixed-average Hit Die value plus Constitution modifier, with a minimum gain of 1 HP per level. Class and global Hit Dice current values are set to their new maximums, while ASI/Feat and other player choices remain explicit. The XP value is raised to at least the minimum threshold for the resulting total level.

### Classes and class editor

The Classes list begins with **+ Add New**, followed by each class in the multiclass build. Short OK opens a class record; Left/Right adjusts the highlighted value, Short OK performs the field's normal cycle/adjust action, and Hold OK opens full numeric entry or custom text where supported.

Class options are presented in this order:

1. **Name** — Short OK opens the bundled Class catalog; Hold OK edits a custom class name. Choosing a bundled class also configures its normal Hit Die and spellcasting mode.
2. **Subclass** — Short OK opens subclasses compatible with the selected class; Hold OK edits a custom subclass. Inside the catalog, Hold OK toggles **Class filter / Showing all**.
3. **Class level** — Changes this class's level while respecting total character level 20. Raising it applies fixed-average HP, refreshes Hit Dice, updates spell progression and opens the level-up review, but does not apply deterministic Features/spells until **Apply Level Grants** is selected.
4. **Hit Point Die** — Sets this class's Hit Die.
5. **Class Hit Dice** — Sets the currently available Hit Dice for this class.
6. **Class Hit Dice max** — Sets this class's Hit Dice maximum.
7. **Casting mode** — Cycles None, Full, Half, Third, Pact, Spell Points and Custom.
8. **Casting ability** — Selects the ability used by this class's spellcasting calculations.
9. **Cantrip limit** — Stores the class's current cantrip allowance; normal bundled class progression updates this when the class level changes.
10. **Prepared limit** — Displays Known/Prepared counts and sets the class's prepared-spell allowance.
11. **Spellbook size** — Stores a class spellbook-size value for classes that use one.
12. **Pact slot level** — Sets the level of this class's Pact Magic slots.
13. **Pact slots** — Sets currently available Pact Magic slots.
14. **Pact slots max** — Sets the maximum Pact Magic slots.
15. **Mystic Arcanum** — Toggles stored Mystic Arcanum levels used by the Warlock resource model.
16. **Spell points** — Sets current spell points.
17. **Spell points max** — Sets the maximum spell-point pool.
18. **Delete class** — Removes this class record.

### Grant review behavior

**Grant Initial Traits** and **Apply Level Grants** apply every deterministic grant the app can resolve and report **Updated** or **No changes**. If a grant cannot be safely resolved, the app opens **Review grants before apply** instead of silently choosing for the player.

- **Apply all pending** applies every currently pending review record.
- **Grant row / Short OK** applies a pending grant, or changes a skipped grant back to Pending.
- **Grant row / Hold Left** marks a pending grant Skipped.
- **Grant row / Hold OK** opens the full grant record editor.
- **+ Add Custom Grant** creates an editable custom grant record for advanced/manual progression handling.

### Vitals options

Left/Right adjusts the highlighted numeric value; Hold OK opens full numeric entry. Vitals are presented in this order:

1. **Current HP** — Sets current hit points.
2. **Maximum HP** — Sets maximum hit points and clamps Current HP if necessary.
3. **Temporary HP** — Sets temporary hit points.
4. **Armor Class** — Sets the character's current AC. Inventory can also calculate and explicitly apply equipped armor/shield AC.
5. **Speed** — Sets base walking speed in feet; Exhaustion-adjusted effective speed is shown when applicable.
6. **Initiative** — Displays the calculated initiative modifier; adjusting this option edits Initiative misc because the total itself is derived.
7. **Initiative misc** — Adds a manual modifier to initiative.
8. **Exhaustion** — Sets Exhaustion from 0–6; it affects displayed speed and initiative/combat calculations.
9. **Death saves** — Tracks successful death saves up to 3.
10. **Death fails** — Tracks failed death saves up to 3.
11. **Hit die** — Sets the legacy/global Hit Die display value. Per-class Hit Dice are maintained in Classes and used by **Spend Hit Die**.
12. **Hit dice current** — Sets the global current Hit Dice value.
13. **Hit dice maximum** — Sets the global maximum Hit Dice value.
14. **Passive Perception** — Displays 10 + the current Perception modifier; adjusting it changes Perception misc.
15. **Passive Insight** — Displays 10 + the current Insight modifier; adjusting it changes Insight misc.
16. **Passive Investigation** — Displays 10 + the current Investigation modifier; adjusting it changes Investigation misc.

### Abilities & Saves controls

Each row represents STR, DEX, CON, INT, WIS or CHA. Left/Right edits the active mode; Hold Left/Right switches between **ability score** editing and **saving-throw misc** editing, while OK cycles saving-throw proficiency and Hold OK opens numeric entry for the active numeric value.

### Skills controls

Each row represents one of the 18 skills and displays its calculated total. Left/Right edits the active mode; Hold Left/Right switches between **proficiency/expertise** editing and **skill misc** editing, while Hold OK opens full numeric entry for the selected skill misc modifier.

### Features & Perks options

The Features list begins with **+ Add New**, followed by character-owned class features, feats, perks and limited-use resources. Short OK opens a Feature; Left/Right adjusts the highlighted field; Hold OK opens full numeric entry or custom text where supported.

Feature options are presented in this order:

1. **Name** — Short OK opens the Feature/Feat catalog; Hold OK edits a custom name. Manual Feature editing is unrestricted.
2. **Notes** — Edits free-form notes.
3. **Source class** — Associates the Feature with one of the character's classes.
4. **Gained at class level** — Records the class level at which the Feature was gained.
5. **Uses** — Sets current uses.
6. **Maximum uses** — Sets the manual maximum use count.
7. **Recharge** — Cycles Manual, Turn, Encounter, Dawn, Short/Long and Long. Initiative refreshes Turn/Encounter resources at their matching cadence; rests refresh the appropriate rest-based resources.
8. **Resource formula** — Cycles Manual, PB and Ability. PB automatically uses the current proficiency bonus; Ability uses the selected ability modifier with a minimum pool of 1.
9. **Resource ability** — Selects the ability used by an Ability-based resource formula and immediately recalculates the maximum.
10. **Delete feature** — Removes the Feature.

During a progression Feat choice, the catalog opens in **Allowed** mode. **Hold OK** toggles **Allowed / All**; Allowed contains only bundled feats whose represented prerequisites and duplicate rules can be positively verified. Unknown/custom entries stay available through All, while `Ability Score Improvement` uses the dedicated ASI +2 and ASI +1/+1 flow instead of the nested Feat picker.

### Magic & Spells options

The Magic screen is presented in this order:

1. **Open Spellbook** — Launches DNDSpellbook. Hold OK performs the same launch.
2. **Casting ability** — Left/Right cycles the ability used for spell attacks and save DCs.
3. **PB / Attack / DC summary** — Displays proficiency bonus, calculated Spell Attack and Spell Save DC; Hold OK recalculates class-derived multiclass slot maximums.
4. **Spell attack misc** — Left/Right changes the modifier; Hold OK opens numeric entry.
5. **Spell save misc** — Left/Right changes the modifier; Hold OK opens numeric entry.
6. **Slots help row** — Documents that Short Left/Right changes available slots and Hold Left/Right changes maximum slots on the level rows below.
7. **Arcane Recovery** — Starts/finishes Wizard Arcane Recovery after a qualifying Short Rest and shows its current state.
8. **Level 1 slots**
9. **Level 2 slots**
10. **Level 3 slots**
11. **Level 4 slots**
12. **Level 5 slots**
13. **Level 6 slots**
14. **Level 7 slots**
15. **Level 8 slots**
16. **Level 9 slots** — On each slot row, Short Left/Right changes current slots, Short OK opens current-slot numeric entry, Hold Left/Right changes the maximum, and Hold OK opens maximum-slot numeric entry. During Arcane Recovery, applicable level 1–5 slot rows instead spend or undo recovery budget.

Multiclass full/half/third-caster slot rules, Pact Magic, spell points, Mystic Arcanum and supported recovery resources are calculated from the character's class data. Eldritch Knight and Arcane Trickster use Wizard-list spell eligibility with third-caster progression.

### Combat options

Combat is presented in this order:

1. **Attack mode** — Cycles Normal, Advantage and Disadvantage for attack rolls.
2. **Weapon Attacks** — Lists usable Inventory weapons and performs attack/damage resolution. Ammunition can come from a weapon's own counter or matching Inventory stacks; loose ammunition matches the required token anywhere in the Item name, so names such as `Fire Arrow` can satisfy an `arrow` requirement.
3. **Spell Attacks** — Lists eligible Spellbook spells and resolves attack/save, damage/healing and resource use from structured spell metadata.
4. **Rituals** — Lists eligible known ritual spells that can be cast through the ritual path without consuming a slot when the character has the applicable ritual capability.
5. **Attack Templates** — Opens saved Unarmed, Spell Attack, Saving Throw or Custom attack templates. Hold OK on an existing template opens its full editor.
6. **Initiative Tracker** — Launches DNDInitiative.
7. **HP** — Adjusts current HP; Hold OK opens full numeric entry.
8. **Temporary HP** — Adjusts temporary HP; Hold OK opens full numeric entry.
9. **Short Rest** — Applies supported Short Rest recovery and enables applicable recovery choices such as Arcane Recovery.
10. **Spend Hit Die** — Chooses a class Hit Die and rolls healing while reducing that class's remaining Hit Dice.
11. **Long Rest** — Applies supported Long Rest recovery for HP, Hit Dice, spell resources, free casts and Feature recharge cadences. Reaction state remains under the separate Reaction control.
12. **Conditions** — Edits character conditions.
13. **Concentration** — Edits the currently concentrated-on effect/spell name.
14. **Reaction** — Toggles the reaction Ready/Used state.
15. **Temp effects** — Edits temporary effects.
16. **Resistances** — Edits resistance text.
17. **Immunities** — Edits immunity text.
18. **Vulnerabilities** — Edits vulnerability text.
19. **Senses** — Edits senses.
20. **Movement** — Edits movement modes.
21. **Death success** — Adjusts death-save successes; Hold OK opens numeric entry.
22. **Death failure** — Adjusts death-save failures; Hold OK opens numeric entry.
23. **Exhaustion** — Adjusts Exhaustion; Hold OK opens numeric entry.

Weapon combat uses STR/DEX/finesse rules, proficiency, magic bonuses, versatile damage, extra dice, riders and critical dice doubling. For weapons with the Ammunition property, a weapon-local Ammo Current/Maximum counter is used first when configured; otherwise Combat consumes one quantity from the first non-weapon Inventory stack whose name or Ammo Group contains the required ammunition token, case-insensitively. Standard families normalize to `arrow`, `bolt`, `bullet` or `needle`, so **Fire Arrow**, **Silvered Arrows**, **Crossbow Bolt Bundle** and similar descriptive names can work without being named exactly `Arrows` or `Bolts`. Older/custom bows, crossbows, slings, blowguns, muskets and pistols can infer the standard ammunition family from the weapon name when Ammo Group is empty.

Spell combat supports source-class casting modifiers, cantrip scaling, higher-level casting, multiple attack/roll instances and supported secondary effects. Short Rest restores Short/Long Features and enables Arcane Recovery; Long Rest restores HP, spell slots, Pact slots, spell points, free casts and applicable Features, clears temporary HP/death-save marks and reduces Exhaustion by one. Reaction Ready/Used state is edited separately.

### Dice Roller controls

The Dice Roller contains Dice Count, Die, Modifier, Mode and the Roll action/result row. Left/Right adjusts the selected setup value, OK rolls when the Roll row is selected, and Hold OK on Dice Count, Die or Modifier opens full numeric entry. Modes include Normal, Advantage, Disadvantage and Guidance. Advantage, Disadvantage and Guidance are d20 conveniences: Guidance automatically adds a rolled d4 to a 1d20 roll, while Advantage/Disadvantage rolls two d20s and keeps the appropriate result. Changing the dice setup away from the supported d20 form returns the roller to Normal mode.

## DNDInventory — inventory, equipment and currency

DNDInventory opens the persisted active character directly to the Inventory list. A truly empty Inventory that has never received starting equipment automatically applies the character's initial class/species/background equipment package once.

### Inventory list

The list begins with **+ Add New**, followed by owned Items in stored order.

- **+ Add New** — Short OK or Hold OK creates a blank Item and opens the Item Editor.
- **Item row / Short OK** — Opens that Item in the Item Editor.
- **Item row / Hold OK** — Toggles Equipped and saves immediately.
- **Short Left / Right** — Moves to the previous/next eight-record Inventory page.
- **Hold Left / Right** — Decreases/increases the selected Item's Stack Qty by one, clamped to 0–999.
- **Hold Up** — Opens **Inventory Tools**.

### Inventory Tools

Inventory Tools are presented in this order:

1. **Currency** — Opens CP, SP, EP, GP and PP. Left/Right changes the selected denomination by one; OK opens full numeric entry.
2. **Inventory Resources** — Opens derived carrying/equipment information and actions.
3. **Grant Initial Inventory** — Short OK applies the normal starting-equipment grant when it has not been used. Hold OK invokes the one-time explicit regrant override when that override is available.

### Inventory Resources

The resource screen is presented in this order:

1. **Carried** — Displays carried weight.
2. **Equipped** — Displays equipped weight.
3. **Capacity** — Displays calculated carrying capacity.
4. **Encumbrance** — Left/Right or OK toggles Standard/Variant encumbrance.
5. **Attuned** — Displays attuned Item count as `x/3` and marks the count with `!` when more than three Items are attuned. It reports the overage but does not delete or forcibly unattune Items.
6. **Formula AC** — Displays AC calculated from equipped armor/shields and character ability data.
7. **Apply armor/shield AC** — Writes the calculated armor/shield AC to the character.
8. **Normalize coin values** — Normalizes currency denominations while preserving total value.
9. **Capacity override** — Left/Right changes the explicit carrying-capacity override. A value above zero replaces the normal Strength × 15 lb capacity calculation.

### Item Editor

Item options are presented in this order. Left/Right or Short OK performs the normal toggle/cycle/increment action; Hold OK opens full numeric entry on supported numeric fields.

1. **Name** — Short OK opens the Item catalog; Hold OK edits a custom Item name.
2. **Notes** — Edits free-form Item notes.
3. **Stack Qty** — Sets the owned quantity; the Inventory list also supports Hold Left/Right for quick ±1 changes.
4. **Weight** — Sets per-item weight in tenths of a pound for carrying calculations.
5. **Equipped** — Toggles whether the Item is equipped.
6. **Attuned** — Toggles attunement; Inventory Resources counts attuned Items against the normal three-item limit.
7. **Weapon** — Marks the Item as usable by Weapon Attacks.
8. **Attack ability** — Selects Auto, Strength, Dexterity or Best. Auto uses DEX for ranged weapons, the better STR/DEX modifier for finesse weapons, and STR otherwise.
9. **Proficient** — Adds the current proficiency bonus to the Item's attack modifier when enabled.
10. **Magic bonus** — Adds the magic bonus to the attack modifier.
11. **Damage dice** — Sets the number of base damage dice.
12. **Damage die** — Sets the base damage die.
13. **Versatile** — Toggles the Versatile property. Enabling it creates an initial d8 alternate die that can then be changed with **Versatile die**.
14. **Versatile die** — Sets the alternate two-handed damage die.
15. **Use versatile** — Chooses the Versatile die for attack damage when available.
16. **Type** — Selects damage type.
17. **Finesse** — Enables finesse STR/DEX selection.
18. **Ranged** — Marks the weapon ranged and makes Auto use DEX.
19. **Light** — Stores the Light weapon property.
20. **Heavy** — Stores the Heavy weapon property.
21. **Thrown** — Stores the Thrown weapon property.
22. **Ammunition** — Marks the weapon as requiring ammunition.
23. **Add ability dmg** — Toggles whether the attack ability modifier is added to damage.
24. **Extra dice** — Sets additional damage/rider dice.
25. **Extra die** — Sets the additional die size.
26. **Ammo** — Sets the weapon-local current ammunition counter. When a local counter is configured, Weapon Attacks consume it before looking for loose ammunition stacks.
27. **Maximum ammo** — Sets the weapon-local ammunition maximum.
28. **Attack / damage summary** — Displays the calculated attack modifier and current base damage dice; this row is informational.
29. **Container** — Assigns the Item to another owned Item or to Carried. An Item cannot contain itself, and container references are remapped after deletions so they do not silently point to a different surviving record.
30. **Charges** — Sets current charges.
31. **Charges max** — Sets maximum charges.
32. **Armor base AC** — Sets armor's base AC for Formula AC.
33. **Armor DEX cap** — Sets the maximum DEX modifier included by the armor; `-1` means uncapped.
34. **Shield AC bonus** — Sets the shield bonus added to Formula AC.
35. **Ammo group** — Edits the weapon/ammunition family used by Combat matching. Exact Item names are not required: the relevant token may appear anywhere in a loose-ammunition Item name.
36. **Delete item** — Removes the Item.

The Item catalog supports All, Weapons, Armor, Ammunition, Gear, Tools, Mounts/Vehicles, Potions, Rings, Rods, Scrolls, Staffs, Wands, Wondrous and Magic filters. **Hold OK** in the catalog advances to the next category filter; Short Left/Right changes catalog pages; Short OK applies the selected catalog entry. Generic Spell Scroll entries cover Cantrip and Levels 1–9 with level-appropriate rarity.

Choosing a recognized bundled weapon or armor also fills its useful mechanical preset—weight, damage, weapon properties, Versatile die, ammunition family, armor base/DEX cap or shield bonus—so it can be used by Combat and Formula AC without manually rebuilding standard equipment statistics. **Normalize coin values** converts the current CP/SP/EP/GP/PP mix into larger denominations while preserving the same total copper-piece value.

## DNDSpellbook — spells, preparation and catalog

DNDSpellbook opens the persisted active character directly to the Spellbook list. Owned spells are stored in level-ascending, case-insensitive name order.

### Spellbook list

The list begins with **+ Add New**, followed by owned Spells.

- **+ Add New** — Short OK or Hold OK creates a blank Spell and opens the Spell Editor.
- **Spell row / Short OK** — Opens that Spell in the Spell Editor.
- **Spell row / Hold OK** — Toggles Prepared for a Known spell and saves immediately. Always Prepared spells stay prepared; unknown spells are not quick-prepared.
- **Short Left / Right** — Moves to the previous/next eight-record Spellbook page.
- **Hold Up** — Opens **Spell Filters**.

List status marks are `A` for Always Prepared, `P` for Prepared, `K` for Known and `-` for neither; `F` also appears while a free cast remains.

### Spell Filters

Filters are presented in this order:

1. **Level** — Any, Cantrip or Levels 1–9.
2. **Class** — **Character Classes** is the default and represents the union of the character's actual spell lists. The selector also provides **Any Class** plus Artificer, Barbarian, Bard, Cleric, Druid, Fighter, Monk, Paladin, Ranger, Rogue, Sorcerer, Warlock and Wizard even when the character does not own that class.
3. **Ritual** — Any or Ritual Only.
4. **School** — Any or a specific spell school.
5. **Source** — Filters by catalog source.
6. **Status** — Any, Prepared, Known or Always Prepared.
7. **Eligibility** — **Allowed** enforces the character's actual spell-list access and permitted spell level; **All Spells** treats the selected class as a catalog-membership filter without requiring character eligibility. **Any Class + All Spells** exposes the complete bundled catalog.

Left/Right changes the selected filter. OK returns to the list/catalog and reapplies the filters.

### Spell Catalog

The bundled 448-spell catalog is sorted by spell level and then name. Short Left/Right changes catalog pages, Short OK selects the highlighted Spell, and Hold OK opens Spell Filters without leaving the catalog workflow.

### Spell Editor

Spell options are presented in this order. Left/Right or Short OK performs the normal toggle/cycle/increment action; Hold OK opens full numeric entry on supported numeric fields.

1. **Name** — Short OK opens the Spell catalog; Hold OK edits a custom Spell name.
2. **Notes** — Edits free-form Spell notes.
3. **Source class** — Selects which owned class supplies the Spell's casting context. Catalog selection automatically resolves a compatible owned class when possible.
4. **Level** — Sets Cantrip/0 through Level 9.
5. **Known** — Marks whether the character knows the Spell. Choosing a Spell from the catalog marks it Known automatically.
6. **Prepared** — Marks the Spell Prepared.
7. **Always prepared** — Keeps the Spell prepared regardless of quick-prepare toggles.
8. **Ritual** — Marks ritual capability for the Rituals combat path.
9. **Free casts** — Sets remaining no-slot casts.
10. **Free casts max** — Sets the maximum free-cast pool restored by the supported recovery logic.
11. **Use one free cast** — Consumes one remaining free cast without spending a spell slot.
12. **Stable ID** — Stores the stable catalog/progression identifier used to avoid duplicate deterministic grants.
13. **Source** — Stores the Spell's catalog source/provenance.
14. **School** — Stores the spell school and supports School filtering.
15. **Grant source** — Shows/edits the progression source label when the Spell came from a deterministic grant.
16. **Grant type** — Stores the grant-source type used by progression bookkeeping.
17. **Delete spell** — Removes the Spell.

Choosing a catalog Spell copies its level, ritual, school, source and a stable ID in addition to the name. Owned Spellbook records are automatically kept in level-ascending, case-insensitive name order after additions or edits. Eldritch Knight and Arcane Trickster resolve Wizard-list eligibility through their third-caster progression.

## DNDAdventure — campaigns and choices

DNDAdventure uses the exact persisted active character and stores campaign progress per character. Campaigns are declarative scene/choice data with narrative text, skill checks, flags, achievements, checkpoints, milestones and Item rewards.

### Campaign menu

The menu lists each bundled/enabled campaign in catalog order, followed by **Restart Current Adventure**.

- **Campaign entry / OK** — Opens that campaign at its saved/current scene or starts it when no progress exists.
- **Restart Current Adventure / OK** — Opens a confirmation screen. Choosing Restart resets that campaign's scene, checkpoint, flags and achievements for the active character.

Bundled campaigns include **Reef Wardens**, **Ghost Protocol**, **Torii Between Tides** and **Moonlit Market**. Installed campaign packs can add more entries through the same campaign format.

### Adventure controls

- **OK on Start Adventure** begins interaction with the loaded scene.
- **Up / Down** moves through the scene's available choices.
- **Short OK** chooses the highlighted action and applies any check, flag, reward or branch associated with it.
- **Hold OK** opens the full-scene text viewer.
- **Hold Left** loads the saved checkpoint.
- **Hold Right** saves the current scene as the checkpoint.
- In the full-text viewer, Up/Down scroll one line, Left/Right moves five lines, and OK returns to the scene.
- A skill-check result screen shows the natural d20, modifier, total, DC and PASS/FAIL; OK continues.
- Short Back saves progress and returns to the Campaign menu; Short Back from the Campaign menu returns to DNDolphins; Hold Back exits to firmware.

Adventure skill checks roll against the active character's real skill modifier, including proficiency/expertise and misc modifiers. Progress, flags, achievements and checkpoint state are stored per active character, so two characters can play the same campaign independently. Item rewards append directly to the active character's Inventory, and guarded quest/achievement rewards are one-shot: revisiting the same guarded branch does not duplicate the Item or milestone. Milestones are written as Journal entries and can later continue the matching active Adventure.

## DNDJournal — notes, milestones and handoffs

DNDJournal uses the exact persisted active character. Entries are timestamped, stored per character and presented newest first.

### Journal list

The list shows existing entries followed by **+ New Entry**.

- **Entry / OK** — Opens the entry detail screen.
- **+ New Entry / OK** — Creates a new entry and opens it for editing.
- Up/Down navigates the list; Short Back returns to DNDolphins; Hold Back exits to firmware.

### Entry options

Entry detail is presented in this order:

1. **Category** — Cycles Quick, Adventure, Item and Milestone. Left/Right or OK changes the category.
2. **Title** — OK opens text editing.
3. **Body** — OK opens text editing.
4. **Complete** — Left/Right or OK toggles completion.
5. **Level class** — For Milestone entries, Left/Right or OK selects one of the character's classes.
6. **Apply milestone level** — For Milestone entries, applies that milestone level once using the same fixed-average HP and Hit Dice advancement used by direct class leveling.
7. **Create inventory item** — For Item entries, creates an Inventory Item from the Journal entry.
8. **Continue active Adventure** — For a matching Milestone entry, launches Adventure continuation for the active campaign.
9. **Delete Entry** — Deletes the current Journal entry.

Applying a Milestone marks that entry Complete and records that its level was granted before changing the character, so the same Journal entry cannot grant two levels. Milestone leveling advances HP/Hit Dice and class progression but leaves deterministic Features/spells for the explicit **Apply Level Grants** action in DNDolphins. Creating an Inventory Item from an Item entry uses the Journal Title as the Item name, Body as Item notes and creates a quantity-1 carried Item that can then be completed in DNDInventory.

## DNDInitiative — roster, turn order and completed encounters

DNDInitiative follows the persisted active character and stores its own per-character roster/combat state.

### Main menu

The main menu is presented in this order:

1. **Start New Combat** — Copies the saved Party Roster into a fresh setup screen and prepares initiative totals for rolling/editing.
2. **Resume** — Returns to the current active combat when one exists.
3. **Party Roster** — Opens persistent participants plus **+ New** for reusable allies/NPCs.
4. **Edit Current Order** — Returns to the setup/order screen for the current participant list.
5. **End Current Combat** — Opens **End + Save History**, **End Without History** and **Cancel**.
6. **Default Roll** — Left/Right cycles Normal, Advantage and Disadvantage for newly rolled initiative.

### Party Roster controls

Short OK opens an existing participant or **+ New**. Participant fields include Name, Initiative Modifier, Roll Mode, Armor Class, Current HP, Maximum HP, Conditions and Delete; Left/Right adjusts numeric/cycle fields and OK opens the appropriate text/numeric editor or action.

### Combat setup controls

Setup contains **Roll for All**, each participant, **+ Temporary Member** and **Begin Combat**.

- **Short Left / Right on a participant** adjusts its initiative total by one.
- **Hold Up on a participant** increases its AC by one as a quick adjustment.
- **Hold Left / Right on a participant** moves it earlier/later in the current order.
- **Hold OK on a participant** opens the full participant editor.
- **Roll for All** rolls initiative according to each participant's roll mode; ties are ordered by initiative modifier.

### Active combat controls

- Up/Down changes the selected participant; the current turn is marked separately.
- **Short OK** advances the current turn.
- **Short Left / Right** adjusts the selected participant's current HP by one and synchronizes the main character when that participant is the active character.
- **Hold Up** moves back one turn, including across a round boundary.
- **Hold Down** opens Conditions text entry for the selected participant.
- **Hold Left / Right** reorders the selected participant.
- **Hold OK** opens the full combat participant editor with Initiative Total, Modifier, Roll Mode, AC, current/max HP, Conditions and Delete.
- Short Back returns to the Initiative main menu without ending combat; **Resume** returns to the same encounter.

When Initiative opens with an active character, it adds or refreshes that character in the Party Roster instead of requiring a duplicate manual participant. The main character's initiative modifier is rebuilt from DEX + Initiative misc, with supported Alert proficiency-bonus or Jack of All Trades half-proficiency behavior and the app's Exhaustion penalty. HP, maximum HP and AC edits made to that main-character participant synchronize back to the character profile.

Starting a new combat refreshes character Features with **Encounter** recharge; advancing each turn refreshes **Turn** recharge. Ending combat can optionally save a timestamped Initiative-owned history record containing the encounter end time, rounds, party HP/AC/conditions and surviving opponents. Bestiary can hand a single monster or a complete generated/saved encounter directly into Initiative.

## DNDBestiary — monsters and encounter generation

DNDBestiary can browse/generate without a character profile. When a character is available, its persisted ID is used for display and Initiative handoff.

### Home options

The Bestiary Home menu is presented in this order:

1. **Browse Monsters** — Opens the streamed monster catalog using the current Search, Max CR, Type, Source, Browse Environment and Browse Role filters. In the list, Up/Down selects a monster, Left/Right changes the catalog window/page, and OK opens the full stat block.
2. **Generate Encounter** — Generates an encounter from Party Level, Party Size, Difficulty, Encounter Environment, Encounter Role, Repeat Types and Template. In an encounter, OK opens a monster/stat action, Simulator, Save Name, Warnings or Send to Initiative as selected; Hold OK regenerates with the current settings.
3. **Party Level** — Left/Right selects levels 1–20 for encounter generation and simulation.
4. **Party Size** — Left/Right selects party size 1–12.
5. **Difficulty** — Left/Right cycles Low, Moderate and High encounter targets.
6. **Encounter Env** — Left/Right cycles the preferred encounter environment.
7. **Encounter Role** — Left/Right cycles the preferred monster role.
8. **Repeat Types** — Left/Right toggles whether the generator may repeat monster types.
9. **Template** — Left/Right cycles Balanced, Horde and Elite generation templates.
10. **Saved Encounters** — Opens named saved encounters. Short OK resumes one; Hold OK opens its actions: Resume, Send to Initiative, Rename, Duplicate, Archive and Delete.
11. **Search** — OK opens monster-name text search.
12. **Max CR** — Left/Right cycles the maximum CR filter.
13. **Type** — Left/Right cycles creature type.
14. **Source** — Left/Right cycles monster source.
15. **Browse Env** — Left/Right cycles browse environment.
16. **Browse Role** — Left/Right cycles browse role.
17. **Saved Filters** — Opens saved browse-filter presets. Short OK applies a preset; Hold OK deletes the selected preset. The final list action creates a new preset from the current browse filters.
18. **Favorite Monsters** — Opens the saved Favorites monster list.
19. **Recent Monsters** — Opens recently viewed monsters.
20. **Create Custom Monster** — Opens the custom-monster editor for a new record.
21. **Pack Diagnostics** — Opens pack status/recovery information and reruns pack diagnostics with OK.

Party Level and Party Size are persisted between Bestiary sessions. Opening a monster automatically adds it to **Recent Monsters**, while Favorites, Saved Filters and Saved Encounters persist until the user changes them. Search is case-insensitive substring matching, so a partial piece of a monster name is enough.

### Monster detail controls

A monster detail screen exposes CR/XP, AC/HP, type/source/role, size/alignment, speed, abilities, skills, defenses, senses, languages, traits, actions and additional text. OK opens long detail lines where applicable; those viewers use Up/Down for single-line scroll, Left/Right for five-line movement and OK to return.

Detail actions also allow Favorite toggle and Send to Initiative. Custom monsters additionally expose Edit and Delete; Delete asks for confirmation, and Hold OK on Delete provides the alternate confirmation path.

### Custom Monster editor

Custom Monster options are presented in this order:

1. **Name** — Sets the custom monster name.
2. **CR** — Sets Challenge Rating.
3. **XP** — Sets XP used by encounter generation/simulation.
4. **AC** — Sets Armor Class.
5. **HP** — Sets Hit Points.
6. **Type** — Sets creature type text.
7. **Environment** — Sets the environment used by browse/generation filters.
8. **Role** — Sets the encounter role used by browse/generation filters.
9. **Size / alignment** — Stores size and alignment text.
10. **Speed** — Stores movement text.
11. **STR** — Sets Strength.
12. **DEX** — Sets Dexterity and therefore the initiative modifier used when the monster is sent to Initiative.
13. **CON** — Sets Constitution.
14. **INT** — Sets Intelligence.
15. **WIS** — Sets Wisdom.
16. **CHA** — Sets Charisma.
17. **Skills** — Stores skill text.
18. **Defenses** — Stores saves/resistances/immunities or other defense text.
19. **Senses** — Stores senses.
20. **Languages** — Stores languages.
21. **Traits** — Stores traits.
22. **Actions** — Stores actions/attacks.
23. **Extra** — Stores additional stat-block text.
24. **Save / Update Custom Monster** — Writes the custom record. Existing custom monsters can also be deleted from their detail screen after confirmation.

### Encounter tools

Generated encounters include the monster composition, an XP/difficulty simulator, a save/name action, composition warnings and Send to Initiative. The simulator totals encounter XP against the selected party level/size and classifies it Low, Moderate or High. Composition analysis separately flags **Leader lacks support**, **Artillery is exposed** and **Minion density is high** when those role patterns are detected; warnings are advisory and do not block saving or sending the encounter. Saved encounters can be resumed, sent to Initiative, renamed, duplicated, archived or deleted.

The bundled Bestiary contains 346 indexed/statblock-matched monsters, including original Dungeons & Dolphins creatures such as Tanuki Trickster, Kappa River Scout, Tsukumogami Umbrella, Kitsune Wayfinder, Karasu Tengu Warden and Lantern Onryo.

## Storage

- Character profiles and character-owned sidecars: `/ext/apps_data/dndolphins/`
- Inventory: `inventory_<id>.txt`
- Spellbook: `spellbook_<id>.txt`
- Features: `feats_<id>.txt`
- Applied deterministic grants: `appliedgrants_<id>.txt`
- Journal: `/ext/apps_data/dndjournal/`
- Initiative: `/ext/apps_data/dndinitiative/`
- Adventure: `/ext/apps_data/dndadventure/`
- Bestiary/custom monsters/packs/encounters: `/ext/apps_data/dndbestiary/`

`/ext/apps_data/dndolphins/custom_active_profile.txt` stores the active character as `Active=<id>`. Inventory and Spellbook sidecars remain under the DNDolphins character root so Combat, progression, Adventure and Journal share the same character-owned records.

## Data and memory behavior

Character/profile loading is best-effort by recognized field name. Collection and catalog readers use bounded pages/windows, and each FAP is loaded independently so launching Inventory, Spellbook, Adventure, Journal, Initiative or Bestiary does not keep DNDolphins resident.

For exact storage fields, memory reservations, compatibility rules and pack formats, see the dedicated documentation files in this directory.

## Documentation

- `CHANGELOG.md` — concise released feature/fix history.
- `ROADMAP.md` — future user-facing features not present in the current application.
- `FEATURE_CHECKLIST.md` — current feature coverage by FAP.
- `SAVE_SCHEMA.md` — character and sidecar persistence fields.
- `COMPATIBILITY.md` — cross-version and cross-FAP data compatibility.
- `MEMORY_AUDIT.md` — stack, app-state and transient working-set information.
- `RULES_AUDIT.md` — implemented 5E-compatible rule behavior.
- `DEVICE_TEST_MATRIX.md` — optional hardware acceptance/testing checklist.
- `SOURCE_OWNERSHIP.md` — which FAP owns each shared/user-visible subsystem and where cross-FAP responsibilities live.
- `CAMPAIGN_PACK_SCHEMA.md` / `MONSTER_PACK_SCHEMA.md` — community pack formats.
- `CATALOG_POLICY.md` — catalog row formats and eligibility policy.
- `ACCESSIBILITY.md` — control and readability conventions.
- `ATTRIBUTION.md` — content/source attribution.

## Build

From a RogueMaster/Flipper firmware tree containing this directory:

```text
fbt fap_dndolphins fap_dndadventure fap_dndjournal fap_dndinitiative fap_dndbestiary fap_dndinventory fap_dndspellbook
```
