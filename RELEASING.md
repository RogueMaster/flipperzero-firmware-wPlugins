# Releasing

Six places end up holding a copy of this app, and the only thing that keeps them honest is
that they all point at **the same commit**. This is the order that gets them there.

- the GitHub release, with one `.fap` per firmware
- the Apps Catalog manifest, which pins a `commit_sha`
- `xMasterX/all-the-plugins`, which carries the source
- `Next-Flip/Momentum-Apps`, which carries the source as a subtree
- `djsime1/awesome-flipperzero`, which only carries a link

v0.7.0 got this slightly wrong: it was tagged at `927f7c4`, then five PRs landed — including the
clang-format sweep — and the catalog was submitted at `f7df74a`. Nothing broke, because none of
the five changed behaviour, but the binaries people downloaded were not built from the code the
catalog builds. Cut the tag last and the question does not arise.

## 1. Before the tag

```bash
python tools/gen_supported_chips.py --check     # CI runs this too
ufbt format                                     # in fake_chip_detector/, if anything changed
```

Bump `fap_version` in `fake_chip_detector/application.fam` — **the catalog rejects a
resubmission that reuses a version**, and a review comment on the open catalog PR means a new
version, not an amended commit. Add the matching entry to `fake_chip_detector/docs/changelog.md`,
which is what the catalog renders. The pack copies carry it as `CHANGELOG.md` in the app
directory instead — that is what `all-the-plugins`' PR template asks for — so copy it across
under that name when you re-copy the app in step 3.

Merge all of that to `master` through a PR, as usual. Everything below starts from a `master`
that is finished.

## 2. Create the release — with no files attached

```bash
gh release create v0.8.0 --prerelease --title "v0.8.0" --notes-file notes.md
```

**Do not pass any `.fap` files here.** The `release: published` event starts
`.github/workflows/release.yml`, which builds the three firmwares and uploads them with
`--clobber`. Attaching files by hand at the same time races that upload; the workflow wins
sometimes and you get a file built from who-knows-what the rest of the time. Let the workflow be
the only thing that writes assets.

Watch it, and check all three arrived:

```bash
gh run watch $(gh run list --workflow=release.yml -L1 --json databaseId --jq '.[0].databaseId')
gh release view v0.8.0 --json assets --jq '[.assets[].name]'
```

If a firmware's SDK is broken that day the other two still upload (`fail-fast: false`). Say so
in the release notes rather than pretending three builds exist. To retry one later:

```bash
gh workflow run release.yml -f tag=v0.8.0
```

## 3. Update the four downstream places

All four point at the release commit — `git rev-parse v0.8.0`.

| Where | What to change |
|---|---|
| [flipper-application-catalog](https://github.com/flipperdevices/flipper-application-catalog) | `applications/GPIO/fake_chip_detector/manifest.yml`: new `commit_sha` |
| [all-the-plugins](https://github.com/xMasterX/all-the-plugins) | re-copy the app into `non_catalog_apps/fake_chip_detector/`; base the branch on **`dev`**, not `main` |
| [Momentum-Apps](https://github.com/Next-Flip/Momentum-Apps) | re-copy into `fake_chip_detector/`; `.gitsubtree` stays as it is; base on **`dev`** |
| [awesome-flipperzero](https://github.com/djsime1/awesome-flipperzero) | nothing unless the description changed |

**The pack copies are not byte-identical to this repository, and a plain re-copy silently
reverts the differences.** After copying, re-apply both:

- `docs/changelog.md` goes in as `CHANGELOG.md` in the app directory (all-the-plugins asks for it
  there; Momentum carries it for consistency).
- In `LIVE_TESTS.md`, the two `../test_plugin_template` links become
  `https://github.com/hleserg/flipper-fake-chip-detector/tree/master/test_plugin_template`, and
  the second one reads "in the upstream repository" rather than "in the repository root".

```bash
grep -rn '](\.\./' non_catalog_apps/fake_chip_detector/    # must print nothing
```

Two more traps that have already cost a round trip each:

- **Base branch.** Both packs develop on `dev` and `gh pr create` will happily open the PR
  against `main` instead, which produces an eleven-thousand-file diff that no maintainer will
  read. Check `changedFiles` after opening it; `gh pr edit <n> --base dev` fixes it in place.
- **The ReadMe index.** `all-the-plugins` keeps a curated table at `ReadMe.md`; the merged
  app-addition PRs add a row to it in the block for the app's category. The catalog cell stays
  `![None Badge]` until the app is actually live on lab.flipper.net.

Validate the catalog manifest before pushing, from a checkout of the catalog repo — **twice**,
because the second form is the one CI runs:

```bash
python tools/bundle.py --nolint applications/GPIO/fake_chip_detector/manifest.yml bundle.zip
python tools/bundle.py          applications/GPIO/fake_chip_detector/manifest.yml bundle.zip
```

## 4. Screenshots and description, if they changed

- Screenshots for the catalog must be **512x256 or 1024x512** — the 128x64 screen at exactly 4x
  or 8x. A native capture is rejected outright.
- `fake_chip_detector/docs/catalog_description.md` may use H1-H2, bold, italic, lists and links
  and nothing else. A table or a single backtick is a hard error, not a silent drop. This is why
  the description is its own file and not the README.

## 5. Which builds have actually been run

The README says only the Unleashed build has been run on hardware. Before saying anything more
generous, run the thing: build, install, and walk scan → verdict → one live test on the firmware
in question. Compiling against three SDKs is not three firmwares having run it.
