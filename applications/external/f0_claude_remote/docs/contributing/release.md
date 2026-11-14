---
description: How one command cuts a release, and what its tag publishes.
---

# Release

One command on a clean `develop` cuts a release and pushes it:

```shell
mise run release 0.2
```

It starts `release/0.2`,
sets `fap_version` in `application.fam`,
regenerates `CHANGELOG.md` with git-cliff,
puts the release's section on top of `catalog/changelog.md`,
builds the app, and commits the three files as `chore: release 0.2`.
Then it finishes the branch, which tags `v0.2`,
returns to `develop`,
and pushes `main`, `develop` and `v0.2` to `origin` in one atomic push.
The push of `main` publishes the site, through `.github/workflows/docs.yml`.

It refuses a name other than the latest tag's `MINOR` plus one,
a branch other than `develop`, and a worktree that is not clean.
A failure before the release commit undoes the run:
the files restored, `develop` checked out, `release/0.2` deleted,
so the fixed run is the same command.
After the commit nothing is undone.
A finish stopped on a conflict resumes once it is resolved,
and a failed push is rerun as is:

```shell
mise exec -- git flow finish --continue
git checkout develop
git push --atomic origin main develop v0.2
```

The tag runs `.github/workflows/release.yml`:
`ci.yml`'s jobs first,
then the tag checked against `fap_version`,
the app built, and the Release drafted with its notes and `f0_claude_remote.fap`,
then the `flipper-apps-catalog` job:
the fork's `main` synced with the Flipper Apps Catalog's,
each earlier version's branch deleted unless a pull request is open from it,
and `<owner>/f0_claude_remote_0.2` cut from `main`
with `applications/Bluetooth/f0_claude_remote/manifest.yml`,
its pull request opened by hand;
and last the `publish` job, which publishes the draft.

A published Release is immutable, and its tag name is spent.
Until `publish`, the Release is a draft and the tag is not locked:
a failed run is undone, its cause fixed, and the same name cut again.
The undo is

```shell
gh release delete v0.2 --yes
git push --delete origin v0.2
git tag --delete v0.2
```

whose first line finds nothing when the run made no draft.
A job that failed on GitHub's side alone is rerun with those after it,
a draft left by a failed `draft` deleted first with that first line.

A GitHub Release's notes are its own section alone:

```shell
git checkout v0.2
mise exec -- git-cliff --current --strip all
```

The `flipper-apps-catalog` job needs the owner's setup, once:

1. Fork `flipperdevices/flipper-application-catalog` into the owner's account, keeping its name.
2. Create a fine-grained token with access to the fork alone,
   Contents and Workflows read and write, and no expiry.
   A token unused for a year is revoked,
   and a new one is due before the next release.
3. In this repository's Settings → Environments, create `flipper-apps-catalog`,
   limit its deployment branches and tags to one tag rule, `v*`,
   and give it the secret `FLIPPER_APPS_CATALOG_TOKEN` holding the token.

## Changelog

`CHANGELOG.md` is git-cliff's output from `cliff.toml`,
written whole by the release commit and never edited by hand.
A release's section lists each Conventional Commit since the previous tag once, grouped by type,
and leaves out merges, the release commit and `chore(openspec)` commits.

`catalog/changelog.md` gains each release's section on top, from `catalog/cliff.toml`,
and keeps the sections below as their releases wrote them; it is never edited by hand either.
A section lists one line per `feat` or `fix` commit since the previous tag, ungrouped,
and leaves out those scoped `release`, as the release machinery ships nothing in the app.
