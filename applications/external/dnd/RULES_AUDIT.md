# Rules audit

Rule ownership is by feature domain rather than by the word “rule.”

## Shared rules

`dnd_rules_core.c` and `dnd_rules.h` expose only the compact rule math still needed across FAP boundaries: single-die/summed-dice rolling, ability modifiers, total level, proficiency, saves, skills and exhaustion. DNDolphins-only Initiative/effective-speed calculations and rest/character-mutation behavior live in `dndolphins_rules_character.*`. DNDolphins-only advantage/disadvantage d20 selection and value-recording multi-die rolls live in `dndolphins_dice.*`; their enum/API is no longer part of the shared rules header.

## Items

`dnd_weapon_rules.*` owns the weapon ability/attack-modifier math shared by DNDolphins and DNDInventory. DNDolphins attack/damage roll result types and combat-time Item access live in `dndolphins_weapon_combat.*`; the shared rules header no longer carries those DNDolphins-only result structs. `dndinventory_rules.c`, `dndinventory_items.c` and `dndinventory_collection.c` own carrying capacity, currency, equipment AC, starting-inventory policy and derived equipped/weight aggregation; `dndadventure_item_reward.*` owns Adventure reward appends.

Starting-inventory currency is treated as part of the same grant transaction as the starting Item records. The Inventory sidecar publishes the existing balance plus the selected class/species/background grant, the Item rows, and the one-shot grant marker in one synced write; the UI then adopts the exact committed `Currency=` total. The one permitted regrant likewise returns the exact combined balance written by the transactional rewrite instead of recomputing it from potentially stale UI state.

## Spells

`dndolphins_spells.*` owns casting ability, spell attack/save DC, DNDolphins class spell-count derivation, native and multiclass slot calculation, Pact/shared-slot initialization, spell-point costs and cast-resource options. The small class maximum-spell-level rule remains in `dnd_spell_eligibility.*` because both DNDolphins progression and DNDSpellbook catalog eligibility use it. Spellbook owns its own Eldritch Knight/Arcane Trickster Wizard-list alias test. `dndolphins_spell_combat.*` remains the structured spell effect/damage mapping layer.

Spellbook catalog class filtering is independent of the character class-slot array. **Character Classes** remains the default union of the active character's spell lists; **Any Class** and each supported class name are explicit browse filters. `Allowed` requires actual character spell-list access plus the current class maximum spell level, while `All Spells` bypasses character eligibility but still honors a selected class's catalog membership. Eldritch Knight and Arcane Trickster count as Wizard-list access.

Wizard combat eligibility distinguishes the spellbook from the prepared list. Wizard cantrips are available as known cantrips. A level-1+ Wizard spell is eligible for the Combat → Spell Attacks list only when `prepared`, `always_prepared`, or `free_casts_current > 0`. If an unprepared Wizard spell is present only because a Free Cast remains, its combat cast-option list contains only the Free Cast; normal slots, Pact slots and spell points are not offered. Non-Wizard classes keep their existing Known/Prepared model.

Wizard **Ritual Adept** is represented separately by Combat → Rituals. A level-1+ spell is listed when it is a known Wizard spellbook entry with the Ritual tag; preparation does not affect eligibility. Ritual casting consumes no slot, Pact slot, spell points or Free Cast and reports the additional 10-minute casting time. Wizard cantrips are excluded because they are not level-1+ spellbook entries.

The structured spell-combat table contains 168 explicit mappings. The resolver supports fixed and dice-based upcasting, multiple attack/roll instances, primary/secondary effects and derived effects while retaining Notes `XdY` fallback for unmapped custom spells. Mappings include Aid vitality scaling and Heal fixed/upcast healing.

## Initiative feature mapping

The active-character Initiative refresh recognizes the base Dexterity modifier, Initiative Misc, exhaustion penalty, Alert, and Jack of All Trades. Alert uses the character proficiency bonus and suppresses Jack of All Trades' half-proficiency contribution so proficiency is not counted twice. Unmapped initiative effects remain representable through Initiative Misc and the per-participant Normal/Advantage/Disadvantage roll setting.

## Player-choice progression

Deterministic numeric progression (such as proficiency, spell-slot/limit math, HP and Hit Dice) may update when the level itself changes, but deterministic class/subclass/species metadata grants are never written by the level-change action. The metadata set includes deterministic SRD subclass features where the class/subclass and level fully determine the result; those records are applied only through the explicit grant actions. Selection-dependent Fighting Styles, Invocations, Metamagic, spells, ASIs and feats remain explicit player choices. Warlock Eldritch Invocations begins at class level 1; the app records deterministic feature availability only when the appropriate explicit grant action is selected and never chooses an Invocation.

Player-choice spell acquisition is not guessed: when cantrip/prepared allowances increase, the UI tells the player to choose spells. Initial deterministic level-one grants are applied only when the player explicitly selects **Grant Initial Traits**; later currently eligible deterministic species/class/subclass grants are applied only through **Apply Level Grants**. Failed grants remain reviewable, and authoritative character/Feature/Spellbook payload checks let stale applied markers repair missing deterministic records without duplicating records that are present; applied-grant marker writes are best-effort bookkeeping and cannot invalidate a successful authoritative write. **Level Choices** handles explicit ASI/Feat opportunities and always opens, including an explicit no-pending state. A class-level increase itself adds fixed-average Hit Die HP plus Constitution modifier (minimum 1), refreshes class/global Hit Dice, updates other derived progression values and opens the bounded **Level-Up Review** without applying deterministic grants.

## Progression feat eligibility

Progression feat picks default to Allowed and may be switched to All. Bundled checks cover General/Epic level gates, Grappler STR/DEX 13+, Fighting Style Feature, Spellcasting Feature where required, and duplicate suppression for non-repeatable feats. Ability Score Improvement, Magic Initiate and Skilled remain repeatable. Allowed is conservative: unknown/custom feat/perk rows are hidden because their prerequisites are not represented in bundled metadata, and duplicate checking hides the row if the Feature sidecar cannot be read; All continues to expose those rows. Manual Feature/Perk editing is unrestricted. `Ability Score Improvement` is omitted from the nested progression feat picker (Allowed and All) because the parent level-choice screen already owns the ASI +2 and +1/+1 effects. The two-ability ASI path does not mutate the first ability on its first pick; the second pick transaction applies exactly +1 to each stored baseline and rolls both back on persistence failure.

## Deterministic subclass additions

Additional deterministic grants cover Path of the Berserker, College of Lore, Oath of Devotion, Hunter, Draconic Sorcery and Fiend Patron. Features requiring a player selection (for example Hunter option choices, Magical Discoveries, Fiendish Resilience choice and Draconic damage-type choice) remain explicit and are not auto-selected.

## Spell-combat addition

Heroism has a structured Temporary-HP result using the caster's spellcasting modifier. Higher-slot target count is not represented as extra HP because the combat mapping has no target-count state.

Journal milestone leveling uses the same fixed-average HP and Hit-Dice refresh and does not apply deterministic progression grants; those remain an explicit Character action.
