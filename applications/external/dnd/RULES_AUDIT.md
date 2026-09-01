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

The previous rule split was regression-checked during the refactor so moved functions retained their call sites and behavior. The Ghost Protocol/default-monster additions do not change character or combat rules.

## Initiative feature mapping

The active-character Initiative refresh recognizes the base Dexterity modifier, Initiative Misc, exhaustion penalty, Alert, and Jack of All Trades. Alert uses the character proficiency bonus and suppresses Jack of All Trades' half-proficiency contribution so proficiency is not counted twice. Unmapped initiative effects remain representable through Initiative Misc and the per-participant Normal/Advantage/Disadvantage roll setting.

## Player-choice progression

Deterministic numeric progression and fixed metadata grants may apply automatically on level increases. Player-choice spell acquisition is not guessed: when cantrip/prepared allowances increase, the UI tells the player to choose spells. Initial level-one grants are staged for review before application. **Level Choices** now handles explicit ASI/Feat opportunities: the player selects the choice, applied choices are represented by existing grant-history records, and the app never silently chooses a feat or ability increase.
