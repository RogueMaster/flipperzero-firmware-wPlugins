# DNDBestiary monster pack schema

Monster packs are owned only by **DNDBestiary** (`dndbestiary`). Campaign-pack handling is not compiled into Bestiary. Eight-record character spell/item paging, Journal paging and Combat changes do not alter the monster pack format.

## Inbox

Stage a runtime pack under:

`/ext/apps_data/dndbestiary/packs/monster_inbox/`

with:

- `manifest.txt`
- `index.txt`
- `statblocks.txt`

Manifest format:

```text
PocketPack=1
Id=filename_safe_pack_id
Name=Display Name
```

## Index row

`id|name|challenge rating in eighths|XP|AC|HP|type|environment|source|role`

Each ID needs a matching `[id]` section in `statblocks.txt`. Common keys include `SizeAlignment`, `Speed`, `Abilities`, `Initiative`, `Skills`, `Defenses`, `Senses`, `Languages`, `Traits`, `Actions`, and `Extra`.

IDs must be unique and filename-safe. Keep values on one line and inside parser limits. No checksum is required; text files are intentionally editable.

Installed monster files, the registry and enabled index remain under DNDBestiary app data. Pack operations must not write campaign or character namespaces.
## Runtime loading note

Monster pack/index/statblock text is consumed incrementally; the storage schema does not require whole-file loading or persistent full-file offsets. Current lookup caches are an implementation detail and may be replaced by bounded/sparse caches without changing this pack format.
