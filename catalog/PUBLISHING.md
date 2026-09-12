# Publishing BEEPBACK to the Flipper Apps Catalog

Everything in this repository is ready except the screenshots, which have to
come off a real device, and the commit SHA, which cannot exist until the
screenshots are committed. This is the order to do it in.

## 1. The screenshots

They are already in `screenshots/`, rendered rather than photographed.
`tools/shoot/shoot.sh` sets up the firmware's own copy of u8g2 with the two
fonts `canvas_set_font()` picks, points it at a 128x64 buffer, and calls the
app's real `bb_draw()`. The canvas layer it draws through is copied from
`applications/services/gui/canvas.c`, alignment arithmetic included. So the
content is what the device shows, pixel for pixel.

**They are not qFlipper captures, and the catalog asks for qFlipper captures.**
Its bundler only checks that a screenshot is exactly 4x or 8x of 128x64, which
these are, so it accepts them - but the guideline is theirs, and the honest
move is either to say so or to replace them.

Replacing them takes about five minutes and needs a device: in qFlipper, use
the screenshot button above the screen preview, save six PNGs over the ones in
`screenshots/` keeping the same names, and do not crop or re-encode them. The
screens, in the order the manifest lists them:

- `ss0.png` - a CLASSIC round mid-playback, shapes assist, HUD showing the
  round and stage. This is the app card preview, so it matters most
- `ss1.png` - a RULE card
- `ss3.png` - REFLEX with a cue live and the bar draining
- `ss2.png` - the RECORDS table for CLASSIC, with scores in it
- `ss5.png` - the STATS screen
- `ss4.png` - the menu

## 2. Commit and push

Commit whatever is in `screenshots/`. Then take the SHA of that commit:

    git rev-parse HEAD

Put it in the `commit_sha` field of `catalog/manifest.yml`, commit that too,
and push. The SHA points at the commit holding the screenshots - it does not
need to be the newest commit, and it must not be a branch name.

## 3. Check the manifest before submitting

From a clone of the catalog repository:

    python3 -m venv venv
    source venv/bin/activate
    pip install -r tools/requirements.txt
    export UFBT_HOME="$PWD/venv/ufbt"
    ufbt update
    python3 tools/bundle.py applications/Games/beepback/manifest.yml bundle.zip

Run it *without* `--nolint`. The lint step is `ufbt lint`, which is
clang-format over the whole tree, and it is a hard failure in their CI.
`./test/run_tests.sh` in this repository runs the same check, so if the tests
pass the lint will too.

This has already been run end to end against commit
`b8522fabbdaac1c61b3ab917715443eb1e39f10c` and passed every stage: clone,
lint, build, manifest sync from `application.fam`, the markdown filter, the
icon check and all six screenshots.

## 4. Open the pull request

1. Fork https://github.com/flipperdevices/flipper-application-catalog
2. Branch, named `<your-github-username>/beepback_1.0`
3. Add `catalog/manifest.yml` from this repo at
   `applications/Games/beepback/manifest.yml` in the fork
4. Open the pull request and fill in their template

Moderation usually takes one to two business days.

## Updating later

Bump `fap_version` in `application.fam`, add a section to `changelog.md`, and
submit a new manifest with the new commit SHA. Each submission must carry a
higher version than the last or it is rejected.

## What the catalog requires, and where this repo answers it

- open source license — `LICENSE` (MIT)
- builds with uFBT against the current release firmware — SDK 1.4.3, API 87.1
- 10x10 1-bit icon — `beepback_10px.png`, pure black and white
- `README.md` for the long description — written to their markdown subset,
  which allows headers to depth two, bold, italic, lists and links, and
  forbids backticks, images, code blocks, horizontal rules and blockquotes
- `changelog.md` — same subset
- unique lowercase app id — `beepback`
- version as major.minor — `fap_version="1.0"`
