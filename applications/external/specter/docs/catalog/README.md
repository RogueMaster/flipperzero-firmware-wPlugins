# Flipper Apps Catalog submission

Specter is listed in the official [Flipper Apps Catalog][cat]. The catalog is
**pinned to v2.8** (`ecd52fd`) — it does not follow this repo, so a release here
does not reach catalog users until someone opens a bump PR there.

`manifest.yml` in this folder is the **prepared v3.1 bump**, ready to drop into
`applications/NFC/specter/manifest.yml` in a fork of the catalog repo. It is a
copy for convenience, not the source of truth — the live file is the one in the
catalog repo.

## Screenshots

**Every image is captured from a real device by `tools_screenshot.py`**, over
the Flipper's own RPC session, at 512x256 (4x the 128x64 panel) in the panel's
own two colours. The catalog accepts 4x. There is no longer any manual step
here and no qFlipper involved:

```bash
python3 tools_screenshot.py --all       # every screen that stands on its own
python3 tools_screenshot.py --reader    # the ones needing a live reader held on it
python3 tools_screenshot.py --catalog   # refresh the ssN.png aliases below
python3 tools_screenshot.py --verify    # assert every file is two colours
```

The six the manifest points at are **aliases**, refreshed by `--catalog` from
the semantically-named captures. The mapping lives in `CATALOG` in
`tools_screenshot.py`, so the manifest never has to be re-pointed when a
capture is retaken — only refreshed:

| Alias | From | Screen |
|---|---|---|
| `ss0.png` | `sweep_reader` | Sweep, reader locked on |
| `ss0_2.png` | `sweep_idle` | Sweep, quiet room, key hint showing |
| `ss1.png` | `watch_reader` | Watch, reader present + strength bar |
| `ss1_2.png` | `watch_quiet` | Watch, standing guard |
| `ss2.png` | `fingerprint_reader` | Fingerprint, POLLING, full carrier trace |
| `ss2_2.png` | `survey_done` | Site Survey verdict card |

Two of those carry a constraint worth knowing before you retake them:

- **`ss2` must be a Fingerprint screen with a reader actually on it.** The
  banner reads its entire signature waveform back out of that file
  (`tools_brand_data.carrier_from_capture`), so an idle capture turns the
  brand's one measured element into a flat line. `--reader` scores every frame
  by how many polls it caught and keeps the richest, because a reader polls in
  bursts and whichever frame the shutter happens to land on otherwise decides
  it. When you retake it, update `DEVICE_UP_PCT` and `DEVICE_PERIOD_MS` in
  `tools_gen_banner.py` to whatever that capture prints — the renderer asserts
  the two agree within a few points and refuses to build if they drift.
- **`ss2_2` must be a survey that ran long enough.** Under
  `SPECTER_SURVEY_MIN_CLEAN_MS` the verdict is `TOO SHORT`, which is correct
  behaviour but a poor shop window.

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
