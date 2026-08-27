MONSTER PACK FORMAT

The bundled index contains redistributable reference records. Each index row is:
id|name|challenge rating in eighths|XP|AC|HP|type|environment|source|role

Examples: CR 1/4 is 2, CR 1/2 is 4, CR 1 is 8, and CR 10 is 80.
The packaged `index.txt` and `statblocks.txt` files are read-only. The on-device editor stores user
summaries in persistent app-data `monsters/custom_index.txt` and their matching `[stable_id]`
sections in `monsters/custom_statblocks.txt`. Those two user files are published atomically.

The app streams packaged and custom indexes as one result set, pages 20 summaries at a time, and
loads only the selected section from the matching stat-block file.
