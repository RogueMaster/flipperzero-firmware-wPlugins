# Rules audit

Rule ownership is by feature domain rather than by the word “rule.”

## Shared rules

`dndolphins_rules.*` owns generic dice, ability modifiers, total level, proficiency, saves, skills, initiative, XP/level helpers, exhaustion/speed and other cross-feature character math.

## Items

`dndolphins_items.*` owns carrying capacity, currency normalization, calculated equipment AC, weapon ability selection, attack modifier, attack rolls and weapon damage behavior. It also owns starting-inventory policy and Adventure item reward helpers.

## Spells

`dndolphins_spells.*` owns casting ability, spell attack/save DC, class spell-level limits, multiclass slot calculation, Pact/shared-slot initialization, spell-point costs and cast-resource options. `dndolphins_spell_combat.*` remains the structured spell effect/damage mapping layer.

The previous rule split was regression-checked during the refactor so moved functions retained their call sites and behavior. The Ghost Protocol/default-monster additions do not change character or combat rules.

## Initiative feature mapping

The active-character Initiative refresh recognizes the base Dexterity modifier, Initiative Misc, exhaustion penalty, Alert, and Jack of All Trades. Alert uses the character proficiency bonus and suppresses Jack of All Trades' half-proficiency contribution so proficiency is not counted twice. Unmapped initiative effects remain representable through Initiative Misc and the per-participant Normal/Advantage/Disadvantage roll setting.

## Player-choice progression

Deterministic numeric progression and fixed metadata grants may apply automatically on level increases. Player-choice spell acquisition is not guessed: when cantrip/prepared allowances increase, the UI tells the player to choose spells. Initial level-one grants are staged for review before application. A dedicated ASI/feat chooser remains a future workflow rather than silently selecting a feat or ability increase.
