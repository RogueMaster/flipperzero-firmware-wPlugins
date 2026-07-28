# Change Files

Each `.md` file in this directory describes one pending change that will be
included in the next release.

## Format

```
---
kind: added       # added | changed | deprecated | removed | fixed | security | safety | chore
breaking: false   # optional; true forces a major semver bump
mandatory: false  # optional; true means this version must be installed before newer ones
---
Technical title (required, one line — appears in CHANGELOG and GitHub Release)

## Release Note
User-facing title line (appears in release.json)
Additional description lines shown in the GitHub Release and release.json.

## Notices
- warning: something users must know before upgrading
- info: optional note or migration step
```

## Semver derivation

- `breaking: true` -> major bump
- `kind: fixed|security|safety|chore` -> patch bump
- everything else -> minor bump

## File naming

Name the file after the change (e.g. `add-user-auth.md`).
Run `release-tool new "<title>" --kind added` to generate one automatically.

## Special files

- `_headline.md` - optional release headline shown at the top of the GitHub Release body
