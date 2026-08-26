# Save schema 2

Schema 2 is the current pre-1.0 text-save contract. Character filenames remain `ch_{id}_{characterName}_{characterLevel}.txt` and each file contains the entire independent profile.

## Envelope

- The first record is `PocketD20Character=2`.
- Records use ordered `key=value` lines.
- Text uses percent escaping for control bytes, line breaks, carriage returns, and `%`.
- Repeated collections use a count record followed by indexed records.
- `End=OK` closes the canonical payload.
- `FileChecksum` is FNV-1a over every byte from the first record through the newline after `End=OK`; the checksum line itself is excluded.

## Compatibility

- Readers reject unknown schema numbers rather than guessing field meanings.
- Pre-freeze and unknown schema numbers are rejected; this pre-release build intentionally contains no migration reader.
- Schema 2 files are validated before their ordered records are parsed.
- A failed primary load attempts the temporary backup before a fresh character is created.
- Migration policy can be reconsidered when a future stable schema is established.

## Data groups

The ordered payload covers identity and builder fields; adventure state; multiclass and spellcasting data; abilities, saves, skills, and vitals; currency; spells; features and resources; inventory and weapons; languages; journal and milestones; party presets including HP and AC; active initiative; combat-sheet state; structured grants; attack templates; and encounter history.

## Transfer folders

- `exports/` contains user-transferable complete character files.
- `archive/` contains profiles removed from the active list without deletion.
- Import reads the first valid exported text file, assigns a new profile ID, and writes it through the normal atomic-save path.
