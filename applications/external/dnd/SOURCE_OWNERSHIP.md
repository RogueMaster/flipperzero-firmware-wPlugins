# Source ownership

Shared source is retained when multiple FAPs use the same canonical contract. App-specific executable behavior must remain unreachable from unrelated FAP entry roots so section garbage collection can keep each runtime working set small.

## Header and symbol rules

- Keep implementation-only helpers `static` in the owning translation unit/common core.
- Declare a function in a header only when another translation unit genuinely calls it.
- Active project types/symbols use DND namespaces. Pocket-era strings remain only as explicit legacy read aliases.
- Ownership cleanup must not change persisted schemas, bounded paging, active-profile selection, launch/return behavior or gameplay rules.

## Shared application implementation

`dnd_app_core.c` contains the proven common character/grant/combat scene engine used by three FAP build modes. Each FAP now has an explicit entry translation unit and mode header:

- DNDolphins: `dnd_app_hub_entry.c` + `dnd_app_hub.h`, compiled with `DND_BUILD_HUB=1`.
- DNDCombat: `dnd_app_combat_entry.c` + `dnd_app_combat.h`, compiled with `DND_BUILD_COMBAT=1`.
- DNDGrants: `dnd_app_grants_entry.c` + `dnd_app_grants.h`, compiled with `DND_BUILD_GRANTS=1`.
- `dnd_app_common.h` contains declaration dependencies required by the common engine; a header include by itself has no runtime RAM cost.

This is intentional source sharing, not runtime co-residency. There are no `.inc` implementation fragments and no three-line source wrappers that textually include a monolith. Build-mode guards plus function/data sections and linker garbage collection prevent Combat and Grant roots from being retained in the normal DNDolphins executable.

The fixed app state has also been reduced rather than merely reorganized. The profile directory is a transient `DndProfileState*`; catalog state is a 48-byte screen-owned runtime; roll/dice state is a 76-byte runtime allocated only on roll-capable Hub screens or for the active Combat session; Grant Review state is a 24-byte runtime owned by DNDGrants; collection cache/index state is a compact 64-byte runtime created only when a streamed collection or spell-count cache is used and released when its last consumer exits; page-offset tables remain separately lazy; and the text edit buffer is allocated only while TextInput is active. Combat-only attack/spell/index state lives in a 248-byte `DndCombatRuntime` allocated only by DNDCombat. The regenerated 32-bit host-layout proxy reports a **3,416-byte** common app state, down from 4,676 bytes (1,260 bytes / 26.9%).

## Shared modules

- `dnd_profile_handoff.*`: used by the suite for persisted `Active=<id>` resolution, exact profile references, common FAP paths, launch arguments and parent-return behavior. Initiative's `from=combat` handoff is defined here.
- `dnd_profile_projection.*`: narrow canonical-field projections used by Inventory, Spellbook and Adventure.
- `dnd_data.*`: character/record allocation, defaults and sanitize support.
- `dnd_rules_core.c` / `dnd_rules.h`: shared rule math.
- `dnd_spell_eligibility.*`: spell-level/class eligibility used by DNDCombat, DNDGrants and DNDSpellbook; it is no longer linked into the DNDolphins hub.
- `dnd_weapon_rules.*`: weapon ability/attack modifier math used by DNDCombat, DNDGrants and DNDInventory; it is no longer linked into the DNDolphins hub.
- `dnd_storage.*`: canonical profile parsing, bounded sidecar paging, bag-aware Inventory lifecycle operations, internal SHD history/restore and transactional publication. Main/Group/named bags are preserved through duplicate/archive and SHD v3. User-facing external backup/import convenience logic is intentionally excluded from this shared module.
- `dnd_settings.*`: shared persisted settings. DNDolphins owns the Settings UI; DNDCombat consumes dice/combat-relevant preferences; DNDGrants consumes Catalog/Homebrew; Inventory and Spellbook consume their catalog/equipment preferences; Adventure/Initiative consume their applicable shared settings.
- `dnd_extra_items.*`: randomized 420/start-item policy where linked.
- `dnd_character_collections.*`: Languages/Proficiencies bounded readers/CRUD used by the character/grant workflows. Owned-list windows are four records; DNDolphins allocates the page only while one of these lists is open.

## App-owned behavior

- **DNDolphins:** character/profile/home/vitals/abilities/skills/features/class progression/dice/settings workflows and companion launching. It exclusively owns the splash plus Graphical Home icon assets/renderer; companion FAPs do not link `dndolphins_menu_graphics.c` or the DNDolphins private icon pack. Home **Magic & Spells** is only a launch bridge to DNDSpellbook. It does not own the runtime Combat workflow, Grant Review workflow or Magic management UI.
- **DNDCombat:** standalone Combat menu, weapon attacks, spell attacks, rituals, attack templates, combat recovery/state controls and **Jump to Initiative**. Dedicated implementations include `dndolphins_spell_combat.*` and `dndolphins_weapon_combat.*` plus the Combat mode entry/root set.
- **DNDGrants:** grant review/application and read-only progression diagnostics; standalone **Grant Initial Traits** / **Apply Level Grants**, Grant Review/Edit and grant-choice catalog workflow. It returns to DNDolphins after completion/Short Back; Hold Back exits.
- **DNDInventory:** Inventory, currency, item catalog/editing, starting-equipment review, bag selection/management and equipment/weight state. Only the selected bag page is resident; Inventory Resources streams all bags.
- **DNDSpellbook:** Spellbook list/catalog/filtering/editing, deterministic sorting, and the **Magic & Spells** management view for casting ability, Spell Attack/DC misc, Known/knowable/free-granted totals and shared slot current/max values. Normal launch opens the list with **Magic & Spells** as the terminal row after the final spell; the Magic launch argument opens the Magic view directly.
- **DNDAdventure:** campaigns, campaign packs, Adventure progression and rewards.
- **DNDJournal:** Journal UI and Journal persistence.
- **DNDInitiative:** roster, initiative/combat turn order, completed-encounter history browser and feature recharge integration. Monster Turn Tools launches DNDBestiary only for non-party combat participants and preserves the active Initiative state. It returns to DNDCombat only when explicitly launched from Combat.
- **DNDBestiary:** monster browse/state/packs, encounter generation/handoff, and stat-block-owned Monster Turn Tools. Initiative can deep-link by monster name; Bestiary resolves the allowed catalog entry lazily and returns to Initiative without duplicating monster stat data in Initiative.
- **DNDBackup & Restore:** user-facing SHD backup destination, backup creation, native `.shd` restore browser, external-bundle validation/staging and restore launch, plus active-character clone and read-only character validation. `dnd_backup_storage.*` and the private `dndbackup_images/` icon pack are linked only by this FAP.

## FAP list

The 4.19.1 suite contains **eleven external FAPs**:

1. DNDolphins
2. DNDCharacter Sheet
3. DNDGrants
4. DNDCombat
5. DNDInventory
6. DNDSpellbook
7. DNDAdventure
8. DNDJournal
9. DNDInitiative
10. DNDBestiary
11. DNDBackup & Restore

Each manifest lists only the source files needed by that build mode. DNDolphins specifically excludes the dedicated Combat implementation units.

## Grant and Combat memory ownership

Grant review batches are bounded at 24 resident `DndGrant` records and now run in DNDGrants, so their allocation/reallocation pressure does not coexist with DNDolphins. Inventory starting-equipment grants remain Inventory-owned.

Combat weapon/spell indexes use bounded eight-record windows with independent `uint16_t` totals. Spell Attacks owns attack-roll spells; Combat Utility Spells owns castable non-attack spells. They run in DNDCombat; the 26 Combat menu actions are not an attack count limit.

DNDBestiary consumes only the shared Debug and Homebrew bytes from DNDolphins Settings at startup. Pack Diagnostics is debug-only. Homebrew controls custom-pack visibility/selection across browsing, generated/custom encounters and saved-encounter execution without changing the stable on-disk source token; user-created Custom monsters are independent.

## DNDCharacter Sheet

- `dndcharactersheet.c` owns the standalone read-only graphical character-sheet UI.
- It reads the canonical active DNDolphins profile through `dnd_profile_handoff` + `dnd_storage`; it does not own or write character state.
- `dnd_rules_core.c` remains authoritative for ability, save, skill, proficiency and level-derived modifiers displayed by the sheet.

## Backup / Restore ownership

`dndbackup.c` owns the user-facing SHD backup folder and restore browser. `dnd_backup_storage.*` owns external bundle export/import/staging and is linked only into DNDBackup. `dnd_storage.c` retains the reusable internal SHD history/transactional restore primitives used for data integrity. `dnd_app_core.c` contains only the launcher; it does not call user-facing export/import/restore functions.

## Refactor integration regression gate

The current host harness compiles the core separately under each manifest build mode, tests real input/screen transitions, and measures optional-state release/retry. Catalog selection snapshots return state before teardown; selected Hub Feats persist before handing dependent review to DNDGrants. Favorite Spells owns its bounded index/page through casting and return; the Combat Roll runtime survives the Magic statistics screen for the session. The obsolete `.inc` and three wrapper files are absent from the corrected package.
