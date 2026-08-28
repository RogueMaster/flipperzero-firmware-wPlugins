# Monster pack schema 1

Monster pack schema 1 is stable as of application version 2.0. Compatible releases accept an `index.txt` beginning with `# MonsterPack=1`, followed by pipe-delimited records:

`id|name|challenge_eighths|xp|armor_class|hit_points|type|environment|source|role`

Each ID maps to an `[id]` section in `statblocks.txt`. Required keys are `SizeAlignment`, `Speed`, `Abilities`, `Senses`, `Languages`, and `Actions`. Optional keys are `Skills`, `Defenses`, `Traits`, and `Extra`. Values are single-line UTF-8 text and should not exceed 191 bytes. `Abilities` contains six comma-separated integers in STR, DEX, CON, INT, WIS, CHA order.

IDs must remain stable and unique. New optional fields may be added without changing the schema. Any incompatible index or required-field change needs a new pack version and an explicit importer.

`source` is a short display/filter label. `role` is optional encounter metadata and accepts `Leader`, `Controller`, `Skirmisher`, `Artillery`, `Brute`, `Minion`, or `Any`. Older schema-1 records with eight fields remain valid and receive safe defaults.

On-device custom creation rewrites temporary index and stat-block files, then atomically publishes both files with rollback copies. Custom records retain source `Custom`; other records remain read-only.

Packaged records use the read-only `/ext/apps_assets/dolphin_bestiary/monsters/index.txt` and `statblocks.txt`. User records use `/ext/apps_data/dolphin_bestiary/monsters/custom_index.txt` and `custom_statblocks.txt`; the app streams both layers as one Bestiary. Temporary transaction files apply only to the custom app-data pair and are removed after commit or rollback.

## Installed packs

Version 3.1 adds a separate installed-pack inbox beneath the Dolphin Bestiary app-data directory:

- `packs/monster_inbox/manifest.txt`
- `packs/monster_inbox/index.txt`
- `packs/monster_inbox/statblocks.txt`

The manifest is:

```text
PocketPack=1
Id=filename_safe_pack_id
Name=Display Name
```

No checksum is stored or required, so pack text remains manually editable. Installed index rows must use source `Custom Pack`. Installation rejects malformed rows, unsafe pack IDs, duplicate record IDs, and IDs already present in packaged, directly created custom, or enabled installed records.

Files are copied and published transactionally in app data, registered through a plain-text registry, and streamed through `enabled_index.txt` and `enabled_statblocks.txt`. Enable/disable rebuilds only those app-data aggregates. It never opens packaged assets for writing and never modifies the direct-custom pair.
