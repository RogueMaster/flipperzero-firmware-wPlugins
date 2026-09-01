# Feature checklist

Current user-facing feature coverage for the seven Dungeons & Dolphins FAPs.

## DNDolphins

- [x] Multiple character profiles with create, switch, rename, duplicate, export, import, archive, delete, save check and backup restore actions.
- [x] Character identity, species, background, alignment, multiclass levels/subclasses, XP/Milestone leveling, languages, proficiencies and Inspiration.
- [x] Bundled class selection configures the class Hit Die and spellcasting mode; recognized older/imported classes can receive missing class-derived spellcasting setup.
- [x] Subclass catalog defaults to the selected class and supports a class-filtered / All toggle.
- [x] Standard-array defaults plus six abilities, saving throws, 18 skills and passive Perception/Insight/Investigation.
- [x] Fresh-character defaults include the standard starting profile state and three editable starter attack templates.
- [x] Vitals including current/max/temp HP, AC, speed, initiative, exhaustion, death saves and Hit Dice.
- [x] Fixed-average HP gain and Hit Dice refresh when class level increases.
- [x] Explicit ASI/Feat level choices with dedicated ASI +2 and ASI +1/+1 application.
- [x] Increasing class level raises XP to at least the minimum threshold for the resulting total level.
- [x] **Grant Initial Traits** for deterministic starting traits and **Apply Level Grants** for deterministic catch-up through current levels; both report `Updated` or `No changes`.
- [x] Grant Review supports Apply All plus Pending/Skipped grant records, editing unresolved grants and adding custom grant records.
- [x] Character-owned Feature/Perk records with uses, recharge cadence, source class and resource formulas.
- [x] Progression Feat picker defaults to **Allowed**; Hold OK toggles **Allowed / All**.
- [x] Casting ability, Spell Attack/DC, spell slots, multiclass casting, Pact Magic, spell points, Mystic Arcanum and Arcane Recovery.
- [x] Weapon Attacks with ability/proficiency/magic/advantage rules, versatile damage, extra dice, criticals and Inventory ammunition consumption.
- [x] Weapon-local ammunition counters are consumed before loose Inventory ammunition when configured.
- [x] Common custom/legacy bows, crossbows, slings, blowguns, muskets and pistols can infer their standard ammunition family when Ammo Group is blank.
- [x] Loose ammunition lookup accepts the required ammo token anywhere in the Item name, case-insensitively.
- [x] Spell Attacks with class casting modifiers, cantrip scaling, upcasting, structured damage/healing effects and supported slot/Pact/point/free-cast resource use.
- [x] Ritual casting path for eligible known Wizard ritual spells.
- [x] Editable attack templates.
- [x] Short Rest, Spend Hit Die and Long Rest recovery actions.
- [x] Conditions, Concentration, Reaction, temporary effects, defenses, senses, movement, death-save and exhaustion controls.
- [x] General Dice Roller with d4/d6/d8/d10/d12/d20/d100, quantity, modifier, Advantage, Disadvantage and Guidance.
- [x] Dice Roller Guidance automatically adds 1d4 to supported d20 rolls; Advantage/Disadvantage return to Normal when the dice setup no longer supports those modes.
- [x] Launch/return integration with Inventory, Spellbook, Bestiary, Initiative, Adventure and Journal.

## DNDInventory

- [x] Opens directly to the active character's Inventory with **+ Add New**.
- [x] Automatic one-time starting-equipment grant for a truly empty, never-granted Inventory.
- [x] Explicit **Grant Initial Inventory** action plus one-time Hold OK regrant override.
- [x] Eight-record bounded Inventory paging.
- [x] Full Item editor with quantity, weight, equipment/attunement, weapon, damage, ammunition, charges, armor, container and source fields.
- [x] Item Name catalog with All, Weapons, Armor, Ammunition, Gear, Tools, Mounts/Vehicles, Potions, Rings, Rods, Scrolls, Staffs, Wands, Wondrous and Magic filters.
- [x] Recognized bundled weapon/armor selections populate useful mechanical presets including weight, damage, properties, Versatile die, ammunition family, armor AC/DEX cap and shield bonus.
- [x] Generic Spell Scroll entries for Cantrip and Levels 1–9 with level-appropriate rarity.
- [x] Hold OK quick Equip/Unequip.
- [x] Hold Left/Right quick Stack Qty adjustment.
- [x] CP/SP/EP/GP/PP currency editing and coin normalization.
- [x] Carried/equipped weight, capacity, Standard/Variant encumbrance, attunement count and calculated armor/shield AC.
- [x] Normal carrying capacity uses Strength × 15 lb with an explicit carrying-capacity override.
- [x] Attunement counts against the normal three-item limit and warns when exceeded without forcibly removing attunement.
- [x] Container references with safe index remapping when Items are deleted.

## DNDSpellbook

- [x] Opens directly to the active character's Spellbook with **+ Add New**.
- [x] Eight-record bounded Spellbook paging.
- [x] Full Spell editor with source class, level, Known/Prepared/Always Prepared/Ritual, free casts and catalog/source/school/grant metadata.
- [x] Hold OK quick Prepare/Unprepare for Known spells.
- [x] Owned Spells kept in level-ascending, case-insensitive alphabetical order.
- [x] Bundled 448-spell catalog sorted by level then name.
- [x] Filters for Level, Class, Ritual, School, Source, Status and Eligibility.
- [x] **Character Classes** default class filter, plus **Any Class** and all 13 supported class filters.
- [x] **Allowed** character eligibility and **All Spells** catalog browsing modes.
- [x] Eldritch Knight and Arcane Trickster Wizard-list eligibility.
- [x] Catalog selection resolves a compatible owned Source Class and marks the selected spell Known.
- [x] Catalog selection copies level, Ritual, School, Source and Stable ID metadata into the owned Spell record.

## DNDAdventure

- [x] Per-character campaign progress.
- [x] Declarative scene text, branching choices, skill checks, flags, achievements, milestones and Item rewards.
- [x] Adventure skill checks use the active character's real skill modifier, including proficiency/expertise and miscellaneous modifiers.
- [x] Guarded Item and milestone rewards are one-shot so revisiting the same rewarded branch does not duplicate them.
- [x] Adventure Item rewards append directly to the active character's Inventory.
- [x] Hold OK full-scene text viewer.
- [x] Hold Left load checkpoint and Hold Right save checkpoint.
- [x] Explicit current-adventure restart with confirmation.
- [x] Journal milestone integration and Journal-to-Adventure continuation.
- [x] Bundled Reef Wardens, Ghost Protocol, Torii Between Tides and Moonlit Market campaigns.
- [x] Installable campaign-pack format with compatibility/content checks before installation.

## DNDJournal

- [x] Per-character timestamped entries sorted newest first.
- [x] Quick, Adventure, Item and Milestone categories.
- [x] Editable title/body and completion state.
- [x] Milestone class selection and one-time milestone-level application.
- [x] Item entries can create Inventory Items.
- [x] Matching Milestone entries can continue the active Adventure.

## DNDInitiative

- [x] Persistent Party Roster plus temporary combat participants.
- [x] Main-character refresh from canonical name, HP, AC and initiative rules.
- [x] Opening Initiative adds or refreshes the active character in the persistent Party Roster without creating duplicate copies.
- [x] Main-character initiative includes DEX, Initiative misc, supported Alert proficiency-bonus and Jack of All Trades half-proficiency behavior, plus Exhaustion penalty.
- [x] Normal, Advantage and Disadvantage initiative modes plus **Roll for All**.
- [x] Full numeric editing for initiative total/modifier, AC and HP.
- [x] Manual participant reordering.
- [x] Hold Up quick AC adjustment during setup.
- [x] Active-combat HP controls, Hold Down Conditions, Hold Left/Right reordering, Hold OK full edit and Hold Up previous turn.
- [x] Round/turn display and Resume without ending combat.
- [x] Initiative ties use initiative modifier as a tie-breaker.
- [x] Explicit **End + Save History**, **End Without History** and Cancel choices.
- [x] Saved encounter history includes end time, rounds, party state and surviving opponents.
- [x] Main-character HP/AC synchronization and Turn/Encounter Feature recharge.
- [x] Single-monster and complete-encounter handoff from Bestiary.

## DNDBestiary

- [x] Streamed bundled monster catalog with Search, Max CR, Type, Source, Environment and Role filters.
- [x] Monster Search uses case-insensitive substring matching.
- [x] Full stat blocks with abilities, defenses, senses, traits, actions and additional text.
- [x] Favorite and Recent monster lists.
- [x] Opening a monster automatically records it in Recent Monsters.
- [x] Custom monster create/edit/delete.
- [x] Party Level, Party Size, Difficulty, Encounter Environment, Encounter Role, Repeat Types and Balanced/Horde/Elite templates.
- [x] Party Level and Party Size persist between Bestiary sessions.
- [x] Encounter generation with simulation and advisory composition warnings such as leader support, exposed artillery and high minion density.
- [x] Saved encounters with Resume, Send to Initiative, Rename, Duplicate, Archive and Delete actions.
- [x] Saved filter presets.
- [x] Bundled and custom monster-pack loading.
- [x] Pack Diagnostics.
- [x] 346 indexed/statblock-matched bundled monsters.

## Shared behavior

- [x] Active character is stored in `/ext/apps_data/dndolphins/custom_active_profile.txt` as `Active=<id>`.
- [x] Character-owned Inventory, Spellbook and Feature sidecars remain under `/ext/apps_data/dndolphins/` for cross-FAP access.
- [x] Companion main-screen Short Back returns to DNDolphins when installed; Hold Back exits to firmware.
- [x] Returning from a companion FAP refocuses the DNDolphins Home option that launched it.
- [x] Companion main screens display the resolved character ID in brackets where applicable.
- [x] Collections and catalogs use bounded streaming/paging rather than whole-file resident loading.
- [x] Editable campaign/monster text packs do not require checksums.
