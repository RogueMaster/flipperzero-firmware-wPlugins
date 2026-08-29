# Community metadata pack

This directory is a separately maintainable metadata index for Dungeons & Dolphins. It contains names and structured annotations, not descriptive rules text.

Each non-comment line appended to `character_assets/metadata/options.txt` uses:

`stable_id|source|option_type|option_name|prerequisites|level_gained|class_associations|grant_value`

Supported grant values include `origin_feat=`, `tool=`, `armor=`, `weapon=`, `size=`, `senses=`, `feature=`, and `spell=`. Leave the final field empty for a catalog-only annotation.

Keep IDs stable once published. The app streams the normal metadata file and does not allocate or scan a second overlay.

Users may populate additional records from campaign books they own. Do not distribute copied descriptive text through this pack.
