# Monster pack schema 1

Monster pack schema 1 is stable as of application version 2.0. Compatible releases accept an `index.txt` beginning with `# MonsterPack=1`, followed by pipe-delimited records:

`id|name|challenge_eighths|xp|armor_class|hit_points|type|environment|source|role`

Each ID maps to an `[id]` section in `statblocks.txt`. Required keys are `SizeAlignment`, `Speed`, `Abilities`, `Senses`, `Languages`, and `Actions`. Optional keys are `Skills`, `Defenses`, `Traits`, and `Extra`. Values are single-line UTF-8 text and should not exceed 191 bytes. `Abilities` contains six comma-separated integers in STR, DEX, CON, INT, WIS, CHA order.

IDs must remain stable and unique. New optional fields may be added without changing the schema. Any incompatible index or required-field change needs a new pack version and an explicit importer.

`source` is a short display/filter label. `role` is optional encounter metadata and accepts `Leader`, `Controller`, `Skirmisher`, `Artillery`, `Brute`, `Minion`, or `Any`. Older schema-1 records with eight fields remain valid and receive safe defaults.

On-device custom creation rewrites temporary index and stat-block files, then atomically publishes both files with rollback copies. Custom records retain source `Custom`; other records remain read-only.

Packaged and user records share `/ext/apps_assets/dolphin_bestiary/monsters/index.txt` and `statblocks.txt`. Temporary transaction files are read only during an interrupted write recovery and are removed after commit or rollback.
