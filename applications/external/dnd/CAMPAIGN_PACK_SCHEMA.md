# DNDAdventure campaign pack schema

Campaigns are declarative DNDAdventure data.

## Index

`stable_id|display_name|pack_version|min_app|max_app|entry_scene|scenes_file`

`0` for `max_app` means no declared maximum. IDs must be filename-safe and stable.

## Scene file

`S|scene_id|title|body|sprite`

`C|scene_id|choice|skill_index_or_-1|dc|success|failure|reward_item|milestone|quest_flag|achievement`

- A scene supports up to four choices.
- `skill_index=-1` with `dc=0` is an automatic choice.
- Success/failure targets must name valid scenes.
- `quest_flag` and `achievement` use `0..31`; `255` means none.
- A milestone choice must be guarded by a flag or achievement to avoid intentional duplicate Journal milestone entries.
- Text is one-line UTF-8 and cannot contain `|`.

Bundled **Ghost Protocol** demonstrates Investigation, Arcana, History, Insight, Perception, Persuasion, Sleight of Hand, Stealth and Survival-style branching using this same format. It is fiction and does not encode real device attack steps.

## Runtime pack inbox

Stage:

- `packs/campaign_inbox/manifest.txt`
- `packs/campaign_inbox/index.txt`
- `packs/campaign_inbox/scenes.txt`

Manifest:

```text
PocketPack=1
Id=filename_safe_pack_id
Name=Display Name
```

No checksum is required. Installed content and enabled indexes stay under DNDAdventure app data.
