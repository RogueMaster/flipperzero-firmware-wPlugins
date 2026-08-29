# Save and storage schema

## DNDolphins characters

The current character writer emits schema 5 (`POCKET_D20_SAVE_VERSION = 5`). Core character loading remains version-tolerant and best-effort by recognized field name. Character files live directly under:

`/ext/apps_data/dndolphins/`

Primary filename:

`ch_{characterId}_{characterName}_{characterLevel}.txt`

Shadow filename:

`ch_{characterId}_{characterName}_{characterLevel}.shd`

Owned spells and items are no longer serialized into the core character file. Their canonical per-character files are:

`ch_{characterId}_spellbook.txt`

`ch_{characterId}_items.txt`

The spellbook and item files are line-oriented collections. They do not use `Spell0...SpellN`, `Item0...ItemN`, or stored count fields, so inserting/removing/reordering records does not renumber serialized variables. The count is derived from valid record lines. String fields are percent-escaped for reserved delimiters/control characters. Runtime page starts/line positions are never persisted as record identity. Malformed `S|`/`I|` lines are preserved during sidecar rewrites but do not count toward logical record/page indexes, so one bad manual line does not shift which later valid record is edited.

Spellbook records use:

`S|name|detail|stable_id|source|school|grant_name|level,class,prepared,ritual,known,always_prepared,free_current,free_max,grant_source`

Item records use:

`I|name|detail|ammo_group|quantity,weight,equipped,attuned,is_weapon,attack_ability,proficient,magic_bonus,damage_dice,damage_die,versatile_die,use_versatile,damage_type,add_ability_damage,extra_dice,extra_die,weapon_properties,ammo_current,ammo_max,container_index,charges_current,charges_max,armor_base,armor_dex_cap,shield_bonus`

DNDolphins loads the core character without allocating the spellbook or item collection. Sidecar access uses `POCKET_D20_COLLECTION_CACHE_SIZE = 8`: only one aligned page of at most eight owned spells or items is resident. Crossing a page boundary saves the current page if changed, frees it, and scans the sidecar for the requested page. The temporary 1,280-byte read-line buffer is freed after each scan. Operations that need collection-wide results iterate through these bounded pages rather than allocating all 24 records. Master spell/item catalogs remain selection sources for adding records; already-owned records are read from the character-specific sidecars.

There is intentionally **no migration or fallback loader for embedded old `Spell*` or `Item*` character fields**. Those fields are ignored. A subsequent core character save omits them. Missing current-format sidecars mean an empty owned collection.

Sidecar publication may use collection-specific `.tmp` / `.bak` working files internally; the authoritative user records remain the `.txt` sidecars. Working `.tmp` / `.bak` files are not alternate owned-record sources.

Duplicate, delete, archive, export, and import operations treat the core character plus its spellbook/items sidecars as one logical character set. Export companions are named `export_ch_{characterId}_spellbook.txt` and `export_ch_{characterId}_items.txt`.

The `.shd` file uses the same stem as the primary character and is maintained only when logical core character state actually changes. Spellbook/item-only changes do not create a character shadow. Before publishing a changed primary, DNDolphins attempts to refresh the existing level/name shadow from the old primary. Shadow maintenance is non-fatal: `.shd` failure never invalidates a successful `.txt` save. Earlier shadows are never deleted by profile delete/archive operations. `.shd` files are write-only history: they are never scanned, loaded, parsed, used for recovery, or used to reserve profile IDs.

The live character picker ignores `.shd` records and keeps only a fixed eight-record metadata cache. Additional character pages are discovered by rescanning the root and selecting the next/previous IDs; the full character directory is not copied into a growable RAM array.

`custom_active_profile.txt` stores the selected character ID in an `Active=` field. The reader scans by field name and ignores unknown/reordered/malformed unrelated fields. If that ID has no current primary character, DNDolphins selects the next available primary character ID and wraps. DNDJournal, DNDAdventure and DNDInitiative use this persisted ID only when opened without an explicit handoff argument; if no valid `Active=` value is available they use character 0. `exports/` and `archive/` remain dedicated subdirectories.

The existing character-file relocation from `/ext/apps_data/dungeons_and_dolphins/profiles/` remains limited to primary `ch*.txt` character files. It does not perform spell/item schema migration. The core character loader applies every recognized core field it can regardless of file version/order; unknown fields and malformed individual values are skipped.

## DNDAdventure

All mutable campaign state is owned by:

`/ext/apps_data/dndadventure/`

Campaign identity, active scene/checkpoint, flags, achievements and campaign-pack values never appear in `PocketCharacter`.

Active campaign, campaign progress, campaign registry and enabled campaign index each use one canonical writable destination. Adventure intentionally does not maintain alternate `.tmp`/`.bak` save generations for those files. Campaign/scene text is read incrementally with bounded reader and line buffers; Adventure does not load an entire scene file into RAM just to locate a scene.

## DNDJournal

Journal entries are individual files under:

`/ext/apps_data/dndjournal/ch_{characterId}/`

Filename:

`ch_{characterId}_{YYYYMMDD}_{HHMMSS}_{suffix}.txt`

Each entry uses independent named fields for character ID, encoded title/body, category/completion metadata and an end marker. Named fields may appear in any order; unknown or malformed individual fields are skipped. The old compact metadata row is intentionally not loaded. Timestamp filenames provide newest-first ordering.

DNDJournal does not load the full journal into RAM. It scans timestamp filenames to select an eight-record page, opens only those selected files to hydrate metadata, and loads the body only for the opened entry. The entry count is therefore not limited by the former full-entry array size; practical limits are storage capacity, directory-scan latency and the 16-bit list index.

## DNDInitiative

Initiative/party state remains under:

`/ext/apps_data/dndinitiative/`

It owns party participants, combat order, rounds, HP/AC, conditions and turn state. None of this is serialized into the character payload. New saves use indexed named fields (`Roster0Name`, `Roster0HpCurrent`, `Combat0Total`, etc.). Loading is streaming and best-effort by field name; the old packed `|` participant/combat rows are intentionally not loaded.

## DNDBestiary

Monster/encounter state remains under:

`/ext/apps_data/dndbestiary/`

Custom monsters, favorites, recents, encounters and monster-pack registry/index files are Bestiary-owned. Campaign pack state does not appear here.
