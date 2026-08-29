# Campaign pack schema 1

Campaign packs are listed in `campaigns/index.txt`. Each non-comment line is:

`stable_id|display_name|pack_version|min_app|max_app|entry_scene|scenes_file`

- `stable_id` is filename-safe and never changes after publication.
- `pack_version` is `1` for this format.
- App compatibility uses a three-digit value: 2.3 is `203`; `0` as `max_app` means no declared maximum.
- `entry_scene` must identify an `S` record in the named scene file.
- `scenes_file` is relative to `campaigns/{stable_id}/`.

Scene files are line-oriented UTF-8:

`S|scene_id|title|body|sprite`

`C|scene_id|choice|skill_index_or_-1|dc|success|failure|reward_item|milestone|quest_flag|achievement`

IDs must be unique inside their scopes. Every success and failure link must identify an existing scene or `-`. Text fields must remain on one line and cannot contain `|`. The on-device diagnostics report incompatible manifests, missing files or entry scenes, duplicate IDs, and broken links.

User packs belong under `/ext/apps_data/dungeons_and_dolphins/campaigns/`. Add their manifest records to `custom_index.txt`; the app streams that persistent user index after the bundled read-only asset index. Prefix user-maintained campaign directories with `custom_`. Progress and checkpoints are stored in the same app-data campaign directory, separately for each character profile and campaign ID.

## Installed packs

Version 3.1 accepts one staged campaign pack from the Dungeons & Dolphins app-data inbox:

- `packs/campaign_inbox/manifest.txt`
- `packs/campaign_inbox/index.txt`
- `packs/campaign_inbox/scenes.txt`

The manifest uses the same `PocketPack=1`, `Id`, and `Name` fields documented for monster packs. No checksum is stored or required. The index must contain exactly one schema-1 campaign row whose stable ID matches the manifest and whose scene filename is `scenes.txt`.

Installation structurally validates both files before publishing, rejects IDs already present in bundled, direct-custom, or enabled installed indexes, and registers the pack transactionally. Enable/disable rebuilds only `APP_DATA_PATH("campaigns/enabled_index.txt")`; bundled manifests and scenes remain exclusively in `APP_ASSETS_PATH`.
