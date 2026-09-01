# Rules audit

Rule ownership is by feature domain rather than by the word “rule.”

## Shared rules

`dnd_rules_core.c` and `dnd_rules.h` expose only the compact rule math still needed across FAP boundaries: single-die/summed-dice rolling, ability modifiers, total level, proficiency, saves, skills and exhaustion. DNDolphins-only Initiative/effective-speed calculations and rest/character-mutation behavior live in `dndolphins_rules_character.*`. DNDolphins-only advantage/disadvantage d20 selection and value-recording multi-die rolls live in `dndolphins_dice.*`; their enum/API is no longer part of the shared rules header.

## Items

`dnd_weapon_rules.*` owns the weapon ability/attack-modifier math shared by DNDolphins and DNDInventory. DNDolphins attack/damage roll result types and combat-time Item access live in `dndolphins_weapon_combat.*`; the shared rules header no longer carries those DNDolphins-only result structs. `dndinventory_rules.c`, `dndinventory_items.c` and `dndinventory_collection.c` own carrying capacity, currency, equipment AC, starting-inventory policy and derived equipped/weight aggregation; `dndadventure_item_reward.*` owns Adventure reward appends.

Starting-inventory currency is treated as part of the same grant transaction as the starting Item records. The Inventory sidecar publishes the existing balance plus the selected class/species/background grant, the Item rows, and the one-shot grant marker in one synced write; the UI then adopts the exact committed `Currency=` total. The one permitted regrant likewise returns the exact combined balance written by the transactional rewrite instead of recomputing it from potentially stale UI state.

## Spells

`dndolphins_spells.*` owns casting ability, spell attack/save DC, DNDolphins class spell-count derivation, native and multiclass slot calculation, Pact/shared-slot initialization, spell-point costs and cast-resource options. The small class maximum-spell-level rule remains in `dnd_spell_eligibility.*` because both DNDolphins progression and DNDSpellbook catalog eligibility use it. Spellbook owns its own Eldritch Knight/Arcane Trickster Wizard-list alias test. `dndolphins_spell_combat.*` remains the structured spell effect/damage mapping layer.

Wizard combat eligibility distinguishes the spellbook from the prepared list. Wizard cantrips are available as known cantrips. A level-1+ Wizard spell is eligible for the Combat → Spell Attacks list only when `prepared`, `always_prepared`, or `free_casts_current > 0`. If an unprepared Wizard spell is present only because a Free Cast remains, its combat cast-option list contains only the Free Cast; normal slots, Pact slots and spell points are not offered. Non-Wizard classes keep their existing Known/Prepared model.

Wizard **Ritual Adept** is represented separately by Combat → Rituals. A level-1+ spell is listed when it is a known Wizard spellbook entry with the Ritual tag; preparation is intentionally irrelevant. Ritual casting consumes no slot, Pact slot, spell points or Free Cast and reports the additional 10-minute casting time. Wizard cantrips are excluded because they are not level-1+ spellbook entries.

The structured spell-combat table now contains 168 explicit mappings. The resolver supports fixed and dice-based upcasting, multiple attack/roll instances, primary/secondary effects and derived effects while retaining Notes `XdY` fallback for unmapped custom spells. New deterministic mappings include Aid's vitality scaling and Heal's fixed healing/upcast behavior.

The previous rule split was regression-checked during the refactor so moved functions retained their call sites and behavior. The Ghost Protocol/default-monster additions do not change character or combat rules.

## Initiative feature mapping

The active-character Initiative refresh recognizes the base Dexterity modifier, Initiative Misc, exhaustion penalty, Alert, and Jack of All Trades. Alert uses the character proficiency bonus and suppresses Jack of All Trades' half-proficiency contribution so proficiency is not counted twice. Unmapped initiative effects remain representable through Initiative Misc and the per-participant Normal/Advantage/Disadvantage roll setting.

## Player-choice progression

Deterministic numeric progression and verified fixed class/subclass/species metadata grants may apply automatically on level increases. The metadata set includes deterministic SRD subclass features where the class/subclass and level fully determine the result; selection-dependent Fighting Styles, Invocations, Metamagic, spells, ASIs and feats remain explicit player choices. Warlock Eldritch Invocations begins at class level 1; the app records the deterministic feature availability but does not choose an Invocation.

Player-choice spell acquisition is not guessed: when cantrip/prepared allowances increase, the UI tells the player to choose spells. Initial deterministic level-one grants are applied when the player explicitly selects **Grant Initial Traits**; failed grants remain reviewable, and deterministic spell grants verify the authoritative Spellbook sidecar so stale applied markers can repair missing spell records without creating duplicates. **Level Choices** handles explicit ASI/Feat opportunities and always opens, including an explicit no-pending state. After a level increase, the bounded **Level-Up Review** stores only before/after rule results, deterministic-grant count and pending-choice flags; progression metadata itself is released after evaluation.

## Progression feat eligibility

Progression feat picks default to Allowed and may be switched to All. Bundled checks cover General/Epic level gates, Grappler STR/DEX 13+, Fighting Style Feature, Spellcasting Feature where required, and duplicate suppression for non-repeatable feats. Ability Score Improvement, Magic Initiate and Skilled remain repeatable. Unknown/custom feats remain best-effort visible rather than receiving guessed prerequisites.

## Deterministic subclass additions

Additional deterministic grants cover Path of the Berserker, College of Lore, Oath of Devotion, Hunter, Draconic Sorcery and Fiend Patron. Features requiring a player selection (for example Hunter option choices, Magical Discoveries, Fiendish Resilience choice and Draconic damage-type choice) remain explicit and are not auto-selected.

## Spell-combat addition

Heroism has a structured Temporary-HP result using the caster's spellcasting modifier. Higher-slot target count is intentionally not represented as extra HP because the combat mapping has no target-count state.
