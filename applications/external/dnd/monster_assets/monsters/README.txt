MONSTER PACK FORMAT

Bundled index rows use:
id|name|challenge rating in eighths|XP|AC|HP|type|environment|source|role

The packaged index.txt/statblocks.txt are read-only. User custom monsters use app-data monsters/custom_index.txt and monsters/custom_statblocks.txt.

If neither user custom file exists, Bestiary seeds them from default_custom_index.txt and default_custom_statblocks.txt. The seed contains Dolphin and Capybara and never overwrites or merges into existing/partial user custom data.

See MONSTER_PACK_SCHEMA.md for pack details.
