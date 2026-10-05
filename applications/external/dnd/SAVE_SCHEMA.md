# Save and storage schema

### Companion profile projections

Canonical character files keep their named-field format. `dnd_profile_projection.c` is an in-memory access layer:

- DNDInventory reads name/species/background/classes/abilities/AC/exhaustion/encumbrance/carry-capacity. Its only canonical write-back is transactional replacement of AC plus encumbrance/carry-capacity inside existing `Vitals=` / `CombatFlags=` lines. Currency and Items remain Inventory-sidecar owned.
- DNDSpellbook reads only name and class/subclass spellcasting metadata. It never writes the canonical character file.
- DNDAdventure reads only name, class levels, ability scores and skill proficiency/misc values. It never writes canonical character fields.

Unrecognized canonical fields are preserved byte-for-line during the Inventory-owned projection rewrite. The projection line bound matches canonical encoded character lines, and the publish step uses temporary/backup rename rollback.

## Ownership

Primary ownership is separated by app-data root, with intentional shared access to the DNDolphins character root:

- DNDolphins: `/ext/apps_data/dndolphins/` — canonical characters, active profile, shared settings, character-owned collection/progression sidecars, level-history SHD bundles, exports/archive.
- DNDInventory: Main edits `/ext/apps_data/dndolphins/inventory_{id}.txt`; Group uses `invGroup_{id}.txt`; named bags use `inv{SafeBagName}_{id}.txt` with `BagName=` preserving the display name. It does **not** create a separate `/ext/apps_data/dndinventory/` character store.
- DNDSpellbook: edits `/ext/apps_data/dndolphins/spellbook_{id}.txt`; it does **not** create a separate `/ext/apps_data/dndspellbook/` character store.
- DNDAdventure: `/ext/apps_data/dndadventure/` — active campaign, progress, direct custom campaigns and installed campaign registry/index. Campaign registry and enabled-index rebuilds publish transactionally; interrupted inbox installs remove only newly copied install artifacts while preserving the inbox source so retry is safe. Adventure also updates the selected DNDolphins character-owned sidecars for rewards and writes milestone entries into that character's DNDJournal directory.
- DNDJournal: `/ext/apps_data/dndjournal/ch_{id}/` — per-character journal entries; Item entries may append to the shared DNDolphins Inventory sidecar.
- DNDInitiative: `/ext/apps_data/dndinitiative/ch_{id}.txt` — party/initiative state.
  - Stores RollMode and MainCharacterName alongside roster/combat fields so automatic roll behavior persists and the profile-backed main participant remains identifiable across character renames.
  - Turn/Encounter recharge reads/writes the shared DNDolphins Feature sidecar directly.
- DNDBestiary: `/ext/apps_data/dndbestiary/` — favorites, recents, filters, encounters, custom monsters and installed monster packs.


### Shared profile lookup

Companion FAPs resolve character profiles only from canonical DNDolphins files named `ch_{id}_{safeName}_{level}.txt`. Item/spellbook sidecars, work files, exports, shadows and Journal/Initiative files cannot satisfy a primary-profile lookup. Character ID `0` is valid.

`/ext/apps_data/dndolphins/custom_active_profile.txt` stores `Active=<id>` and is the only companion character-selection metadata. All eleven FAPs use the shared `dnd_profile_handoff.c` reader/lookup contract for this metadata and exact canonical-profile references; launch arguments do not override the persisted selection and companions do not discover/fall forward to another character. Inventory, Spellbook and Adventure then stream only their required canonical fields through `dnd_profile_projection.c`; they do not keep `DndCharacter` resident. DNDCharacter Sheet reads the exact active canonical profile read-only for its graphical pages. Initiative and Bestiary choose ID `0` only when active-profile metadata itself is absent or unreadable. If `Active=<id>` is present but that character is missing, Initiative does not switch to 0 or another character; Bestiary keeps the persisted ID and remains usable without a character profile.

## DNDolphins

Primary character files are named `ch_{id}_{safeName}_{level}.txt`. Recognized fields load independently and unknown fields are ignored. Live `.txt` files remain authoritative. Character `.shd` files are level-history snapshots and are never used as live input without an explicit restore action.

Shared suite settings live in `/ext/apps_data/dndolphins/settings.txt` with `SkipDiceLoading=0|1`, `Debug=0|1` and `ExtraItems=0|1` (1 means 420). Defaults are Off, Off and 420 respectively. Missing keys use their defaults; read failures are reported by the owning caller. The settings file contains no character-specific game state. Saves are transactional: a synchronized `settings.tmp` is published over the live file with `settings.bak` rollback protection, and successful saves remove both companions.

### Level SHD history and restore

A new/updated character save maintains a core level snapshot named `ch_{id}_{safeName}_{level}.shd`. The matching character-owned sidecars are snapshotted separately as `ch_{id}_{safeName}_{level}_items.shd`, `_spellbook.shd`, `_features.shd`, `_appliedgrants.shd`, `_languages.shd` and `_proficiencies.shd`. A small internal `_bundle.shd` marker records that the snapshot was created by the complete bundled-history writer. This keeps collection/progression history out of the core character record exactly as the live schema does.

Before a level-changing save publishes the new canonical character, the previous live core and live sidecars are refreshed into the previous level's SHD bundle. After the new canonical character publishes, the current level SHD bundle is refreshed. Same-profile/same-level renames remove stale duplicate core SHD filenames so the level selector remains unambiguous.

**SHD restore** is explicit and active-character only, and the user-facing selection/staging workflow is owned by DNDBackup & Restore. The selected core SHD is parsed as a character and republished to the normal canonical `.txt` path. Matching snapshot sidecars are then copied to `inventory_{id}.txt`, `spellbook_{id}.txt`, `feats_{id}.txt`, `appliedgrants_{id}.txt`, `languages_{id}.txt` and `proficiencies_{id}.txt`. `DNDHistoryBundle=3` additionally snapshots every non-Main Inventory bag as a separate `..._itemsbag_<SafeBag>.shd` companion and restores the bag split transactionally. `=2` declares the six fixed companions; `=1` declares only Items, Spellbook, Features and applied grants. For a companion declared by that marker, absence at that level removes it from live state so later collection state does not leak backward. Older bundle/core-only SHDs remain compatible and preserve bag state they never recorded. Restore keeps rollback copies of the current core/fixed sidecars and temporary backups of live non-Main bags, restoring them if any part of the multi-file publish fails.

Owned spells live in `spellbook_{id}.txt`. Inventory is bag-aware: Main remains `inventory_{id}.txt`, Group is `invGroup_{id}.txt`, and custom bags are `inv{SafeBagName}_{id}.txt`; non-Main files carry `BagName=<display name>`. Main also owns character-global currency through `Currency=cp,sp,ep,gp,pp`. Only one aligned Item page from the selected bag is resident at a time, while collection-wide/resource operations stream bags one by one. Duplicate, archive and export/import keep bags as separate companions. Current-level `.shd` snapshots are write history; the live `.txt` collection files remain authoritative.

Currency is not part of the character-profile schema. Character-profile `Currency=` lines are ignored on load and are not written on save. The only authoritative persisted currency record is `Currency=cp,sp,ep,gp,pp` in Main `inventory_{id}.txt`. Group/custom bag files contain `DNDItems=1`, `BagName=` and `I|...` rows and do not duplicate Currency. Other FAPs using the legacy Inventory API continue to target Main. When DNDInventory opens an existing Main sidecar without Currency, it initializes the missing record to `Currency=0,0,0,0,0`.

Persistent Features live in `feats_{id}.txt`. DNDolphins loads at most eight Feature records for the active Feature page; Feature use/recharge changes rewrite that sidecar immediately. Initiative Turn/Encounter recharge also operates directly on this sidecar, so Feature state does not need to be embedded in the canonical character file.

Applied deterministic progression IDs live in `appliedgrants_{id}.txt`. They are scanned/streamed during progression checks and are not loaded as a resident array. Legacy embedded Feature/Grant rows in older character files are tolerated but ignored and never allocated or migrated. Existing current-format sidecars remain authoritative; when a sidecar is absent it is treated as empty and is created only on the first real Feature write or applied-grant mark.

A truly empty Inventory with no `InitialInventory` marker receives the normal class/species/background starting package when DNDInventory opens. Equipment, starting currency and `InitialInventory=1` publish in the same synced sidecar transaction. The marker remains even if all Items are later deleted, preventing silent reseeding. **Grant Initial Inventory**, shown after the owned Item rows on the normal Inventory list and before the final **Bag Mover** action, exposes grant state and permits one deliberate Hold-OK regrant from state `1`; that append preserves existing Items, adds the starting package/currency again, and publishes `InitialInventory=2`. State `2` consumes the override. Failed writes do not advance the marker. A currency-only sidecar with no marker is still eligible for the normal initial grant. A d100 trinket is fallback-only when the selected starting assets yield neither Items nor currency and uses the bundled SRD trinket table.

## Adventure

Campaign progress is DNDAdventure-owned and stores campaign ID, scene/checkpoint, quest flags and achievements in its own text files. Milestones are journaled after the corresponding guard is saved so replaying a guarded reward does not duplicate it.

## Bestiary custom monsters

Custom monsters use:

- `/ext/apps_data/dndbestiary/monsters/custom_index.txt`
- `/ext/apps_data/dndbestiary/monsters/custom_statblocks.txt`

If neither exists, Bestiary may seed both from bundled default-custom assets. If either user file already exists, the seed does nothing. Existing recovery/transaction logic remains authoritative for user custom edits.

No campaign, Bestiary, Journal or Initiative state is serialized into the core character file.

### Initiative participant roll mode

DNDInitiative owns its `ch_{characterId}.txt` sidecar. Participant rows may include `RosterN RollMode` / `CombatN RollMode`-style named fields (serialized without spaces as `RosterNRollMode` and `CombatNRollMode`) using 0=Normal, 1=Advantage, 2=Disadvantage. Loading remains best-effort by field name, so older Initiative files without these fields default normally and remain readable.

## Ritual Adept derivation

Combat → Rituals adds no persisted field. The list is derived from existing Spellbook records: the spell must be `Known`, have `Ritual=1`, be level 1 or higher, and resolve as a Wizard spell for the active character. Preparation state does not affect this ritual list. Ritual casting consumes no slot/Pact/points/free-cast state, so no collection rewrite is required solely for the ritual action.

## Spellbook record order

Spellbook ordering is not a schema field. Valid `S|` records may be rewritten in ascending spell level and then case-insensitive alphabetical name order for deterministic display/paging. The existing record fields, preparation/free-cast state and non-record metadata remain authoritative; no migration marker or checksum is introduced.

## Initiative completed-encounter history

History is Initiative-owned and is created only when the user explicitly chooses **End + Save History**. Each completed encounter is a separate atomic record under `/ext/apps_data/dndinitiative/history/` named `ch_<profile>_<YYYYMMDD>_<HHMMSS>_<NN>.txt`; ending without history creates no record.

```text
DNDInitiativeHistory=1
Profile=<id>
Ended=YYYY-MM-DD HH:MM:SS
Rounds=<round>
P|name|hp_current|hp_max|ac|conditions
O|name|hp_current|hp_max|ac|conditions
```

`P` rows contain all party participants identified from the main character/saved roster, including downed members. `O` rows contain only opponents whose current HP is above zero when combat ends. History does not modify the canonical character file or Initiative live-combat schema.

## Languages, proficiencies and collection width

All paths below are relative to `/ext/apps_data/dndolphins/`.

| Live file | Header | Record format | SHD suffix |
|---|---|---|---|
| `languages_{id}.txt` | `DNDLanguages=1` | `L|Name` | `_languages.shd` |
| `proficiencies_{id}.txt` | `DNDProficiencies=1` | `P|Armor/Weapon/Tool|Name` | `_proficiencies.shd` |

Names use at most 46 characters, proficiency types at most 15. Embedded delimiters/newlines are rejected. Appends ignore an exact existing name (or exact type/name pair). Add/edit/delete uses a synced temporary file and guarded rename; live data survives failed publication. Character deletion removes live collections while retaining historical SHDs. Duplicate, export/import and archive handle all six companions.

Legacy Language counts/arrays and weapon/armor/tool/other free-text training fields are ignored on load and omitted on save. The new sidecars are the only authoritative format; no migration is attempted. `SaveProficiency` remains the sole saving-throw proficiency field.

Logical collection indexes/counts are `uint16_t`, while resident counts/capacities are limited to eight. The implementation is bounded by that index representation, available storage and practical I/O time, not the former small gameplay caps. Item container references use a signed 32-bit in-memory index to retain -1 and later logical indexes; the existing textual numeric field position is unchanged. No Item/Spell/Feature file migration is required.

The **Get Elevated** bundle is appended in one batch transaction. It never creates a sidecar for a character whose Inventory has not been generated. The legacy persisted key remains `ExtraItems=` for backward compatibility. Disabling Get Elevated affects only automatic/randomized bundle granting; **Homebrew** controls whether the 420 rows are visible in the Item catalog. Neither setting deletes already-owned Items. This setting adds no campaign variables and no character-specific ownership marker.

## Cross-FAP return metadata

Launch/return arguments are transient navigation metadata, not persisted character selection. DNDCombat launches Initiative with `from=combat` so Initiative Short Back can return to DNDCombat. Other Initiative launches return to DNDolphins. `custom_active_profile.txt` remains authoritative for the character ID in every case.
