# Community metadata pack

This folder is a build-time template for extending DNDolphins option metadata without distributing proprietary descriptive text. Runtime character shadows, eight-record owned spell/item paging, storage-backed profile paging, XP floors and Combat attack mode do not alter this row format.

Rows use:

`stable_id|source|option_type|option_name|prerequisites|level_gained|class_associations|grant_value`

Supported grant values include `origin_feat=`, `tool=`, `armor=`, `weapon=`, `size=`, `senses=`, `feature=`, and `spell=`. Leave the final field empty for catalog-only metadata.

Keep stable IDs durable once published. When included in a build, rows belong in DNDolphins character metadata assets; this template is not a runtime campaign or monster pack and should not be copied into DNDAdventure/DNDBestiary data folders.

Only distribute metadata/text you have permission to redistribute.
