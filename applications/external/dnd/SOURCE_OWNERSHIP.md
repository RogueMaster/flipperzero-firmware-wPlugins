# Source ownership

Shared source is retained when multiple FAPs use the same contract or when splitting a canonical parser/transaction would duplicate behavior. App-specific behavior stays with the FAP that owns it.

## Header rule

- Keep implementation-only helpers `static` in the owning `.c` file.
- Declare a function in a header only when another translation unit calls it.
- Keep app-only state/result types in app-owned headers.
- Shared headers expose only cross-module contracts, shared record types/constants or canonical persistence/rule primitives.
- Never silence `unused-function` to preserve stale wrappers; remove the wrapper after confirming the real implementation still has callers.

## Shared modules retained intentionally

- `dnd_profile_handoff.*`: used by **all seven FAPs**. It is the one shared contract for persisted `Active=<id>` resolution, exact canonical profile lookup/existence, common data/FAP paths, launch arguments and Loader handoff/parent-return behavior. The former profile-reference and handoff modules are intentionally consolidated here.
- `dnd_data.*`: shared character/record allocation, defaults, sanitize and load support.
- `dnd_rules_core.c` / `dnd_rules.h`: compact rule math used across FAPs. DNDolphins-only dice state and character-display helpers stay outside this interface.
- `dnd_spell_eligibility.*`: shared class maximum-spell-level calculation used by DNDolphins and DNDSpellbook.
- `dnd_weapon_rules.*`: weapon ability/attack modifier math shared by DNDolphins and DNDInventory.
- `dnd_storage.*`: canonical character/profile parsing, bounded collection paging and transactional persistence. It owns full storage mechanics where linked, but no longer contains a second active-profile metadata parser. A storage operation may remain here with one current caller when moving it would duplicate a parser, scanner or atomic rewrite/publish path.

## App-owned code

- **DNDolphins:** `dndolphins_rules_character.*`, `dndolphins_dice.*`, `dndolphins_spells.*`, `dndolphins_spell_combat.*`, `dndolphins_weapon_combat.*`, `dndolphins_progression_store.*` and `dndolphins.c`.
- **DNDInventory:** `dndinventory_collection.c`, `dndinventory_items.c`, `dndinventory_rules.c` and the entry wrapper. Item policy, Item catalog/filtering and derived equipment/weight/attunement state are Inventory-owned.
- **DNDSpellbook:** `dndspellbook_collection.c` and the entry wrapper. Spellbook UI/catalog/filtering, Eldritch Knight/Arcane Trickster Wizard-list aliasing and deterministic spell sorting are Spellbook-owned.
- **DNDAdventure:** Adventure/campaign/pack/reward sources remain Adventure-owned.
- **DNDJournal:** Journal UI/persistence behavior remains Journal-owned.
- **DNDInitiative:** Initiative roster/combat and feature-recharge sources remain Initiative-owned.
- **DNDBestiary:** Bestiary monster/state/pack/encounter sources remain Bestiary-owned.

## Manifest rule

Each FAP lists only the sources it actually links. `sources` entries are alphabetized for audit readability; source-list ordering has no runtime semantic role in these manifests.

## Stability constraint

Ownership cleanup must not change persisted schemas, collection ordering, active-profile selection, launch/return paths, lazy paging, draw-time I/O rules, grants or resource-consumption behavior. Shared code is split only when behavior remains single-source; app-specific code is moved local only when no other FAP needs that implementation.
