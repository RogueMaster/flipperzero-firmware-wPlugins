# Flipper Apps Catalog submission

Specter is listed in the official [Flipper Apps Catalog][cat]. The catalog is
**pinned to v2.8** (`ecd52fd`) — it does not follow this repo, so a release here
does not reach catalog users until someone opens a bump PR there.

`manifest.yml` in this folder is the **prepared v3.0 bump**, ready to drop into
`applications/NFC/specter/manifest.yml` in a fork of the catalog repo. It is a
copy for convenience, not the source of truth — the live file is the one in the
catalog repo.

## Screenshot status (verified against the source, not eyeballed)

The six images in `screenshots/` are genuine qFlipper captures. They were taken
against v3.0, and v3.0.1 then moved two screens by a row or two, so they are
not all current. Each was checked by downsampling the 512x256 capture back to
128x64 device pixels and comparing the inked rows against the constants in the
view sources:

| File | Screen | Status |
|---|---|---|
| `ss0.png` | Sweep, on a reader, meter pegged | **current** |
| `ss0_2.png` | Sweep, quiet room, key hint showing | **current** |
| `ss1.png` | Watch, reader present + strength bar | **current** |
| `ss1_2.png` | Watch, after a contact, clock running | **current** |
| `ss2.png` | Fingerprint, POLLING | **stale** — divider is on row 50, code now says 51 |
| `ss2_2.png` | Fingerprint, INTERMITTENT + LOGGED flash | **stale** — same row shift |
| `ss3.png` | Logbook | **stale** — shows the mid-word wrap fixed in 3.0.1 |
| `ss4.png` | Site Survey verdict | **do not ship** — shows `SURVEY 1s / CLEAN`, the overclaim 3.0.1 replaced with `TOO SHORT` |

So before submitting, re-take **`ss2` and `ss2_2`** (Fingerprint: the stat rows
are a row further apart now, and the confidence bar moved up one). `ss3` is
worth re-taking too — the logbook now wraps at words instead of splitting them,
which looks considerably better and is worth showing. `ss4` should either be
re-taken as a real full-length survey or left out; it currently advertises a
bug.

Capture with **qFlipper -> Screenshot** (it writes 128x64 PNGs; the existing
files are 512x256, i.e. 4x, which the catalog accepts — scale with nearest
neighbour, never smooth).

Whichever set you settle on, **update `commit_sha` in `manifest.yml` to the
commit that contains them** — the catalog resolves the screenshot paths against
this repo at that exact sha.

## Submitting

1. Fork `flipperdevices/flipper-application-catalog`.
2. Copy this `manifest.yml` over `applications/NFC/specter/manifest.yml`.
3. Validate locally before opening anything:
   `python3 tools/bundle.py --nolint applications/NFC/specter/manifest.yml bundle.zip`
4. Open the PR against `main`.

Use the **mobile app or [lab.flipper.net][lab]** to check how the listing
renders — not qFlipper, which does not show catalog pages.

### Sanitizer rules that have bitten before

The catalog runs its own markdown sanitizer over `description` and `changelog`:

- **No backticks** — no inline code, no fenced blocks. Write `Meter scale` as
  plain words, not as code. (This repo's own changelog is full of them, so the
  catalog changelog is hand-written rather than copied.)
- **No images** in either field.
- Links are fine; keep the "full version history" link at the bottom.

[cat]: https://github.com/flipperdevices/flipper-application-catalog
[lab]: https://lab.flipper.net/apps
