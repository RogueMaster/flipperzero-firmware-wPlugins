# Compatibility matrix

| Component | Status | Notes |
|---|---|---|
| RogueMaster firmware | Primary target | Full build/device verification should use the target firmware tree |
| Momentum firmware | Not verified | API/manifest differences may require changes |
| SD card | Required for persistent user data | Bundled data uses FAP assets; mutable data uses app-data paths |
| Character schema 5 | Supported | Current writer; core fields remain best-effort by field name |
| Character spellbook/items sidecars | Supported | `ch_{id}_spellbook.txt` / `ch_{id}_items.txt`; one escaped record per line; at most eight owned records are cached at a time |
| Character `.shd` shadows | Supported | Same-stem level/name history under `/ext/apps_data/dndolphins/`; hidden from live picker and never removed by profile delete/archive |
| Storage-backed character picker | Supported | Fixed eight-record metadata cache; directory scans provide additional pages |
| Older/future/mismatched character saves | Best-effort load | Recognized fields load by name regardless of version/order; unknown/malformed fields are skipped and valid filename metadata supplies name/level fallback |
| Campaign pack schema 1 | Supported by DNDAdventure | Adventure-owned only |
| Journal named-field format | Supported by DNDJournal | Compact legacy metadata rows are not loaded |
| Initiative named-field format | Supported by DNDInitiative | Streaming loader; packed legacy participant/combat rows are not loaded |
| Monster pack schema 1 | Supported by DNDBestiary | Bestiary-owned only; 3.2.15 changes only volatile sparse/recent lookup acceleration and the 15-summary browse working set, not monster text formats |

The shared manifest builds `fap_dndolphins`, `fap_dndadventure`, `fap_dndjournal`, `fap_dndinitiative`, and `fap_dndbestiary`.

## Character relocation

At DNDolphins startup, only `/ext/apps_data/dungeons_and_dolphins/profiles/ch*.txt` character files are moved into `/ext/apps_data/dndolphins/` when no current primary character with that character ID already exists. The move is content-blind: the file is not schema-checked or rewritten before the tolerant field-name loader reads it. No other legacy app data is moved.

## Direct companion launch

DNDJournal, DNDAdventure and DNDInitiative accept an explicit decimal character ID from a handoff. Without one, they read DNDolphins' persisted active character reference and fall back to character 0 when that metadata is unavailable or invalid. DNDBestiary does not require a character reference to launch, but DNDolphins passes the active character ID when available so Bestiary-to-Initiative transfers target the same character.

## Spell/item compatibility boundary

Owned spell and item data is current only when present in the character-specific sidecar files. The core schema-5 parser does not read, reconstruct, or migrate pre-sidecar `Spell*`, `Item*`, `SpellCount`, or `ItemCount` fields. If a current sidecar is absent, that owned collection is empty. Master spell/item catalogs are not a recovery source for already-owned records.

Sidecar line order is non-authoritative: records can be inserted, removed, or reordered without renumbering serialized variables. The implementation does not persist line numbers as record identity. Malformed record lines are ignored for logical paging and preserved when another valid page is rewritten.
