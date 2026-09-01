# Save and storage schema

### Companion profile projections

The canonical character file and field names are unchanged. `dnd_profile_projection.c` is an in-memory access layer only:

- DNDInventory reads name/species/background/classes/abilities/AC/exhaustion/encumbrance/carry-capacity. Its only canonical write-back is transactional replacement of AC plus encumbrance/carry-capacity inside existing `Vitals=` / `CombatFlags=` lines. Currency and Items remain Inventory-sidecar owned.
- DNDSpellbook reads only name and class/subclass spellcasting metadata. It never writes the canonical character file.
- DNDAdventure reads only name, class levels, ability scores and skill proficiency/misc values. It never writes canonical character fields.

Unrecognized canonical fields are preserved byte-for-line during the Inventory-owned projection rewrite. The projection line bound matches canonical encoded character lines, and the publish step uses temporary/backup rename rollback.

## Ownership

Primary ownership is separated by app-data root, with intentional shared access to the DNDolphins character root:

- DNDolphins: `/ext/apps_data/dndolphins/` — canonical characters, active profile, character-owned collection/progression sidecars, exports/archive.
- DNDInventory: edits `/ext/apps_data/dndolphins/inventory_{id}.txt`; it does **not** create a separate `/ext/apps_data/dndinventory/` character store.
- DNDSpellbook: edits `/ext/apps_data/dndolphins/spellbook_{id}.txt`; it does **not** create a separate `/ext/apps_data/dndspellbook/` character store.
- DNDAdventure: `/ext/apps_data/dndadventure/` — active campaign, progress, direct custom campaigns and installed campaign registry/index. Adventure also updates the selected DNDolphins character-owned sidecars for rewards and writes milestone entries into that character's DNDJournal directory.
- DNDJournal: `/ext/apps_data/dndjournal/ch_{id}/` — per-character journal entries; Item entries may append to the shared DNDolphins Inventory sidecar.
- DNDInitiative: `/ext/apps_data/dndinitiative/ch_{id}.txt` — party/initiative state.
  - Stores RollMode and MainCharacterName alongside roster/combat fields so automatic roll behavior persists and the profile-backed main participant remains identifiable across character renames.
  - Turn/Encounter recharge reads/writes the shared DNDolphins Feature sidecar directly.
- DNDBestiary: `/ext/apps_data/dndbestiary/` — favorites, recents, filters, encounters, custom monsters and installed monster packs.



### Shared profile lookup

Companion FAPs resolve character profiles only from canonical DNDolphins files named `ch_{id}_{safeName}_{level}.txt`. Item/spellbook sidecars, work files, exports, shadows and Journal/Initiative files cannot satisfy a primary-profile lookup. Character ID `0` is valid.

`/ext/apps_data/dndolphins/custom_active_profile.txt` stores `Active=<id>` and is the only companion character-selection metadata. All seven FAPs use the shared `dnd_profile_handoff.c` reader/lookup contract for this metadata and exact canonical-profile references; launch arguments do not override the persisted selection and companions do not discover/fall forward to another character. Inventory, Spellbook and Adventure then stream only their required canonical fields through `dnd_profile_projection.c`; they do not keep `PocketCharacter` resident. Initiative and Bestiary choose ID `0` only when active-profile metadata itself is absent or unreadable. If `Active=<id>` is present but that character is missing, Initiative does not switch to 0 or another character; Bestiary keeps the persisted ID and remains usable without a character profile.

## DNDolphins

Primary character files are named `ch_{id}_{safeName}_{level}.txt`. Recognized fields load independently and unknown fields are ignored. Character `.shd` files are write-only history and are never used as live input.

Owned spells and items live in character-specific collection files named `spellbook_{id}.txt` and `inventory_{id}.txt`. The Inventory sidecar also owns currency through `Currency=cp,sp,ep,gp,pp`. The distinct prefixes keep collection files unambiguous from canonical `ch_{id}_{safeName}_{level}.txt` character profiles. Only one aligned page of up to eight records is resident at a time. Collection-wide operations stream the live collection file rather than allocating all records. Current-level `.swd` snapshots are write history; the live `.txt` collection files remain authoritative.

Currency is not part of the character-profile schema. Character-profile `Currency=` lines are ignored on load and are not written on save. The only authoritative persisted currency record is `Currency=cp,sp,ep,gp,pp` in `inventory_{id}.txt`. Other FAPs may create an item-only sidecar containing `DNDItems=1` and `I|...` rows without Currency; they must preserve the absence of Currency. When DNDInventory opens such an existing sidecar, it initializes the missing record to `Currency=0,0,0,0,0`.

Persistent Features live in `feats_{id}.txt`. DNDolphins loads at most eight Feature records for the active Feature page; Feature use/recharge changes rewrite that sidecar immediately. Initiative Turn/Encounter recharge also operates directly on this sidecar, so Feature state does not need to be embedded in the canonical character file.

Applied deterministic progression IDs live in `appliedgrants_{id}.txt`. They are scanned/streamed during progression checks and are not loaded as a resident array. Legacy embedded Feature/Grant rows in older character files are tolerated but ignored and never allocated or migrated. Existing current-format sidecars remain authoritative; when a sidecar is absent it is treated as empty and is created only on the first real Feature write or applied-grant mark.

Starting inventory is not a character-creation or first-open side effect. It is granted only through **Hold Up → Inventory Tools → Grant Initial Inventory**. The Items module requests class/species/background rows and writes both equipment and starting currency into the Inventory sidecar. A successful normal Short-OK grant writes `InitialInventory=1`, which remains even if all items are later deleted. Hold OK while that state is present may deliberately regrant once: existing Item records are preserved, the starting package/currency is appended again, and a successful publish rewrites the marker as `InitialInventory=2`. State `2` means the override is consumed and neither Short nor Hold OK may duplicate the grant again. Failed regrant writes do not consume the override. A currency-only sidecar created by manual Currency editing has no grant marker and can still receive the initial grant. A d100 trinket is **not** part of a successful normal seed; it is fallback-only when the selected starting assets yield neither items nor currency.

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

## Ritual Adept derivation

Combat → Rituals adds no persisted field. The list is derived from existing Spellbook records: the spell must be `Known`, have `Ritual=1`, be level 1 or higher, and resolve as a Wizard spell for the active character. Preparation state is intentionally ignored for this ritual list. Ritual casting consumes no slot/Pact/points/free-cast state, so no collection rewrite is required solely for the ritual action.

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
