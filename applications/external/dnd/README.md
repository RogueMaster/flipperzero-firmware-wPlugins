<h1 align="center"><a href='https://rogue-master.net'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/rmlogo.png" width="40%"></a>
<br><a href='https://discord.gg/gF2bBUzAFe' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Discord.png" alt='Discord' title='Discord'></a>
&nbsp;<a href='https://github.com/RogueMaster/flipperzero-firmware-wPlugins/releases/latest' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Github.png" alt='Firmware GitHub' title='Firmware GitHub'></a>
&nbsp;<a href='https://www.patreon.com/RogueMaster?filters[tag]=Latest%20Release' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Patreon.png" alt='Latest PATREON Release' title='Latest PATREON Release'></a>
&nbsp;<a href='https://github.com/RogueMaster/awesome-flipperzero-withModules' target='_blank'><img src="https://raw.githubusercontent.com/RogueMaster/flipperzero-firmware-wPlugins/420/.github/assets/Resources.png" alt='More Research / Assets' title='More Research / Assets'></a></h1>

# DNDolphins

**Release: 4.20.3**

This release randomizes loading artwork across the original splash and fourteen supplied images while keeping one selected bitmap in RAM. It retains four feature FALs, the DND-owned loading FAL, direct per-item bag moves and Hold OK in Bag Mover. Eleven FAPs, five real shared modules and twenty host regressions pass. All changes stay in the DND app family and use the supplied stock firmware API. Install the matching FAPs and FALs together. ARM/device verification remains outstanding. See [FAL_INTEGRATION.md](FAL_INTEGRATION.md) and [RELEASE_AUDIT_STATUS.md](RELEASE_AUDIT_STATUS.md).

DNDolphins is an offline 5E-compatible character, combat, campaign and encounter suite for Flipper Zero. It is split into eleven FAPs so the DNDolphins character hub, DNDCharacter Sheet, Grants, Combat, Inventory, Spellbook, Adventure, Journal, Initiative, Bestiary, and DNDBackup & Restore can each keep their own working set small while sharing the same active character where appropriate.

Startup and app handoffs select from fifteen finished 128×64 images, including Monk, Sorcerer, Warlock and Barbarian, with the existing animated hourglass over the selected image. Overlapping Hub and loading FAL instances share one immutable 1,024-byte bitmap; draw callbacks do no file reads or allocations. A loading sequence keeps its selection until the last owner releases it, and consecutive sequences may repeat. No-argument Hub launch retains the two-second minimum; return/deep-link launches have no minimum. Home becomes active before the startup splash is removed. Raw file assets are automatically unpacked to SD by native preload; they are not fifteen resident icon arrays. The Hub privately owns its Graphical Home images. See [SPLASH_LOADING.md](SPLASH_LOADING.md) for formats, ownership and fallback behavior. Loading before DND code starts remains controlled by stock firmware.

Use it as a pocket companion to your table, not as a replacement for the game itself. DNDolphins is not meant to replace the source material, the Player's Handbook, or a good Dungeon Master. Keep the books handy, respect the rulings at your table, and support your local Dungeon Masters.

## Common controls

- **Up / Down:** move through menu rows, records or choices. Held navigation repeats where the screen supports repeat input.
- **Left / Right:** adjust the selected value, change a page or cycle a filter when that screen supports it.
- **OK:** open, select, edit or perform the highlighted action.
- **Hold OK:** performs the screen-specific alternate action documented below, such as opening profile actions, quick-equipping an Item, quick-preparing a Spell, opening filters or entering a full numeric editor.
- **Back:** returns to the previous screen. On the main screen of a companion FAP, Short Back returns to DNDolphins when it is installed. DNDInitiative returns to DNDCombat instead only when Initiative was launched by Combat's **Jump to Initiative** action.
- **Hold Back:** exits the current FAP to firmware instead of returning to DNDolphins.

When a companion FAP returns to DNDolphins, Home automatically refocuses the option that launched it, so returning from DNDCharacter Sheet, Inventory, Spellbook, Adventure, Journal, Initiative, Bestiary or Combat does not drop the cursor at an unrelated menu item.

## DNDolphins — character and suite hub

### Home options

The Home menu is presented in this order:

1. **Characters** — Opens the character list. Short OK opens/switches to a character or creates **+ New Character**; Hold OK on an existing character opens Profile Actions.
2. **Character** — Opens identity, classes, leveling mode, languages, progression choices and explicit grant actions for the active character.
3. **Vitals** — Opens HP, AC, speed, initiative, exhaustion, death saves, Hit Dice and passive score values.
4. **Abilities & Saves** — Edits the six ability scores and their saving-throw proficiency/misc modifiers while showing the calculated save totals.
5. **Skills** — Edits all 18 skill proficiency/expertise states and misc modifiers while showing calculated totals.
6. **Features & Perks** — Opens the character-owned Feature list for class features, feats, perks, limited-use resources and recharge settings.
7. **Inventory** — Saves the active character and launches DNDInventory for that character.
8. **Magic & Spells** — Saves/unloads DNDolphins and launches DNDSpellbook directly in its **Magic & Spells** view for casting statistics and spell-slot management. Arcane Recovery spending remains part of DNDCombat Short Rest.
9. **Bestiary** — Launches DNDBestiary. Bestiary can also operate when no character is available.
10. **Initiative** — Saves the active character and launches DNDInitiative.
11. **Combat** — Saves the active character, unloads DNDolphins and launches DNDCombat for weapon attacks, spell attacks, rituals, attack templates, recovery actions and combat-state controls.
12. **Dice Roller** — Opens the general-purpose dice roller.
13. **Adventure** — Saves the active character and launches DNDAdventure.
14. **Journal** — Saves the active character and opens the Journal FAL in the Hub; reloads that character after Journal changes.
15. **Settings** — Opens shared Dungeons & Dolphins preferences for dice loading, diagnostics, Get Elevated, catalog scope, Homebrew visibility and **Menu Type**.

### Characters and Profile Actions

Hold OK on an existing character to open these actions in order:

1. **Switch / Open** — Makes the selected profile active and opens it.
2. **Rename Active** — Renames the profile when it is the active character; switch to it first when necessary.
3. **Duplicate** — Creates a new local character from the selected profile, including character-owned sidecars and Inventory bags.
4. **Archive** — Archives a non-active character together with its character-owned sidecars so the profile is removed from the active list as one set.
5. **Delete** — Deletes the selected character plus its DNDolphins-owned sidecars. If another character exists, a survivor becomes active; deleting the final profile leaves **+ New Character**.
6. **Verify Save** — Reads the selected profile and reports whether the character can be loaded.
7. **Backup & Restore** — Launches the standalone **DNDBackup & Restore** FAP for SHD bundle backup and restore. DNDolphins itself does not export, import or restore user backups.

### Settings

Settings are shared across the suite and persist in `/ext/apps_data/dndolphins/settings.txt`.
Settings are reconstructable, but saves now publish through short-lived `settings.tmp` / `settings.bak` rollback files so an interrupted write preserves the last complete `settings.txt`. Successful saves remove the transaction companions. Loading remains best-effort: defaults are established first, each complete valid setting line is applied independently, and missing, unreadable, malformed or truncated data leaves the affected preference at its default without preventing the app from opening.

1. **Skip Dice Loading** — Off by default. When enabled, DNDolphins skips its rolling animation and immediately shows the already-resolved dice result for the general Dice Roller, weapon attacks/damage, spell-combat rolls and Hit Dice. Companion FAPs read the same preference. Adventure skill rolls and Initiative rolls already resolve directly without an animation, so enabling this setting keeps their existing immediate-result behavior.
2. **Debug** — Off by default. Enables additional diagnostic logging at selected shared/settings/restore and roll paths. It does not change dice math, character rules or persisted character data.
3. **Get Elevated** — **420** by default. This controls only the randomized starting 420 bundle. Changing Off to 420 adds one random bundle to the active character if an Inventory already exists; otherwise the bundle waits for normal starting-equipment generation. Turning it Off does not remove already-owned items and does not control catalog visibility.
4. **Catalog** — **SRD** by default. Selects the installed rules catalog used by Character, grant, Inventory and Spellbook loaders. Catalog assets are streamed on demand rather than loaded wholesale into memory.
5. **Homebrew** — **Yes** by default. Controls visibility of rows sourced as `Homebrew` or `DNDolphins` without changing already-owned character data. The normal Item catalog retains these project rows so the toggle works independently of the rules catalog.
6. **Menu Type** — **Text** by default. Toggle between **Text** and **Graphical**. Graphical Home uses a native 4×2 icon grid with directional navigation; the selected menu name is shown in the header. All 15 Home entries use their supplied native 25×25 monochrome assets: Characters, Character, Vitals, Abilities & Saves, Skills, Features & Perks, Inventory, Magic & Spells, Bestiary, Initiative, Combat, Dice Roller, Adventure, Journal and Settings. Each menu entry has a stable per-entry filename under `dndolphins_images/`, so artwork can be replaced independently later without changing C code; replacements must remain native **25×25, 1-bit monochrome**.

### Fresh-character defaults and automatic setup

A new character starts as **New Hero**, Human, Adventurer, True Neutral, Fighter 1 with Milestone leveling, 10 HP, AC 10 and the standard ability array assigned as STR 15, DEX 14, CON 13, INT 12, WIS 10 and CHA 8. Languages and class saving-throw proficiencies are deliberately not written as hidden defaults: **Grant Initial Traits** stages Common, the starting language choices and the primary-class save pair alongside the other level-1 grants for review before they are applied. Adding another class does not silently grant another save pair. Manual save adjustments remain available in Abilities & Saves. Five editable attack templates are also created: Unarmed Strike, Grapple, Shove, Spell Attack and Saving Throw Action.

Choosing a bundled class automatically applies that class's normal Hit Die and spellcasting classification, and choosing a subclass defaults to choices compatible with the selected class. In the subclass catalog, **Hold OK** toggles the normal class-filtered list and **All** subclasses; Hold OK on the Class or Subclass field itself opens custom text entry. When an older/imported character contains a recognized class but its spellcasting setup or normal slot pool is unset, opening DNDolphins can fill the missing class-derived spellcasting data instead of requiring the user to rebuild it manually.

### Character options

The Character screen is presented in this order:

1. **Name** — OK edits the character name.
2. **Player** — OK edits the player name.
Catalog-backed Class, Subclass, Species, Background, Feat and Spell choices stream their installed assets at load time. Grant metadata is also streamed rather than materialized in RAM. Existing character-owned values and already-applied grants remain valid if catalog settings change. If a picker asset is missing, DNDolphins uses a small SRD fallback list for the core Class/Subclass/Species/Background/Feat choices.

3. **Species** — OK opens the bundled Species catalog; Hold OK enters a custom Species name.
4. **Background** — OK opens the bundled Background catalog; Hold OK enters a custom Background name.
5. **Alignment** — OK opens the bundled Alignment catalog; Hold OK enters a custom Alignment value.
6. **Classes** — Opens the multiclass list. Class records store class name, level, subclass, Hit Die and class-specific progression data.
7. **Total Level / Proficiency Bonus** — Displays the calculated total character level and proficiency bonus.
8. **XP** — OK adds 100 XP, Left/Right changes XP by 100, and Hold OK opens full numeric entry.
9. **Leveling** — Toggles XP or Milestone leveling.
10. **Languages** — Opens the character language list.
11. **Proficiencies** — Opens owned weapon, armor and tool training, with a catalog for adding or replacing entries.
12. **Inspiration** — Toggles Inspiration.
13. **Level Choices** — Opens the next pending ASI/Feat choice. ASI +2 and ASI +1/+1 apply the ability change directly; progression feat choices open the Feat catalog in **Allowed** mode.
14. **Grant Initial Traits** — Saves the active character, unloads DNDolphins and launches DNDGrants in initial-traits mode. DNDGrants stages missing starting/level-1 species, background, primary-class, subclass, language, save, feat/proficiency and spell grants into **Review grants before apply**. Nothing is committed merely because the action was opened.
15. **Apply Level Grants** — Saves the active character, unloads DNDolphins and launches DNDGrants in level-grants mode. DNDGrants stages missing grants through the character's current levels. Increasing a class level does not silently apply grants; its post-level review now offers **Hold OK: Apply Level Grants** for a direct handoff. Each review batch must be approved or have its required choices resolved before progression continues to the next bounded batch.

When a class level increases, HP grows by the class's fixed-average Hit Die value plus Constitution modifier, with a minimum gain of 1 HP per level. Class and global Hit Dice current values are set to their new maximums, while ASI/Feat and other player choices remain explicit. The XP value is raised to at least the minimum threshold for the resulting total level. On the resulting **Level Review**, Short OK continues to the pending ASI/Feat choice (or back to the class record when none is pending), while **Hold OK** saves/unloads DNDolphins and opens DNDGrants for the newly eligible deterministic grants.

### Languages and Proficiencies

Languages begins with **+ New Language**. Short OK opens the streamed language catalog; Left/Right moves through catalog pages and OK adds the selected language. Open an owned language to replace it from the catalog or choose **Delete language**. Starting and feature-granted language choices appear as reviewable `Freepick` Language grants; restrictions such as **Standard language** are enforced by the choice catalog instead of silently selecting a language.

Proficiencies begins with **+ New Proficiency**. The streamed catalog covers weapons, armor and tools and defaults to **Allowed**, using class training/choice eligibility and already-owned grants. **Hold OK** switches between Allowed and All for table rulings and homebrew. Open an owned proficiency to replace its type/name from the catalog or choose **Delete proficiency**. Granted training is staged through the same approval flow; a choice grant opens only the applicable options rather than granting every eligible tool/weapon/armor. An Item's own Proficient switch continues to control that Item's attack modifier.

Both owned lists remain unbounded by the resident page size. Up/Down traverses entries and Short Left/Right moves by **four-record owned-list pages**, with `PgX<>` in the header when needed. The Language/Proficiency page buffer is allocated only while one of those lists is open and released on exit. Their catalogs use **eight-row streamed pages**, so smaller expected libraries consume less RAM without imposing an eight-entry total limit. Duplicate additions are ignored. Existing legacy language/training text is not imported; select entries in the new lists.

### Classes and class editor

The Classes list begins with **+ Add New**, followed by each class in the multiclass build. Short OK opens a class record; Left/Right adjusts the highlighted value, Short OK performs the field's normal cycle/adjust action, and Hold OK opens full numeric entry or custom text where supported.

Class options are presented in this order:

1. **Name** — Short OK opens the bundled Class catalog; Hold OK edits a custom class name. Choosing a bundled class also configures its normal Hit Die and spellcasting mode.
2. **Subclass** — Short OK opens subclasses compatible with the selected class; Hold OK edits a custom subclass. Inside the catalog, Hold OK toggles **Class filter / Showing all**.
3. **Class level** — Changes this class's level while respecting total character level 20. Raising it applies fixed-average HP, refreshes Hit Dice, updates spell progression and opens the level-up review. Short OK keeps the existing choice flow; **Hold OK** launches **Apply Level Grants** directly. Deterministic Features/spells are still never silently applied.
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

**Grant Initial Traits** and **Apply Level Grants** always stage newly eligible grants into **Review grants before apply** before changing the character. Rows begin with `?`, successfully applied rows show `A`, and skipped rows show `S`. Choice-bearing rows identify their choice type before opening the applicable catalog. Review batches are capped at 24 resident grants and resume the metadata file from a saved byte cursor, so a high-level character does not repeatedly rescan the catalog from byte zero.

- **Apply all pending** applies only deterministic pending rows. Choice rows remain pending and the status reports how many choices still need player input.
- **Grant row / Short OK** applies a deterministic grant; for Language, Spell, Feat/Perk, Skill, Skill/Tool, Proficiency or Size choices it opens the appropriate choice catalog instead. Selecting a Feat/Perk can stage grants owned by that newly acquired feature as a later review batch.
- **Spell choices** are restricted by the class/feature that granted them. Level-based `Freepick Spell` rows use the highest spell level available at the class level that earned the grant; school/list/explicit-option restrictions are applied when metadata supplies them.
- **Grant row / Hold Left** marks a pending grant Skipped.
- **Grant row / Hold OK** opens the full grant record editor.
- **+ Add Custom Grant** creates an editable custom grant record for advanced/manual progression handling.

Grant scanning is an explicit input/deferred-event operation. It is never run from the periodic UI tick or Canvas draw callbacks. Feature/Feat dependency discovery is consolidated into bounded forward passes and has an eight-generation safety ceiling to prevent malformed dependency chains from looping indefinitely.

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

The Features list begins with **+ Add New**, followed by character-owned class features, feats, perks and limited-use resources. It grows beyond 20 Features; `Features` and `PgX<>` identify the current page. On the list, Short Left/Right moves by eight-record pages. Short OK opens a Feature; Left/Right adjusts the highlighted field; Hold OK opens full numeric entry or custom text where supported.

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

### Magic & Spells ownership

The DNDolphins Home **Magic & Spells** option is now a thin launch handoff: DNDolphins saves, tears down and opens DNDSpellbook directly on its Magic view. This keeps spell-management UI and slot editing out of the normal DNDolphins runtime. Class level/casting-mode progression remains character-owned in DNDolphins, and active Short Rest / Arcane Recovery spending remains Combat-owned in DNDCombat.

Inside DNDSpellbook, the Magic view is presented in this order:

1. **Open Spellbook** — Switches to the normal Spellbook list.
2. **Casting ability** — Left/Right cycles the ability used for spell attacks and save DCs.
3. **PB / Attack / DC summary** — Displays proficiency bonus, calculated Spell Attack and Spell Save DC; Hold OK recalculates class-derived shared multiclass slot maximums.
4. **Spell attack misc** — Left/Right changes the modifier; Hold OK opens numeric entry.
5. **Spell save misc** — Left/Right changes the modifier; Hold OK opens numeric entry.
6. **Known / knowable / free-granted summary** — Uses the streamed Spellbook rather than keeping a second resident spell collection.
7. **Slots help row** — Documents that Short Left/Right changes available slots and Hold Left/Right changes maximum slots on the level rows below.
8. **Arcane Recovery status** — Shows no Wizard / Ready / Used. Short OK directs a ready Wizard to use **Short Rest** in DNDCombat; recovery budget spending remains there so Combat behavior is not duplicated or removed.
9. **Level 1 slots**
10. **Level 2 slots**
11. **Level 3 slots**
12. **Level 4 slots**
13. **Level 5 slots**
14. **Level 6 slots**
15. **Level 7 slots**
16. **Level 8 slots**
17. **Level 9 slots** — On each slot row, Short Left/Right changes current slots, Short OK opens current-slot numeric entry, Hold Left/Right changes the maximum, and Hold OK opens maximum-slot numeric entry.

Normal DNDSpellbook launches still open directly on the Spellbook list. **Magic & Spells** is shown as the final row after the last owned spell, matching Inventory's end-of-list utility actions; short OK opens it without leaving DNDSpellbook. Pact Magic and other class-specific casting resources remain represented by their existing class/Combat rules rather than being duplicated into this shared-slot view.

## DNDGrants — progression grant review

DNDGrants is launched only by **Grant Initial Traits** or **Apply Level Grants**. DNDolphins saves and tears down before DNDGrants starts, so the large grant review batch and grant-processing workflow do not share the DNDolphins runtime heap. DNDGrants reads the same persisted active character and shared Catalog/Homebrew settings, checks `_All` availability on its own launch only when **Catalog: All** is selected, and returns to DNDolphins when review is complete or Short Back is used. Hold Back exits to firmware.

Grant review retains the existing bounded behavior: at most 24 grant records are resident in one review batch and additional eligible grants continue from the saved metadata cursor. The 24-record batch limit is a grant-memory bound, not a Combat attack limit.

## DNDCombat — standalone combat

DNDCombat is a separate FAP. DNDolphins saves and tears down before launching it, so Combat executable code and its active attack/cast working set are not resident in DNDolphins. Short Back from DNDCombat returns to DNDolphins with **Combat** focused; Hold Back exits to firmware.

### Combat options

Combat is presented in this order:

1. **Attack mode** — Cycles Normal, Advantage and Disadvantage for attack rolls.
2. **Weapon Attacks** — Lists usable Inventory weapons and performs attack/damage resolution. Ammunition can come from a weapon's own counter or matching Inventory stacks; loose ammunition matches the required token anywhere in the Item name, so names such as `Fire Arrow` can satisfy an `arrow` requirement.
3. **Spell Attacks** — Lists currently castable tracked spells that make an attack roll. Each row shows the spell-specific casting ability and attack modifier after the name, for example `(WIS/+5)`, while preserving streamed paging beyond 24 records.
4. **Spellcasting stats** — Shows the current Spellcasting Ability abbreviation, Spell Attack Bonus and Spell Save DC on one row. Short OK opens **Magic & Spells** for the full casting controls.
5. **Combat Utility Spells** — Lists currently castable spells that do **not** make an attack roll, including save-based, healing, buff/control and other utility casts; attack-roll spells remain in Spell Attacks so the lists do not duplicate one another.
6. **Rituals** — Lists eligible known ritual spells that can be cast through the ritual path without consuming a slot when the character has the applicable ritual capability.
7. **Attack Templates** — Opens with three separate persisted templates: **Unarmed Strike**, **Grapple**, and **Shove**, followed by Spell Attack, Saving Throw and any Custom templates. Unarmed Strike rolls `d20 + ability modifier + Proficiency Bonus` (plus Attack misc and exhaustion effects) and reports hit damage as `1 + ability modifier`. Grapple and Shove are independent editable templates and show the target-choice STR/DEX save against `8 + ability modifier + Proficiency Bonus`; they do not share or trigger the Unarmed Strike attack roll. Hold OK on any saved template opens its full editor. Older characters receive missing Grapple/Shove templates without overwriting existing templates.
8. **Jump to Initiative** — Saves/tears down DNDCombat and launches DNDInitiative. Short Back from that Initiative session returns to DNDCombat; Initiative launched from anywhere else returns to DNDolphins.
9. **HP** — Adjusts current HP; Hold OK opens full numeric entry.
10. **Temporary HP** — Adjusts temporary HP; Hold OK opens full numeric entry.
11. **Short Rest** — Applies supported Short Rest recovery and enables applicable recovery choices such as Arcane Recovery.
12. **Spend Hit Die** — Chooses a class Hit Die and rolls healing while reducing that class's remaining Hit Dice.
13. **Long Rest** — Applies supported Long Rest recovery for HP, Hit Dice, spell resources, free casts and Feature recharge cadences. Reaction state remains under the separate Reaction control.
14. **Conditions** — Edits character conditions.
15. **Concentration** — Edits the currently concentrated-on effect/spell name.
16. **Reaction** — Toggles the reaction Ready/Used state.
17. **Temp effects** — Edits temporary effects.
18. **Resistances** — Edits resistance text.
19. **Immunities** — Edits immunity text.
20. **Vulnerabilities** — Edits vulnerability text.
21. **Senses** — Edits senses.
22. **Movement** — Edits movement modes.
23. **Death success** — Adjusts death-save successes; Hold OK opens numeric entry.
24. **Death failure** — Adjusts death-save failures; Hold OK opens numeric entry.
25. **Exhaustion** — Adjusts Exhaustion; Hold OK opens numeric entry.

Weapon and spell attack lists are streamed in bounded eight-record windows and use a separate total count; they are not limited to the 26 Combat menu actions. Host regression coverage currently exercises 300 weapon records and 300 spell records, including the final record.

Weapon combat uses STR/DEX/finesse rules, proficiency, magic bonuses, versatile damage, extra dice, riders and critical dice doubling. For weapons with the Ammunition property, a weapon-local Ammo Current/Maximum counter is used first when configured; otherwise Combat consumes one quantity from the first non-weapon Inventory stack whose name or Ammo Group contains the required ammunition token, case-insensitively. Standard families normalize to `arrow`, `bolt`, `bullet` or `needle`, so **Fire Arrow**, **Silvered Arrows**, **Crossbow Bolt Bundle** and similar descriptive names can work without being named exactly `Arrows` or `Bolts`. Older/custom bows, crossbows, slings, blowguns, muskets and pistols can infer the standard ammunition family from the weapon name when Ammo Group is empty.

The Magic screen displays **Known / knowable / free-granted** totals. Known is the number of owned Known spells. Knowable combines each class's current cantrip/prepared-or-spellbook capacity with genuinely additional granted spells. The free-granted value excludes ordinary class-capacity learning (for example normal Wizard spellbook picks) so bonus species/feat/feature/subclass spells do not silently distort the class allowance. The aggregate is refreshed when Magic is entered; drawing the screen does not read the Spellbook file.

Spell combat separates attack-roll spells from Combat Utility Spells and shows the resolved source-class casting ability/modifier on each non-ritual spell row. It supports cantrip scaling, higher-level casting, multiple attack/roll instances and supported secondary effects. Short Rest restores Short/Long Features and enables Arcane Recovery; Long Rest restores HP, spell slots, Pact slots, spell points, free casts and applicable Features, clears temporary HP/death-save marks and reduces Exhaustion by one. Reaction Ready/Used state is edited separately.

### Dice Roller controls

The Dice Roller contains Dice Count, Die, Modifier, Mode and the Roll action/result row. Left/Right adjusts the selected setup value, OK rolls when the Roll row is selected, and Hold OK on Dice Count, Die or Modifier opens full numeric entry. Modes include Normal, Advantage, Disadvantage and Guidance. Advantage, Disadvantage and Guidance are d20 conveniences: Guidance automatically adds a rolled d4 to a 1d20 roll, while Advantage/Disadvantage rolls two d20s and keeps the appropriate result. Changing the dice setup away from the supported d20 form returns the roller to Normal mode. With **Settings → Skip Dice Loading** enabled, the roll is still resolved normally but its rolling animation is omitted and the result is shown immediately.


## DNDCharacter Sheet — graphical active-character sheet

**DNDolphins → Character → Character Sheet** runs the read-only Character Sheet FAL inside the Hub. The standalone **DNDCharacter Sheet** FAP remains a thin wrapper for direct launch. It follows the exact persisted active character and presents the familiar tabletop character-sheet information as native 128×64 monochrome pages rather than a scrolling text editor.

Use **Left/Up** and **Right/Down** to move through ten graphical pages. The pages cover the character header and combat summary; all six ability scores and modifiers; all six saving throws plus Proficiency Bonus and passive scores; all eighteen skills across two pages; Armor Class, Initiative, Speed, HP, Temporary HP, Hit Dice and Death Saves; identity/class fields; spellcasting ability, attack modifier, save DC and spell-slot usage; conditions/defenses; and senses/movement. Proficiency markers are shown beside proficient saves and skills. Short Back returns to DNDolphins with **Character** focused; Hold Back exits to firmware.

The layout deliberately borrows the recognizable information hierarchy of a tabletop 5e character sheet—identity first, boxed abilities and combat values, then saves/skills and supporting sections—while using original Flipper-native box geometry and typography rather than reproducing proprietary paper-sheet artwork.

## DNDBackup & Restore — character backup, restore and validation

DNDBackup & Restore is the standalone owner of user-facing character backup and restore operations. It works on the persisted active character after DNDolphins has saved and torn down, so external SHD bundle I/O does not share the hub's runtime state.

The main actions are:

1. **Create Backup** — Writes a coherent SHD bundle for the active character to the configured `/ext` backup folder. Character data and supported companion sidecars are copied with checked read/write completion so a failed SD read cannot be accepted as a successful truncated backup.
2. **Restore Backup** — Opens the native Flipper file browser for `.shd` selection, validates the selected core SHD and matching companions, stages the restore, and publishes it transactionally. A failed restore rolls back instead of leaving mixed live data.
3. **Backup Folder** — Selects the external destination under `/ext`. The folder setting is itself published transactionally, and nested backup directories can be created as needed.
4. **Clone Active Character** — Allocates the next local profile ID and duplicates the active character plus supported character-owned sidecars and Inventory bags. The clone does not automatically become active.
5. **Validate Character** — Performs read-only structural and semantic validation of the active character, including class levels, ability-score bounds and HP sanity.
6. **Profile ID** — Displays the active profile ID for reference; it is informational rather than a selectable no-op.

DNDolphins does not contain a second backup/restore implementation. Its **Characters → Backup & Restore** action only launches this FAP. Short Back returns to DNDolphins; Hold Back exits to firmware.

## DNDInventory — inventory, equipment and currency

DNDInventory opens the persisted active character directly to the Inventory list. Opening Inventory never writes starting equipment automatically. When an initial package is still available, **Grant Initial Inventory** opens the explicit review screen before anything is written.

### Inventory list

The list begins with **Bag: Main <>**, then **+ Add New**, **Currency**, and the Items stored in the selected bag. **Inventory Resources**, **Grant Initial Inventory**, and **Bag Mover** are placed after the final Item so these actions remain visible in the normal list.

- **Bag: <name> <>** — Left/Right cycles bags without loading every bag into RAM. **Main** and **Group** always exist as logical choices. Hold OK opens **Manage Bags**, where custom bags can be added or removed; Main and Group cannot be removed.
- **+ Add New** — Short OK or Hold OK creates a blank Item in the currently selected bag and opens the Item Editor.
- **Currency** — Short OK opens CP, SP, EP, GP and PP. Currency remains character-global and is stored with the Main bag. Left/Right changes the selected denomination by one; OK opens full numeric entry.
- **Item row / Short OK** — Opens that Item in the Item Editor.
- **Item row / Hold OK** — Toggles Equipped and saves immediately.
- **Short Left / Right** — Moves to the previous/next eight-record Inventory page.
- **Hold Left / Right on an Item** — Decreases/increases Stack Qty by five, clamped to 0–999.
- **Inventory Resources** — Short OK opens derived carrying/equipment information and actions.
- **Grant Initial Inventory** — Short OK opens **Review inventory grant** for the normal class/background starting package; OK on that review screen performs the transaction and Back cancels. Hold OK opens the same review screen for the one-time explicit regrant override when available. Opening an empty Inventory never silently applies the package.
- **Bag Mover** — Final Inventory-list action. Short OK opens the current bag as a multi-select list and toggles `[X]` on Item rows. **Hold OK anywhere in Bag Mover** opens the destination list for checked Items; the terminal **Move Selected** row also works. The footer shows `OK: select  Hold OK: move`. Back from destinations cancels while keeping the selection. Moving publishes source and destination together and repairs container indexes. Hold OK on normal Inventory Item rows remains Equip/Unequip.
- **Item Editor → Container: <bag>** — Short OK or Right cycles to the next bag; Left cycles backwards. Selecting **Container: Group** immediately moves that one Item to Group and continues editing it there, with no extra screen. Main and custom bags work the same way. Quantity, equipment flags and all other Item fields are preserved; held-direction Repeat does not perform further moves.

When the selected bag exceeds one eight-record page, the header shows **PgX<>** at the upper right. The indicator follows that bag's currently resident page; temporary save/status messages take its place while they are active. Carrying, equipped-weight, attunement and related **Inventory Resources** calculations stream across **all bags** for the character rather than only the visible bag.

Main keeps the compatible `inventory_<id>.txt` path. Group uses `invGroup_<id>.txt`; additional named bags use `inv<BagName>_<id>.txt` with a safe filename token and a `BagName=` metadata line preserving the display name. Profile duplicate, archive, export/import and SHD history preserve every bag separately rather than flattening Inventory.

### Get Elevated bundle and Homebrew catalog

Inventory grows beyond 24 items while grants, rewards, editing and Combat lookups remain available on later pages. With **Get Elevated: 420**, normal starting-equipment generation also adds one random bundle:

| d6 | Accessory | Additional items |
|---|---|---|
| 1 | Old Pipe & Lighter | Two different Premium Flower strains |
| 2 | Small Bong & Lighter | Two different Premium Flower strains |
| 3 | One Hitter & Lighter | Two different Premium Flower strains |
| 4 | Gandalf Pipe & Lighter | Two different Premium Flower strains |
| 5 | Puffco Peak | One Live Rosin strain |
| 6 | Blue Dream Vape Pen | None |

Each flower stack contains **5–37** one-gram units; the rosin stack contains **2–9**. Strains are **Blue Dream, Girl Scout Cookies, Wedding Cake, Sour Diesel, Pineapple Express, Lemon Cherry Gelato**. If the second flower roll repeats the first, it advances to the next strain and wraps at the end of this list. Stack quantities roll independently.

The **420** catalog filter appears after Magic and contains the six accessories, six **Premium Flower (1 gram)** entries and six **Live Rosin (1 gram)** entries. These rows are present in both physical Item catalog modes and are visible only when **Homebrew: Yes**. **Get Elevated** does not hide or reveal catalog rows; it only controls automatic/randomized bundle granting. Owned items remain visible regardless of either setting. The bundle is committed as a unit, so a failed storage operation cannot leave only part of the bundle in Inventory.

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
3. **Stack Qty** — Sets the owned quantity; the Inventory list also supports Hold Left/Right for quick ±5 changes.
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

The Item catalog supports All, Weapons, Armor, Ammunition, Gear, Tools, Instruments, Trinkets, Mounts/Vehicles, Potions, Rings, Rods, Scrolls, Staffs, Wands, Wondrous and Magic filters. The normal catalog contains **615 rows**: 579 SRD rows plus the same 18 `Homebrew` and 18 `DNDolphins` project rows used by the independent Homebrew toggle. **Homebrew: No** yields the 579-row SRD view without affecting already-owned Inventory records. Catalog rows display compact Source tags such as `[Core]`, `[DND]` or `[HB]`. **Hold OK** opens the filter-category picker; Up/Down chooses a filter and OK applies it. Short Left/Right changes catalog pages and Short OK applies the selected catalog entry. Musical Instrument has its own category with the SRD instrument variants, and the catalog includes the d100 SRD Trinkets under their own category. **Tea Set** is included as generic Mundane Gear as a DNDolphins convenience and is not represented as an SRD equipment-table title. Generic Spell Scroll entries cover Cantrip and Levels 1–9 with level-appropriate rarity.

Choosing a recognized bundled weapon or armor also fills its useful mechanical preset—weight, damage, weapon properties, Versatile die, ammunition family, armor base/DEX cap or shield bonus—so it can be used by Combat and Formula AC without manually rebuilding standard equipment statistics. **Normalize coin values** converts the current CP/SP/EP/GP/PP mix into larger denominations while preserving the same total copper-piece value.

## DNDSpellbook — spells, preparation and catalog

DNDSpellbook normally opens the persisted active character directly to the Spellbook list. A launch from DNDolphins **Magic & Spells** opens the same app directly on its Magic view instead. Owned spells are stored in level-ascending, case-insensitive name order.

### Spellbook list

The list begins with **+ Add New**, followed by owned Spells. There is no 24-spell limit or manual spells-known allowance restriction. The header shows `Spellbook` and `PgX<>` when more than one page exists, alongside the active character ID.

- **+ Add New** — Short OK or Hold OK creates a blank Spell, marks it Known, and opens the Spell Editor.
- **Spell row / Short OK** — Opens that Spell in the Spell Editor.
- **Spell row / Hold OK** — Toggles Prepared for a Known spell and saves immediately. Always Prepared spells stay prepared; unknown spells are not quick-prepared.
- **Short Left / Right** — Moves to the previous/next eight-record Spellbook page.
- **Hold Up** — Opens **Spell Filters**.
- **Magic & Spells** — Final Spellbook row after the last spell; short OK opens casting statistics and spell-slot management.

List status marks are `A` for Always Prepared, `P` for Prepared, `K` for Known and `-` for neither; `F` also appears while a free cast remains. Each owned spell row also shows a right-aligned three-character source tag: the spell's class when class-sourced (for example `WIZ`), or the actual grant/origin/feat source name when granted (for example `HIG` for High Elf or `MAG` for Magic Initiate).

### Spell Filters

Filters are presented in this order:

1. **Level** — Any, Cantrip or Levels 1–9.
2. **Class** — **Character Classes** is the default and represents the union of the character's actual spell lists. **Any Class** plus the bundled SRD class filters are available for catalog browsing even when the character does not own the selected class.
3. **Ritual** — Any or Ritual Only.
4. **School** — Any or a specific spell school.
5. **Source** — Filters by catalog source. The bundled SRD selector provides **Any / Core**.
6. **Status** — Any, Prepared, Known or Always Prepared.
7. **Eligibility** — **Allowed** enforces the character's actual spell-list access and permitted spell level; **All Spells** treats the selected class as a catalog-membership filter without requiring character eligibility. **Any Class + All Spells** exposes the complete bundled catalog.

Left/Right changes the selected filter. OK returns to the list/catalog and reapplies the filters.

### Spell Catalog

The bundled Spell catalog contains **355 SRD spells**, sorted by spell level and then name. Homebrew is an independent source gate for project-owned spell rows. Short Left/Right changes catalog pages, Short OK selects the highlighted Spell, and Hold OK opens Spell Filters without leaving the catalog workflow.

### Spell Editor

Spell options are presented in this order. Left/Right or Short OK performs the normal toggle/cycle/increment action; Hold OK opens full numeric entry on supported numeric fields.

1. **Name** — Short OK opens the Spell catalog; Hold OK edits a custom Spell name.
2. **Notes** — Edits free-form Spell notes.
3. **Source class** — Selects which owned class supplies the Spell's casting context. Catalog selection automatically resolves a compatible owned class when possible.
4. **Level** — Sets Cantrip/0 through Level 9.
5. **Known** — Marks whether the character knows the Spell. Choosing a Spell from the catalog marks it Known automatically.
6. **Prepared** — Marks the Spell Prepared.
7. **Always prepared** — Keeps the Spell prepared regardless of quick-prepare toggles.
8. **Ritual** — Marks ritual capability for the Rituals combat path. This tag does not change Known.
9. **Free casts** — Sets remaining no-slot casts.
10. **Free casts max** — Sets the maximum free-cast pool restored by the supported recovery logic.
11. **Use one free cast** — Consumes one remaining free cast without spending a spell slot.
12. **Stable ID** — Stores the stable catalog/progression identifier used to avoid duplicate deterministic grants.
13. **Source** — Stores the Spell's catalog source/provenance.
14. **School** — Stores the spell school and supports School filtering.
15. **Grant source** — Shows/edits the progression source label when the Spell came from a deterministic grant.
16. **Grant type** — Stores the grant-source type used by progression bookkeeping.
17. **Delete spell** — Removes the Spell.

Choosing a catalog Spell copies its level, ritual, school, source and a stable ID in addition to the name. Owned Spellbook records are automatically kept in level-ascending, case-insensitive name order after additions or edits.

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

1. **Browse Monsters** — Opens the streamed monster catalog using the current Search, Max CR, Type, Source, Browse Environment and Browse Role filters. In the list, Up/Down selects a monster, Left/Right changes the catalog window/page, Short OK opens the full stat block, and **Hold OK adds that catalog entry directly to Custom Encounter**.
2. **Generate Encounter** — Generates an encounter from Party Level, Party Size, Difficulty, Encounter Environment, Encounter Role, Repeat Types and Template. In an encounter, OK opens a monster/stat action, Simulator, Save Name, Warnings or Send to Initiative as selected; Hold OK regenerates with the current settings.
3. **Custom Encounter** — Opens an empty/resumable custom encounter. **+ Add Monster** opens the Monster Catalog; Hold OK on a catalog row adds it directly, and monster details also expose **Add to Custom Encounter**. The custom encounter provides **Difficulty Simulator**, **Save Encounter**, composition warnings and **Add to Initiative** using the same bounded encounter model as generated encounters.
4. **Party Level** — Left/Right selects levels 1–20 for encounter generation and simulation.
5. **Party Size** — Left/Right selects party size 1–12.
6. **Difficulty** — Left/Right cycles Low, Moderate and High encounter targets.
7. **Encounter Env** — Left/Right cycles the preferred encounter environment.
8. **Encounter Role** — Left/Right cycles the preferred monster role.
9. **Repeat Types** — Left/Right toggles whether the generator may repeat monster types.
10. **Template** — Left/Right cycles Balanced, Horde and Elite generation templates.
11. **Saved Encounters** — Opens named saved encounters. Short OK resumes one; Hold OK opens its actions: Resume, Send to Initiative, Rename, Duplicate, Archive and Delete.
12. **Search** — OK opens monster-name text search.
13. **Max CR** — Left/Right cycles the maximum CR filter.
14. **Type** — Left/Right cycles creature type.
15. **Source** — Left/Right cycles monster source, including **Homebrew** for monsters supplied by the custom monster pack when Settings → Homebrew is On.
16. **Browse Env** — Left/Right cycles browse environment.
17. **Browse Role** — Left/Right cycles browse role.
18. **Saved Filters** — Opens saved browse-filter presets. Short OK applies a preset; Hold OK deletes the selected preset. The final list action creates a new preset from the current browse filters.
19. **Favorite Monsters** — Opens the saved Favorites monster list.
20. **Recent Monsters** — Opens recently viewed monsters.
21. **Create Custom Monster** — Opens the custom-monster editor for a new record.
22. **Pack Diagnostics** — Shown **only when DNDolphins Settings → Debug is On**. Opens pack status/recovery information and reruns pack diagnostics with OK.


DNDBestiary loads only the shared **Debug** and **Homebrew** settings at startup through a two-byte settings projection rather than carrying the complete DNDolphins settings model. With **Homebrew: No**, custom-pack monsters are excluded from Source: Any browsing, the Homebrew Source choice, Favorites/Recents, generated encounters, Custom Encounter additions, and saved-encounter resume/Initiative handoff; user-created `Custom` monsters remain available. Party Level and Party Size are persisted between Bestiary sessions. Opening a monster automatically adds it to **Recent Monsters**, while Favorites, Saved Filters and Saved Encounters persist until the user changes them. Search is case-insensitive substring matching, so a partial piece of a monster name is enough.

On the six browse-filter rows (**Search**, **Max CR**, **Type**, **Source**, **Browse Env** and **Browse Role**), **Left** resets that filter to its default/Any state immediately; **Right** advances through the available values where applicable.

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
- Inventory Main bag: `inventory_<id>.txt`
- Inventory Group bag: `invGroup_<id>.txt`
- Inventory named bags: `inv<BagName>_<id>.txt`
- Spellbook: `spellbook_<id>.txt`
- Features: `feats_<id>.txt`
- Applied deterministic grants: `appliedgrants_<id>.txt`
- Shared settings: `settings.txt`
- Level history: `ch_<id>_<safeName>_<level>.shd` core snapshots plus matching `_items.shd`, `_spellbook.shd`, `_features.shd` and `_appliedgrants.shd` sidecar snapshots. Current complete bundles also carry an internal `_bundle.shd` marker so Restore can distinguish a truly empty historical collection from an older core-only SHD that never captured sidecars.
- Journal: `/ext/apps_data/dndjournal/`
- Initiative: `/ext/apps_data/dndinitiative/`
- Adventure: `/ext/apps_data/dndadventure/`
- Bestiary/custom monsters/packs/encounters: `/ext/apps_data/dndbestiary/`

`/ext/apps_data/dndolphins/custom_active_profile.txt` stores the active character as `Active=<id>`. Inventory and Spellbook sidecars remain under the DNDolphins character root so Combat, progression, Adventure and Journal share the same character-owned records. Live `.txt` character/sidecar files remain authoritative during normal play; SHD files are level-history snapshots used only by the explicit restore workflow owned by **DNDBackup & Restore**.

## Data and memory behavior

Character/profile loading is best-effort by recognized field name. Collection and catalog readers use bounded pages/windows. Separate Inventory, Spellbook, Adventure, Initiative, Bestiary, Combat and Grants FAP launches release the Hub. Integrated Character Sheet and Journal instead keep the Hub resident and unload their FALs on return; standalone wrappers remain available.

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
fbt fap_dndolphins fap_dndcharactersheet fap_dndcombat fap_dndgrants fap_dndinventory fap_dndspellbook fap_dndadventure fap_dndjournal fap_dndinitiative fap_dndbestiary fap_dndbackup fap_dnd_character_sheet fap_dnd_journal fap_dnd_monster_turn fap_dnd_spell_damage fap_dnd_loading
```

The suite release label is kept separately from numeric `(4, 20)` FAP metadata. Build with the firmware tree/SDK matching your device and install all eight FAL destinations in [FAL_INTEGRATION.md](FAL_INTEGRATION.md), including loading API version 2 under DNDolphins. This release needs no firmware overlay or firmware flash. `tests/sdk/VALIDATION.md` retains historical official-SDK evidence; it is not a current 4.20.3 build certificate. Current host evidence is in `tests/host/VALIDATION.md`; device checks are in `DEVICE_TEST_MATRIX.md`.

### Abilities & Saves Quick Rolls

In **Abilities & Saves**, Left/Right visibly selects **Check** or **Save** for the highlighted ability. Short OK rolls the selected type through the existing d20 roller. Hold OK edits the ability score when Check is selected, or toggles that ability's saving-throw proficiency when Save is selected. Skills use short OK to roll the displayed skill modifier.

### Inventory and Spellbook Search

Inventory and Spellbook support case-insensitive partial-name search. Search requires at least 3 characters; shorter entries are rejected without running a scan. Results preserve the app's existing order. Spellbook catalog search is applied together with the existing filters.

### Favorite Spells and Backup / Restore

DNDSpellbook owns Favorite state for each Spell. DNDCombat exposes a **Favorite Spells** list and reuses the same spell-casting/resource resolution used by the normal combat spell lists.

**DNDBackup & Restore** is the exclusive user-facing SHD backup/restore owner. From the active character's actions, DNDolphins launches the companion FAP; it does not export, import, or restore character backups itself. The backup FAP can save the current SHD bundle to a configured `/ext/...` folder and browse the configured folder for valid core SHD backups belonging to the active profile. External bundle files are staged transactionally before the existing SHD restore engine updates live character data. The native Flipper file browser is filtered to `.shd` and uses the packaged 10×10 1-bit sword icon at `dndbackup_images/shd_sword_10x10.png`.

### Initiative Monster Turn Tools

While editing a participant in active DNDInitiative combat, **Monster Turn Tools** loads the focused FAL for non-party participants and resolves their name against allowed Bestiary sources. The stat block's tools list common parsed attacks; OK rolls attack and damage. Back returns to the same member, round and turn without launching the full Bestiary FAP. Bestiary's own stat-block Tools row uses that same FAL with its borrowed detail record, returning to the same stat block and scroll position.

### Initiative Combat History

DNDInitiative's main menu includes **Combat History**. It reads the existing `history/` records created by **End + Save History**, keeps only a bounded list of the newest records for the active character, and displays their saved end time, round count, party state, and surviving opponents.

### Journal Session Logs and Search

DNDJournal opens to a lightweight menu with **Journal Entries**, **Search Journal**, and **New Session Log**. A Session Log is a normal Adventure-category Journal entry, prefilled with editable sections for Party State, Milestones, NPCs / Monsters, Loot / Rewards, and Notes; no parallel session-log storage is created.

Journal search accepts **3 or more characters** only. Matching is ASCII case-insensitive and partial-string based across both entry titles and the full editable note/body. Results are bounded to 24 entries and retain filesystem traversal order rather than allocating memory for runtime sorting.

### Full-screen Journal Note Editor

Selecting **Body/Notes** on a Journal entry opens a wrapped full-screen note view with an insertion cursor. Left/Right moves the cursor one character, Up/Down moves by wrapped display line, and OK opens the native full-screen keyboard; submitted text is inserted at the selected cursor without replacing the surrounding note. Hold Left deletes the previous character and Hold Right deletes the next character. Back returns to the Journal entry. The editor continues to use the existing Journal `body` field, so Session Log notes and Journal Search require no schema migration.

### Editable TXT content and Adventure Reward Preview

DNDAdventure previews pending Item rewards, milestones, quest flags and achievements before applying a rewarding choice. Confirming the preview reuses the existing Adventure choice/reward transaction path; cancelling writes nothing.

Catalog, metadata/grant and campaign-pack TXT files are intentionally editable. Production code does not require their contents to match a shipped hash, checksum, SHA or digest. Internal hashes used by Bestiary and Spellbook are lookup/index helpers derived from names/IDs and do not reject modified TXT content. `tests/host/validated_sources.sha256` is release-validation evidence only and is not consumed by any FAP at runtime.

### Backup Clone and Character Validation

DNDBackup & Restore includes **Clone Active Character** and **Validate Character**. Clone allocates the next available local profile ID and uses the same transactional duplicate operation already used by profile management, carrying the character-owned companion collections and Inventory bags with it. The new clone is left inactive so the current play context is not changed unexpectedly. Validation is read-only and combines the existing canonical save verifier/parser with basic semantic checks for class levels, ability scores and HP bounds.

### Grant Progression Diagnostics

In DNDGrants **Grant Review**, hold **Right** to open **Progression Diagnostics**. The view summarizes Applied, Pending, Pending Choice and Skipped/manual-review counts. Up/Down inspects the staged grants; OK shows a compact stable-ID/payload diagnostic. Diagnostics reads the existing grant state only and does not create another progression ledger.

### DNDGrants Progression Diagnostics

**Progression Diagnostics** appears in DNDGrants' Grant Review menu only when **DNDolphins → Settings → Debug** is enabled. When Debug is off, the row is omitted entirely. There is no hidden shortcut; diagnostics is a normal selectable menu action in Debug mode.

## Package-output policy
