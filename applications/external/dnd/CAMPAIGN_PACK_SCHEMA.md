# DNDAdventure campaign pack schema

Campaign packs are owned exclusively by **DNDAdventure** (`dndadventure`). They are never parsed or stored by DNDolphins or DNDBestiary. Character shadowing, eight-record spell/item sidecar paging, storage-backed character/Journal paging and Combat draw optimizations do not alter the campaign pack format.

## Campaign index

Bundled and enabled campaign index rows use:

`stable_id|display_name|pack_version|min_app|max_app|entry_scene|scenes_file`

- `stable_id` must be filename-safe and stable.
- `pack_version` is `1` for this format.
- `0` as `max_app` means no declared maximum.
- `entry_scene` must reference an `S` record.
- `scenes_file` is relative to the campaign directory and installed packs currently use `scenes.txt`.

## Scene records

Scene files are line-oriented UTF-8:

`S|scene_id|title|body|sprite`

`C|scene_id|choice|skill_index_or_-1|dc|success|failure|reward_item|milestone|quest_flag|achievement`

IDs must be unique in scope. Success/failure must reference an existing scene or `-`. Text fields stay on one line and cannot contain `|`.

`quest_flag` and `achievement` use `0..31`; `255` means no guard. A choice that emits a milestone must have at least one guard. DNDAdventure saves the new guard in its own progress file before writing the milestone into DNDJournal so an already-granted branch is not intentionally journaled again.

## Writable layout

All mutable Adventure data is under `/ext/apps_data/dndadventure/`.

Important paths include:

- `campaigns/custom_index.txt` — direct custom campaign index.
- `campaigns/custom_{stable_id}/scenes.txt` — direct custom scene file.
- `campaigns/custom_progress_{characterId}_{campaignId}.txt` — per-character campaign progress.
- `campaigns/active_{characterId}.txt` — active campaign pointer.
- `packs/campaign_registry.txt` — installed campaign pack registry.
- `campaigns/enabled_index.txt` — currently enabled installed campaign rows.

Campaign progress, active campaign, registry and enabled index each use a **single canonical `.txt` path**. DNDAdventure does not create alternate `.tmp`/`.bak` generations for these writes. A failed write returns failure/status instead of restoring another Adventure copy.

No campaign identity, scene, checkpoint, flag, achievement or campaign-pack value is serialized into `PocketCharacter`.

## Installed campaign-pack inbox

Stage a pack at:

- `/ext/apps_data/dndadventure/packs/campaign_inbox/manifest.txt`
- `/ext/apps_data/dndadventure/packs/campaign_inbox/index.txt`
- `/ext/apps_data/dndadventure/packs/campaign_inbox/scenes.txt`

Manifest format:

```text
PocketPack=1
Id=filename_safe_pack_id
Name=Display Name
```

The inbox index must contain exactly one campaign row whose ID matches the manifest and whose scene filename is `scenes.txt`. No checksum is stored or required.

Before installation, DNDAdventure validates the manifest/index and rejects IDs already present in bundled, direct-custom or enabled campaigns. Installed files, registry and enabled index are then written directly into DNDAdventure app data.
