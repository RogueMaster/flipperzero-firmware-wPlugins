# Community monster pack template

Use `monsters/index_rows.txt` for index rows and add matching `[id]` sections to `monsters/statblock_sections.txt` when preparing content for a pack.

Index format:

`id|name|CR_eighths|XP|AC|HP|type|environment|source|role`

Keep IDs stable, text within parser limits and all required statblock fields present. Runtime custom monsters belong under DNDBestiary app data; do not overwrite a user's custom pack merely to add defaults.
