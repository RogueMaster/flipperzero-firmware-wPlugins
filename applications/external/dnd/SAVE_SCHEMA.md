# Save and storage schema

## Ownership

Primary ownership is separated by app-data root, with two intentional cross-app bridges:

- DNDolphins: `/ext/apps_data/dndolphins/` — characters, active profile, spell/item sidecars, exports/archive.
- DNDAdventure: `/ext/apps_data/dndadventure/` — active campaign, progress, direct custom campaigns and installed campaign registry/index. Adventure also updates the selected DNDolphins character for rewards and writes milestone entries into that character's DNDJournal directory.
- DNDJournal: `/ext/apps_data/dndjournal/ch_{id}/` — per-character journal entries.
- DNDInitiative: `/ext/apps_data/dndinitiative/ch_{id}.txt` — party/initiative state.
  - Stores RollMode and MainCharacterName alongside roster/combat fields so automatic roll behavior persists and the profile-backed main participant remains identifiable across character renames.
- DNDBestiary: `/ext/apps_data/dndbestiary/` — favorites, recents, filters, encounters, custom monsters and installed monster packs.



### Shared profile lookup

Companion FAPs resolve character profiles only from canonical DNDolphins files named `ch_{id}_{safeName}_{level}.txt`. Item/spellbook sidecars, work files, exports, shadows and Journal/Initiative files cannot satisfy a primary-profile lookup. Character ID `0` is valid.

`/ext/apps_data/dndolphins/custom_active_profile.txt` is read best-effort by field name. If its `Active` ID is missing or stale, companion apps fall forward to the next canonical character ID and wrap to the first canonical character, matching DNDolphins profile-selection behavior. Explicit handoff IDs are validated before use; a stale explicit ID falls back through the same resolver rather than creating orphan per-character state.

## DNDolphins

Primary character files are named `ch_{id}_{safeName}_{level}.txt`. Recognized fields load independently and unknown fields are ignored. Character `.shd` files are write-only history and are never used as live input.

Owned spells and items live in character-specific collection files named `spellbook_{id}.txt` and `inventory_{id}.txt`. The distinct prefixes keep collection files unambiguous from canonical `ch_{id}_{safeName}_{level}.txt` character profiles. Only one aligned page of up to eight records is resident at a time. Collection-wide operations stream the live collection file rather than allocating all records. Current-level `.swd` snapshots are write history; the live `.txt` collection files remain authoritative.

Starting inventory is not a character-creation side effect. On first Inventory open, if no live item sidecar exists, the Items module requests class/species/background rows plus one hidden d100 trinket and applies any starting currency once.

## Adventure

Campaign progress is DNDAdventure-owned and stores campaign ID, scene/checkpoint, quest flags and achievements in its own text files. Milestones are journaled after the corresponding guard is saved so replaying a guarded reward does not intentionally duplicate it.

## Bestiary custom monsters

Custom monsters use:

- `/ext/apps_data/dndbestiary/monsters/custom_index.txt`
- `/ext/apps_data/dndbestiary/monsters/custom_statblocks.txt`

If neither exists, Bestiary may seed both from bundled default-custom assets. If either user file already exists, the seed does nothing. Existing recovery/transaction logic remains authoritative for user custom edits.

No campaign, Bestiary, Journal or Initiative state is serialized into the core character file.

### Initiative participant roll mode

DNDInitiative owns its `ch_{characterId}.txt` sidecar. Participant rows may include `RosterN RollMode` / `CombatN RollMode`-style named fields (serialized without spaces as `RosterNRollMode` and `CombatNRollMode`) using 0=Normal, 1=Advantage, 2=Disadvantage. Loading remains best-effort by field name, so older Initiative files without these fields default normally and remain readable.
