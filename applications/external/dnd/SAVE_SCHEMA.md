# Save schema 2

Schema 2 is the current compatibility baseline. Character filenames remain `ch_{id}_{characterName}_{characterLevel}.txt` and each file contains the entire independent profile.

## Envelope

- The first record is `PocketD20Character=2`.
- Records use ordered `key=value` lines.
- Text uses percent escaping for control bytes, line breaks, carriage returns, and `%`.
- Repeated collections use a count record followed by indexed records.
- `End=OK` closes the canonical payload.
- `FileChecksum` is FNV-1a over every byte from the first record through the newline after `End=OK`; the checksum line itself is excluded.

## Compatibility

- Readers dispatch by explicit schema number and reject unknown versions rather than guessing field meanings.
- Schema 2 is the oldest supported version in 3.0.2. Future releases must retain its reader or provide a validated, non-destructive migration before publishing a newer schema.
- Schema 2 files are validated before their ordered records are parsed.
- A failed primary load attempts the retained backup. If neither validates and a profile file exists, the app preserves every file and refuses to autosave a blank replacement.
- A fresh character is created only when no profile files exist.
- Legacy asset-path profiles are relocated into persistent app data only when the destination file and profile ID are absent. Existing app-data files are authoritative and never overwritten; the legacy source is removed only after the destination is safely present.

## Data groups

The ordered payload covers identity and builder fields; adventure state; multiclass and spellcasting data; abilities, saves, skills, and vitals; currency; spells; features and resources; inventory and weapons; languages; journal and milestones; party presets including HP and AC; active initiative; combat-sheet state; structured grants; attack templates; and encounter history.

## Transfer folders

- `exports/` contains user-transferable complete character files.
- `archive/` contains profiles removed from the active list without deletion.
- Import reads the first valid exported text file, assigns a new profile ID, and writes it through the normal atomic-save path.
