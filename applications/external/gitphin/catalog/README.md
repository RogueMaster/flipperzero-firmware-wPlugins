# Publishing Gitphin in the Flipper Apps Catalog

The [Flipper Apps Catalog](https://github.com/flipperdevices/flipper-application-catalog) is the
official app store of the Flipper Zero: apps listed there install with one tap from the Flipper
mobile app and from [Flipper Lab](https://lab.flipper.net/apps), and are rebuilt by Flipper for
every new firmware. The catalog hosts only a `manifest.yml` that points to a commit of this
repository; Flipper's servers build the `.fap` from source.

## What is ready

- `flipper_app/application.fam`: unique `appid` (`gitphin`), name, category `Tools`, version
  (`major.minor`), author, web URL, description, 10x10 1-bit icon (`gitphin_10.png`).
- The plugins and the web page are embedded in the `.fap` (`fal_embedded`, `fap_file_assets`),
  so the single file the catalog delivers is the whole app.
- `flipper_app/README.md`: the catalog description (only the Markdown the catalog allows).
- `flipper_app/docs/changelog.md`: the catalog changelog (`v1.0:` entries).
- MIT license, public GitHub repository, builds with uFBT on the release and dev firmware (CI).
- `catalog/manifest.yml` (this folder); each GitHub release attaches `catalog-manifest.yml` with
  `commit_sha` filled in.

## What you still do once

1. **Screenshots.** Start Gitphin, take two screenshots with **qFlipper** (the screenshot button
   on the device screen view), for example "Ready" and a transfer with the progress bar. Do not
   resize or edit them. Save them as `flipper_app/screenshots/ss0.png` and `ss1.png`, commit, and
   release (bump `fap_version`) so the release commit contains them.
2. **Validate** in a checkout of the catalog repository (see its `documentation/Manifest.md`):

   ```bash
   python3 -m venv venv && . venv/bin/activate && pip install -r tools/requirements.txt
   mkdir -p applications/Tools/gitphin && cp <catalog-manifest.yml from the release> applications/Tools/gitphin/manifest.yml
   python3 tools/bundle.py --nolint applications/Tools/gitphin/manifest.yml bundle.zip
   ```

3. **Pull request.** Fork the catalog, create the branch `Adhamura/gitphin_1.0`, add
   `applications/Tools/gitphin/manifest.yml`, open the pull request and fill in its template.
   Moderation usually takes 1–2 business days.

## Notes for the review

- Gitphin needs a PC companion (the bridge, Linux). The description says so in the first
  section, with the link to this repository.
- The bridge's optional Collector tab (PC facts, opt-in Wi-Fi password helper) runs on the PC
  only; the Flipper app does not contain it. Mention it if the reviewers ask.
- The catalog lints sources with `ufbt lint` (clang-format). CI runs the same lint on every
  push; fix a failure with `cd flipper_app && ufbt format`.

## Updating

Each catalog update needs a higher `fap_version`. After a release, copy the new
`catalog-manifest.yml` over the old manifest in your catalog fork and open a new pull request.
