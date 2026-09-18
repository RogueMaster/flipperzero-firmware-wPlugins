# Flipper Apps Catalog submission

Specter is listed in the official [Flipper Apps Catalog][cat]. The catalog is
**pinned to v2.8** (`ecd52fd`) — it does not follow this repo, so a release here
does not reach catalog users until someone opens a bump PR there.

`manifest.yml` in this folder is the **prepared v3.0 bump**, ready to drop into
`applications/NFC/specter/manifest.yml` in a fork of the catalog repo. It is a
copy for convenience, not the source of truth — the live file is the one in the
catalog repo.

## Before it can be submitted: re-take the screenshots

The six images in `screenshots/` are **genuine qFlipper captures, and every one
of them shows the pre-3.0 UI** — `SPECTER` in the header, `SCANNING`, `PK100 C1`,
`READER PRESENT`, `DUTY`, `SEEN`, and the empty band in Watch's alarm that 3.0
fills with a strength bar. Shipping them with a 3.0 manifest would put the old
app in the store listing.

They cannot be regenerated from this repo: the mockup renderer in
`tools_gen_mockups.py` is pixel-accurate to the layout but draws with a desktop
font, so its output is visibly not a Flipper capture. These need a real device.

Capture each with **qFlipper → Screenshot** (it writes 128×64 PNGs; the existing
files are 512×256, i.e. 4×, which the catalog accepts — scale with nearest
neighbour, never smooth):

| File | Screen | State to get it into |
|---|---|---|
| `ss0.png` | Sweep | Resting on a live reader — meter pegged, `ACTIVE READER` / `PEGGED` bar |
| `ss0_2.png` | Sweep | Quiet room, nothing found yet — shows the `LEFT=cal hold OK=log` hint |
| `ss1.png` | Watch Mode | Reader present — inverted `ACTIVE READER` band **and the new strength bar** |
| `ss1_2.png` | Watch Mode | After a contact has passed — clock running, `QUIET NOW`, `OK=re-arm` |
| `ss2.png` | Fingerprint | Locked onto a polling reader — `POLLING`, `CONF`, `PER`/`BST`/`JIT`/`UP` |
| `ss2_2.png` | Fingerprint | Just after a short `OK` — the `LOGGED` flash in the header |

Drop the new files over the old ones in `screenshots/`, commit, and **update
`commit_sha` in `manifest.yml` to that commit** — the catalog resolves the
screenshot paths against the source repo at that exact sha, so it must be a
commit that already contains them. (It is currently set to `fa32666`, the v3.0
tag, which does **not** have them.)

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
