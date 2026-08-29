# Community monster pack template

For runtime third-party monsters, use the **DNDBestiary monster-pack inbox** documented in `../MONSTER_PACK_SCHEMA.md`; do not modify packaged `/ext/apps_assets/` files on device. Eight-record character spell/item paging, Journal paging, shadows, XP floors and Combat draw behavior do not change this template.

A pack index row uses:

`id|name|challenge rating in eighths|XP|AC|HP|type|environment|source|role`

Each ID needs a matching `[id]` section in `statblocks.txt`. Common keys include `SizeAlignment`, `Speed`, `Abilities`, `Initiative`, `Skills`, `Defenses`, `Senses`, `Languages`, `Traits`, `Actions`, and `Extra`.

IDs must be unique and filename-safe. Keep values on one line and within the parser's documented limits. Use only material you have permission to redistribute.
