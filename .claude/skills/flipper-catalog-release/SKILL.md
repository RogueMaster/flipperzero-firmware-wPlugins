---
name: flipper-catalog-release
description: Bump the tpms_bridge Flipper app to a new version and prepare/submit the update PR to the flipper-application-catalog fork. Use whenever the user asks to release, publish, or bump the version for the Flipper Apps Catalog, or to "prepare a version for the flipper-zero repo".
---

# TPMS Bridge — Apps Catalog release

This app (`flipper/tpms_bridge`) is already live in
[flipperdevices/flipper-application-catalog](https://github.com/flipperdevices/flipper-application-catalog)
under `applications/Sub-GHz/tpms_bridge/`. The user's fork is
[VishovVladimir/flipper-application-catalog](https://github.com/VishovVladimir/flipper-application-catalog).
Full upstream rules: `documentation/Contributing.md` in that repo.

## In this repo (flipper-zero-tpms)

1. Bump `fap_version` in `flipper/tpms_bridge/application.fam`.
2. Add a new top entry to `flipper/tpms_bridge/changelog.md` (newest first,
   format `vX.Y:` then a short paragraph of what changed).
3. If screenshots changed, update `flipper/tpms_bridge/screenshots/README.md`
   and the `screenshots:` order in `flipper/tpms_bridge/catalog/manifest.yml`
   (first entry = catalog preview image).
4. **Check `README.md` and `changelog.md` for backticks, fenced code blocks,
   images, `###`+ headers, blockquotes, or HTML.** The catalog's markdown
   filter (`tools/flipp_catalog/markdown_filter.py`) rejects all of these in
   the `description`/`changelog` fields — it raises `Markdown element 'X' is
   not allowed` at bundle time. v1.0's README had none of these; a later
   rewrite introduced backtick code spans and that broke catalog validation
   silently (nobody re-validates until the next release). Use italics
   (`*word*`) instead of backticks for inline "code".
5. Commit, push to `origin` (`VishovVladimir/flipper-zero-tpms`).
6. Update `commit_sha` in `flipper/tpms_bridge/catalog/manifest.yml` to that
   commit, commit again ("Pin the catalog manifest to the vX.Y commit"), push.

## In the catalog fork (separate clone, e.g. under the scratchpad)

```sh
git clone git@github.com:VishovVladimir/flipper-application-catalog.git
cd flipper-application-catalog
git remote add upstream https://github.com/flipperdevices/flipper-application-catalog.git
git fetch upstream
git checkout -b VishovVladimir/tpms_bridge_<version> upstream/main
```

Copy the *contents* of this repo's `flipper/tpms_bridge/catalog/manifest.yml`
into `applications/Sub-GHz/tpms_bridge/manifest.yml` in the catalog clone —
verbatim, comments and all are fine (the live file just happens to have none
because nobody's re-added them, not because they're forbidden).

Validate before pushing — this is the step that actually catches the
markdown problem above:

```sh
python3 -m venv venv && source venv/bin/activate
pip install -r tools/requirements.txt
export UFBT_HOME="$PWD/venv/ufbt" && ufbt update
python3 tools/bundle.py --nolint applications/Sub-GHz/tpms_bridge/manifest.yml bundle.zip
```

Success looks like `Bundle created: bundle.zip` with no `[E]` lines. This
clones the source repo at the pinned `commit_sha`, so if it fails, fix the
source repo, push, re-pin the commit_sha in both places, and re-run.

Commit (`Update TPMS Bridge to version X.Y`, matching the catalog's own
convention — see recent merged commits for style), push the branch to the
fork (`git push origin VishovVladimir/tpms_bridge_<version>`).

## Opening the PR

No `gh` CLI is installed in this environment, so the PR itself can't be
created from the terminal — hand the user:

- The compare link GitHub prints after the branch push (or construct it:
  `https://github.com/VishovVladimir/flipper-application-catalog/pull/new/VishovVladimir/tpms_bridge_<version>`).
- A filled-in PR body following the repo's `.github/PULL_REQUEST_TEMPLATE.md`
  (fetch it fresh each time in case it changed).

**Never check the Author Checklist boxes on the user's behalf** — "I own the
code", "I have performed a self-review", "I have commented my code" are
personal attestations only the user can honestly make. Leave those unchecked
in the draft and tell the user to review before submitting. The one box that
can legitimately be pre-checked is manifest validation, and only if it was
actually run successfully in this session.

The **AI usage disclosure** section is mandatory and its wording is the
user's call, not an assumption — ask which of the three options applies
(none / partial / fully AI-generated) before drafting the PR body, unless
they've already told you for this release.
