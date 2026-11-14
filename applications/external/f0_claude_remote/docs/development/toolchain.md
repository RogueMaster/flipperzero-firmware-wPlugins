---
description: What a clone installs and where, how ufbt and the SDK stay in the project, and how the SDK and a tool are bumped.
---

# Toolchain

Building takes an Apple Silicon Mac and mise.
Debugging on the device is in [Debugging](debugging.md).

## What a clone installs

A fresh clone needs mise and nothing else;
after `mise run setup`, `clion .` opens the project.

`mise install` puts each tool pinned in `mise.toml` into mise's store in the home folder, `~/.local/share/mise/`,
from the graph `mise.lock` and `.mise/locks/` record, wheels only,
and refuses a graph that is missing or edited.

ufbt runs from its own venv on Python 3.14.10, with no `pip`,
and adds nothing to that runtime.
Neither the runtime, the venv's `python` nor the toolchain's `bin` reaches `PATH`,
and ufbt fails outside the project.
The project environment unsets `PYTHONPATH` and `PYTHONHOME`,
refuses `pip` outside a venv, forbids uv's Python downloads and disables ccache,
so the caller's shell cannot redirect the tools.

SDK, toolchain and build output stay in `.ufbt/` and `dist/`.
`UFBT_HOME` in `mise.toml` holds ufbt to `.ufbt/` however it starts from the root,
and Python's bytecode lands there too.

Project tasks are files in `.mise/tasks/`:

- `setup` deploys the pinned SDK and builds once,
  under a sandbox that denies writes outside the project and temp.
  It runs on macOS only.
- `check-workflow` runs the jobs of `.github/workflows/ci.yml` through `act`,
  contained, against colima's Docker socket unless `DOCKER_HOST` says otherwise.
- `docs-build` builds the site from `docs/` into `.site/`,
  failing on a link to a missing page.
- `docs-dev` serves it at `http://127.0.0.1:8000/f0-claude-remote/`,
  reloading the open page on each saved edit.
- `release` cuts a release from `develop` and pushes it; see [Release](../contributing/release.md).

The rest have their pages:
`build`, `launch` and `clang-tidy` [Build and run](build-and-run.md),
`test` [Host tests](host-tests.md),
and `debug` [Debugging](debugging.md).

`setup` also runs ufbt under a throwaway `HOME` and fails if anything lands in it,
so `~/.ufbt` never appears.

Never run `ufbt vscode_dist`:
it overwrites `.editorconfig` and `.gitignore`.

## Bumping

To bump the SDK,
edit `FLIPPER_SDK_VERSION` in `mise.toml` and run `mise run setup`.
`setup` deploys that version whatever the update server offers that day,
and fails if another one stays deployed.

To bump a tool,
remove `[tool_config]` from `mise.toml`,
since `locked = true` refuses a graph that does not exist yet.
Change the pin, then restore the section once the new graph is locked:

```shell
mise install uv python@<version>
mise lock
mise install
```
