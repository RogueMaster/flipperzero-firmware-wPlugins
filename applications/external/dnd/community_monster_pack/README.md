# Community monster record template

Append the sample summary row to `/ext/apps_assets/dolphin_bestiary/monsters/index.txt`, then append the matching `[stable_id]` section to `statblocks.txt`. The app streams those two normal pack files and identifies editable records by source `Custom`.

Add one index row per creature using:

`id|name|challenge rating in eighths|XP|AC|HP|type|environment|source|role`

IDs must be unique and filename-safe. A matching section can contain `SizeAlignment`, `Speed`, `Abilities`, `Skills`, `Defenses`, `Senses`, `Languages`, `Traits`, `Actions`, and `Extra` key/value lines. Keep each value on one line and within 191 characters.

Use only material you have permission to redistribute. The on-device Pack Diagnostics screen reports missing sections and duplicate IDs.
