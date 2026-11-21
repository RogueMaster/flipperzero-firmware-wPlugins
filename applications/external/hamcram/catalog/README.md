# Submitting to the Flipper Apps Catalog

Ham Cram 1.0 is in the catalog at
[lab.flipper.net/apps/hamcram](https://lab.flipper.net/apps/hamcram).
`manifest.yml` in this folder is a copy of the one published there; the steps
below are for sending an update. Requirements were checked against the
catalog's `Contributing.md` and `Manifest.md` on 2026-11-10.

## Before submitting

1. Screenshots live in `screenshots/` as `ss0.png`, `ss1.png`, ... and are
   listed in `manifest.yml`; the first is the catalog preview. Take new ones
   with qFlipper and leave them exactly as it saves them (512x256).
2. The catalog allows only basic Markdown in a description (no images, HTML
   or code spans), so it does not use `README.md` directly. After editing
   the README, run `python tools/catalog_description.py` to rewrite
   `catalog/description.md`, which is the README without the blocks between
   `<!-- github-only:start -->` and `<!-- github-only:end -->`. CI fails if
   the two are out of step.
3. Commit everything and push to `main`. Wait for CI to pass.
4. Put that commit's full SHA into `commit_sha` in `manifest.yml`. If you
   use fewer or more screenshots, edit the list to match.

## Validate the manifest locally (recommended)

In a clone of your fork of `flipperdevices/flipper-application-catalog`:

```powershell
python -m venv venv
.\venv\Scripts\Activate.ps1
python -m pip install -r tools\requirements.txt
New-Item -ItemType Directory -Force applications\Tools\hamcram
Copy-Item <path to HamCram>\catalog\manifest.yml applications\Tools\hamcram\
python tools/bundle.py --nolint applications/Tools/hamcram/manifest.yml bundle.zip
```

The script clones this repo at `commit_sha`, builds the app and checks the
icon, screenshots, README and changelog. Fix anything it reports.

## Open the pull request

1. Create a branch named `NicPWNs/hamcram_<version>`, for example
   `NicPWNs/hamcram_1.1`.
2. Replace `applications/Tools/hamcram/manifest.yml` with the one from this
   folder.
3. Open the pull request and fill in its template.

## Updates

Bump `fap_version` in `application.fam` (the catalog rejects a version it
has already seen), add a changelog entry, push, then send a new pull request
that changes only `commit_sha`.
