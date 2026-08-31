# DNDBestiary monster pack schema

## Index row

`id|name|CR_eighths|XP|AC|HP|type|environment|source|role`

Each ID needs a matching `[id]` section in the statblock file. Direct custom-monster rows use source `Custom`; installed-pack rows are normalized by the pack system. Supported named fields include `SizeAlignment`, `Speed`, `Abilities`, `Initiative`, `Skills`, `Defenses`, `Senses`, `Languages`, `Traits`, `Actions` and `Extra`.

## Runtime pack inbox

Stage under `/ext/apps_data/dndbestiary/packs/monster_inbox/`:

- `manifest.txt`
- `index.txt`
- `statblocks.txt`

Manifest:

```text
PocketPack=1
Id=filename_safe_pack_id
Name=Display Name
```

No checksum is required.

## Default custom seed

Bundled assets include a small default custom index/statblock pair for Dolphin and Capybara. On startup it is copied to the normal custom-monster app-data files only when neither custom file already exists. It is not merged into or allowed to overwrite an existing user pack.
