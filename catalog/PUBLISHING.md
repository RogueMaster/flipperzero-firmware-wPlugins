# Publishing BEEPBACK to the Flipper Apps Catalog

Everything in this repository is ready except the screenshots, which have to
come off a real device, and the commit SHA, which cannot exist until the
screenshots are committed. This is the order to do it in.

## 1. The screenshots

`screenshots/` holds captures taken off a real device with qFlipper's
screenshot button, at the 512x256 that button writes, unedited. That is what
the catalog asks for and what the manifest points at.

`renders/` holds the same screens drawn by `tools/shoot/`, which sets up the
firmware's own copy of u8g2 with the two fonts `canvas_set_font()` picks and
calls the app's real `bb_draw()`. Those are for the promo art and for checking
a layout without a device. The two directories are kept apart on purpose: a
render is not a screenshot, the first submission was rejected for confusing
the two, and `shoot.sh` writing into `renders/` means it cannot make that
mistake again for us.

To replace a capture: qFlipper, click the Flipper's screen to start full
screen streaming, navigate on the device, then Ctrl+S. Do not crop, scale or
re-encode it afterwards - and be careful how the file travels, because iOS
re-encodes a PNG on export and shifts the orange by a few counts.

## 2. The commit SHA

Already filled in. `catalog/manifest.yml` points at the commit holding the
screenshots and `catalog/description.md`. If you replace the screenshots,
commit them, run `git rev-parse HEAD`, and put that SHA in instead — it must be
a SHA, never a branch name.

## 2b. Two things only the repo owner can change

Writes to repository settings are blocked for this session, so these are yours:

- **Settings, General, Default branch** — switch it to `main`. The branch is
  pushed and identical; the default is still the old `claude/...` name, which is
  what a moderator sees first. The manifest pins a SHA, so nothing breaks either
  way. Afterwards the old branch can be deleted.
- **The repo page, the gear beside About** — description and topics. Suggested
  description: *A memory game for Flipper Zero. Five modes, changing rules, and
  a daily run every Flipper in the world plays the same.* Topics:
  `flipperzero`, `flipper-zero`, `flipperzero-app`, `fap`, `game`, `memory-game`,
  `embedded`, `c`.

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
- version as major.minor — `fap_version`, raised on every submission
