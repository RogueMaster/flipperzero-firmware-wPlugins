# Publishing to the Flipper Apps Catalog

The repository is prepared for the catalog. The catalog hosts only a small
`manifest.yml` that points to this public source repository.

## 1. Push to a public GitHub repository

Create a public repository named `flipper-zero-ducobox` under the account you
want to use, then push this repository:

```bash
git remote add origin git@github.com:YOUR_GITHUB_USER/flipper-zero-ducobox.git
git add .
git commit -m "Prepare for Flipper Apps Catalog"
git push -u origin main
```

The catalog only accepts public GitHub repositories. If your default branch is
not `main`, adjust the commands.

## 2. Create the catalog manifest

The tracked `manifest.template.yml` is the template. Copy it once to the
gitignored `manifest.yml` and fill in:

- `YOUR_GITHUB_USER` with your GitHub account name.
- `YOUR_COMMIT_SHA` with the full SHA of the pushed commit.

```bash
cp manifest.template.yml manifest.yml
git rev-parse HEAD
# edit manifest.yml with your account and the SHA
```

The remaining metadata (name, ID, category, version, author, icon, short
description) is read from `application.fam`. The catalog reads the copy of the
manifest in its own repository; `manifest.template.yml` stays tracked and
`manifest.yml` stays local.

## 3. Validate the submission

The catalog's validator clones the source repository, fetches the latest
release SDK with `ufbt`, builds the FAP, and checks the icon, screenshots and
Markdown. Run it on a machine with Python 3 and `ufbt` available:

```bash
git clone https://github.com/flipperdevices/flipper-application-catalog.git
cd flipper-application-catalog
python3 -m venv venv
. venv/bin/activate
pip install -r tools/requirements.txt

mkdir -p applications/Sub-GHz/ducobox
cp /path/to/flipper-zero-ducobox/manifest.yml applications/Sub-GHz/ducobox/manifest.yml
python3 tools/bundle.py applications/Sub-GHz/ducobox/manifest.yml bundle.zip
```

The command must finish without errors. The catalog CI runs the same checks on
the pull request.

On NixOS, the project's `nix-shell` provides `ufbt`, but the validation venv
needs its own Python packages. The simplest option is to run the validator
outside the project shell on a system where the generic Linux `ufbt` toolchain
runs, or enable `programs.nix-ld`.

## 4. Open the pull request

1. Fork `flipperdevices/flipper-application-catalog` on GitHub.
2. Add the filled `manifest.yml` from your source checkout as
   `applications/Sub-GHz/ducobox/manifest.yml` in the fork. Do not add the rest
   of the app there.
3. Create a branch such as `YOUR_GITHUB_USER/ducobox_0.1`, commit, and push.
4. Open a pull request against the catalog's `main` branch and fill in the
   template, including the AI usage disclosure.

When the PR is merged, the app appears under **Sub-GHz** in the mobile apps and
Flipper Lab. Future updates require a higher `fap_version` in `application.fam`
and a new `manifest.yml` (copied from `manifest.template.yml`) with the new
commit SHA.

## Notes

- The catalog builds with the latest release SDK (currently 1.5.1, API 88.2),
  not the `FW_VERSION=1.4.3` pin used by the local `Makefile`. The source builds
  against both, but a catalog build may be too new for an older device
  firmware.
- `screenshots/ss0.png` is a 4x nearest-neighbour copy of the 128x64 Flipper
  framebuffer, so the catalog validator can downscale it back to 128x64. It
  passes the validator. For strict compliance with the catalog's qFlipper rule,
  replace it with a 512x256 or 1024x512 qFlipper capture before pushing.
- `CATALOG.md` is written for the catalog's limited Markdown subset. Keep code
  blocks, tables, inline code and headings deeper than level 2 out of it.
