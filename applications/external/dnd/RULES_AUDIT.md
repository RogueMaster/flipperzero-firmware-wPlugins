# Dungeons & Dolphins rules audit

## Character progression

- Total level is the sum of class levels.
- Raising total level applies the Character Advancement XP minimum as a floor. It never reduces XP that is already higher and lowering level never reduces XP.
- Multiclass class levels, Hit Dice, spellcasting resources and derived values remain character-owned.

## Combat roll mode

- The main Combat screen exposes `Normal`, `Advantage` and `Disadvantage` as one shared attack-roll mode.
- Weapon attack d20s and spell attack d20s use the selected mode.
- Damage dice, saving-throw spells, healing rolls and automatic effects do not use attack advantage/disadvantage.

## Spell combat data

- Structured spell mappings remain the preferred source for attack/save resolution, base dice, cast-level scaling, multiple attack rolls, secondary dice and special effects.
- The Notes `XdY` parser is a guarded fallback for simple unmapped damage expressions.
- Moving all spell damage instructions into free-form Notes is intentionally avoided because parsing prose cannot represent every supported cast-level/multi-resolution rule as reliably as typed combat metadata. Spell-combat enumeration now walks the character spellbook through the bounded eight-record sidecar cache; paging must not change spell eligibility, casting ability, resource use, or damage resolution.

## Rest and combat state

- Character combat tracks HP/temp HP, death saves, reaction, concentration, conditions, temporary effects, resistances, immunities, vulnerabilities, senses, movement and exhaustion.
- DNDInitiative owns rounds/turns/participants. Short Back moves to the previous turn; hold Back exits combat back to the Initiative menu.

## Storage boundaries affecting rules

- Campaign flags/achievements never live in the character payload.
- Journal completion/milestone state never drives Adventure progress directly.
- Bestiary monster/encounter state is separate from the character save; explicit handoff writes participant data to DNDInitiative.

## Compatibility

Character schema 5 is the current writer format. Core character loading remains best-effort by recognized field name across version/order differences; unknown or malformed unrelated fields do not reject the rest of a readable character. Owned spells/items are defined only by `ch_{id}_spellbook.txt` and `ch_{id}_items.txt`; old embedded spell/item fields are intentionally ignored and are not migrated.
