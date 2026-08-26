# Dungeons & Dolphins 1.0 rules audit

This audit checks calculated fields and recovery behavior against the 2024 rules exposed in SRD 5.2.1. The application is a tracker and roller, not a substitute for rules text. External catalogs contain option names and original metadata rather than reproduced descriptions.

## Character mathematics

| Area | Implemented behavior | Result |
|---|---|---|
| Ability modifier | Floor of `(score - 10) / 2`, including odd negative scores | Pass |
| Proficiency Bonus | `2 + floor((total level - 1) / 4)`, total level capped at 20 | Pass |
| Saving throws | Governing ability modifier, optional proficiency, and miscellaneous adjustment | Pass |
| Skills | Governing ability plus none/proficiency/expertise and miscellaneous adjustment | Pass |
| Initiative | Dexterity modifier plus miscellaneous adjustment | Pass |
| Passive scores | `10 +` the relevant skill total | Pass |
| Spell attack | Casting ability modifier + Proficiency Bonus + miscellaneous adjustment | Pass |
| Spell save DC | `8 +` casting ability modifier + Proficiency Bonus + miscellaneous adjustment | Pass |
| Exhaustion | Applies the configured level penalty to D20 Tests and adjusted speed | Pass |

All six saving throws are explicit. All 18 standard skills are grouped by governing ability: Athletics under Strength; Acrobatics, Sleight of Hand, and Stealth under Dexterity; Arcana, History, Investigation, Nature, and Religion under Intelligence; Animal Handling, Insight, Medicine, Perception, and Survival under Wisdom; and Deception, Intimidation, Performance, and Persuasion under Charisma.

## Multiclass and spellcasting

- Total character level and Proficiency Bonus use the sum of class levels.
- Shared multiclass spell slots are calculated independently of each class's spell records.
- Full, half, and one-third caster contributions are supported; Pact Magic and spell-point/custom pools remain separate.
- Each class tracks its casting ability, cantrip limit, prepared limit, spellbook size, Pact slots, Mystic Arcanum mask, and spell points.
- Known, Prepared, Always Prepared, Ritual, and renewable free-cast states are independent.
- A Long Rest restores ordinary slots and configured long-rest resources. Arcane Recovery is a dedicated budgeted helper rather than a blanket Short Rest slot refill.
- Unusual class or table-specific rounding can be represented with custom/manual casting mode and editable slot maxima.

## Combat and rests

- Attack rolls add the chosen ability, proficiency when enabled, magic bonus, and miscellaneous modifier.
- Finesse can choose the better Strength or Dexterity modifier. Ranged attacks default to Dexterity.
- Damage adds applicable ability and magic modifiers once. A critical hit doubles damage dice, including configured rider dice, without doubling fixed modifiers.
- Short Rest Hit Dice are spent from a selected class pool. Long Rest restores class pools and long-rest resources.
- Death saves, concentration, reaction availability, conditions, temporary effects, resistances, immunities, vulnerabilities, senses, and movement modes are tracked explicitly.
- Initiative history can undo turn movement, participant HP changes, and feature-resource changes.

## Inventory and currency

- Carried weight includes items stored inside containers; containers are organizational and do not make contents weightless.
- Equipped weight, Strength-based carrying capacity, capacity override, armor/shield Armor Class, attunement count, ammunition groups, charges, and optional encumbrance state are tracked.
- Coin normalization uses Copper, Silver, Electrum, Gold, and Platinum denominations.

## Catalog and builder boundary

Annotated records carry a stable ID, source label, option type, prerequisite note, level, class association, and grant value. The builder stages changes for review before applying them. On-device diagnostics check required fields, supported record types, duplicate IDs, and missing catalog files.

Rules that require a choice, copyrighted description, table ruling, or context-sensitive prerequisite remain player-entered or review-gated. This includes exact feature effects, bespoke spell eligibility, equipment packages, and unusual multiclass exceptions. The full-catalog view and custom text entry intentionally remain available.

## Remaining verification

- Hardware navigation and long-session heap behavior require physical-device testing.
- Catalog metadata supplied by users should be validated on device after copying.
- Schema 2 is the current pre-release format. Older schemas are deliberately rejected until a stable migration policy is adopted.
