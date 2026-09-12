# Publishing BEEPBACK to the Flipper Apps Catalog

Everything in this repository is ready except the screenshots, which have to
come off a real device, and the commit SHA, which cannot exist until the
screenshots are committed. This is the order to do it in.

## 1. Take the screenshots (only you can do this)

The catalog is explicit: *"Screenshots must be created using the qFlipper
screenshot feature. Please don't change their resolution or format."*
qFlipper writes a 512x256 PNG, which is the device's 128x64 screen at 4x. The
bundler rejects any other size, so do not crop, scale or re-encode them.

In qFlipper, connect the Flipper and use the screenshot button above the
screen preview. Save four, in this order — the first one is what people see
on the app card, so it should be the game actually being played:

- `screenshots/ss0.png` — a CLASSIC round mid-playback, with the shapes cue
  and the HUD showing round and stage
- `screenshots/ss1.png` — a RULE card, so the rules mode is visible at a
  glance (SKIP DOWN or similar)
- `screenshots/ss2.png` — the RECORDS table for CLASSIC, with some scores in
  it rather than a grid of dashes
- `screenshots/ss3.png` — REFLEX with a live cue and the bar draining

Put them in `screenshots/` with exactly those names, or change the names in
`catalog/manifest.yml` to match.

## 2. Commit and push

Commit the screenshots. Then take the SHA of that commit:

    git rev-parse HEAD

Put it in the `commit_sha` field of `catalog/manifest.yml`, commit that too,
and push. The SHA in the manifest points at the commit with the screenshots —
it does not need to be the newest commit, and it must not be a branch name.

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
