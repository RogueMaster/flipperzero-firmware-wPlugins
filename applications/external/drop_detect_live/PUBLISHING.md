# Publishing FliPort to the Flipper App Catalog

The catalog lives at https://github.com/flipperdevices/flipper-application-catalog.
Submissions are pull requests against that repo — the catalog re-builds your
`.fap` from the source you point it at, so we never upload a binary.

Everything in `expo_app/` is the submission. The pieces it expects:

| File | What it does | Status |
| ---- | ------------ | ------ |
| `application.fam` | Tells `ufbt` how to build the app + provides metadata for the catalog. | Ready. |
| `manifest.yml` | Catalog metadata: source URL, description, changelog, screenshots. | Ready, but needs a real `commit_sha` (see step 3). |
| `fliport_10px.png` | 10×10 1-bit menu icon shown next to the app name in `Apps → Tools`. | Placeholder teardrop — replace with real art when you have it. |
| `screenshots/*.png` | Four 128×64 1-bit PNGs shown on the catalog page. | **TODO — see step 2.** |

## What's already done

- The app builds clean with `ufbt` (target 7, API 87.1) and the new
  `dist/drop_detect.fap` is in place.
- `application.fam` now has `fap_icon`, `fap_weburl`, `fap_version="0.2"`.
- `manifest.yml` is scaffolded with description + changelog.

## What you need to do before opening the PR

### 1. Replace the placeholder icon (optional but nice)

`fliport_10px.png` is a generic teardrop I generated as a placeholder. To
swap in your own art, drop a 10×10 1-bit PNG at the same path (white
background, black foreground) and rebuild:

```powershell
ufbt
```

### 2. Capture screenshots

The catalog requires PNG screenshots of the actual app running. Easiest path:

```powershell
ufbt launch              # flashes + launches FliPort on a connected device
# In another shell, with qFlipper closed, use the screenshot CLI:
ufbt cli screenshot      # saves a .bmp / .png from the current screen
```

Take at least four — menu, drop "READY", drop "Falling" animation, and one
of the REC screens — and save them as:

```
screenshots/01_menu.png
screenshots/02_drop_ready.png
screenshots/03_drop_falling.png
screenshots/04_idle_rec.png
```

The filenames already match the `screenshots:` list in `manifest.yml`. If
you change the filenames, update the manifest to match.

### 3. Tag a release commit

The catalog pulls source by SHA, not by branch, so the build is
reproducible. Tag the commit you want shipped:

```powershell
git tag -a v0.2 -m "FliPort 0.2: smooth animations + reset-on-entry"
git push origin v0.2
```

Then put the SHA into `manifest.yml`:

```powershell
git rev-parse HEAD       # copy the 40-char SHA
```

Paste it in place of `REPLACE_WITH_RELEASE_COMMIT_SHA` in `manifest.yml`.
Bump `fap_version` in `application.fam` and the changelog header in
`manifest.yml` together every time you ship a new release.

### 4. Open the PR

1. Fork https://github.com/flipperdevices/flipper-application-catalog.
2. In your fork, add a folder under `applications/Tools/drop_detect/` and
   put `manifest.yml` + `fliport_10px.png` + `screenshots/` inside it.
   The catalog manifest lives in the catalog repo, NOT in this repo — the
   `manifest.yml` we just created is the template you copy across.
3. Open a PR. The catalog CI will:
   - Clone your `sourcecode.location.origin` at the pinned `commit_sha`.
   - Run `ufbt` against `subdir` (= `flipper-zero/expo_app`).
   - Validate the resulting `.fap` against the catalog rules.
4. A maintainer reviews and merges; the app shows up on
   https://lab.flipper.net/apps automatically.

### Useful refs

- Catalog repo + submission guide: https://github.com/flipperdevices/flipper-application-catalog
- `application.fam` reference: https://developer.flipper.net/flipperzero/doxygen/app_manifests.html
- Icon spec (10×10, 1-bit): https://developer.flipper.net/flipperzero/doxygen/js_gui.html

## Re-building locally

From `flipper-zero/expo_app/`:

```powershell
ufbt              # builds dist/drop_detect.fap
ufbt launch       # builds + flashes + launches on connected device
ufbt cli log      # tail the device log to watch FURI_LOG_I output
```
