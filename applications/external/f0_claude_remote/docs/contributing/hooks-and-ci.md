---
description: The hooks that gate a commit, and the CI jobs that check each push.
---

# Hooks and CI

## Hooks

[prek](https://prek.j178.dev/) holds the gate before a commit is allowed.
It is pinned in `mise.toml` and configured in `prek.toml`,
which gives each rule's reason.
Every hook is one of prek's own Rust implementations
or a binary pinned in `mise.toml`,
so this repository clones no hook repositories
and pins no third-party revs.

File-hygiene hooks run at `pre-commit`,
and so does [shellcheck](https://www.shellcheck.net) over every shell script,
the tasks under `.mise/tasks/` included.
actionlint hands it the workflows' `run` steps.
Three more read the message at `commit-msg`:
a subject of at most 72 characters,
in Conventional Commits,
and no line that credits an AI agent or a model.

```shell
mise exec -- prek install           # wire both git hooks
mise exec -- prek run --all-files   # every pre-commit hook over the whole tree
```

Run `prek install` in a fresh clone and after a prek upgrade:
the generated hooks pin the path of the prek binary they were installed from,
and that path carries the version.

## CI

`.github/workflows/ci.yml` runs on every push to `develop` and `main`
and on every pull request,
and `release.yml` calls it before it publishes,
in four parallel jobs on `ubuntu-24.04`.
Each step is a command to run before pushing:

| Job | Steps |
|---|---|
| `test` | `mise run test` |
| `build` | `mise exec -- sh -c 'ufbt update --branch "$FLIPPER_SDK_VERSION"'`, `mise run build`, `mise exec -- ufbt lint`, `mise run clang-tidy` |
| `hygiene` | `mise exec -- prek run --all-files`, `mise exec -- actionlint -pyflakes=` |
| `docs` | `mise run docs-build` |

On the Mac, `mise run setup` stands in for `build`'s first step.
Actions are pinned by commit, mise by version and the tools by `mise.lock`,
and no cache is restored;
the runner image, with its `cc` and `gh`, stays unpinned.

`mise run check-workflow` runs these jobs on the Mac through `act`,
with no Docker socket, host network or GitHub token in their containers.

`.github/workflows/docs.yml` builds the site on every push to `main`,
which only a release makes,
and publishes it to GitHub Pages,
so the site shows the latest Release.
A manual run publishes the branch it runs on, so it picks `main`.
