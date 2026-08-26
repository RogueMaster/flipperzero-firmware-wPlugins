MONSTER PACK FORMAT

The bundled index contains redistributable reference records. Each index row is:
id|name|challenge rating in eighths|XP|AC|HP|type|environment|source|role

Examples: CR 1/4 is 2, CR 1/2 is 4, CR 1 is 8, and CR 10 is 80.
Append optional user summaries to `index.txt` with source `Custom`, and add their matching
`[stable_id]` key/value section to `statblocks.txt`. The on-device editor performs these updates
atomically and protects records whose source is not `Custom`.

The app scans the index on demand, pages 50 summaries at a time, and streams only the selected section
from the shared stat-block file.
