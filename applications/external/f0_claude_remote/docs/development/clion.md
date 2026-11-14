---
description: How CLion opens a set-up clone with navigation and the run configurations, with no plugin.
---

# CLion

In a clone where `mise run setup` has run:

```shell
clion .
```

Only `.idea/misc.xml` and `.idea/debug-profiles.xml` are tracked,
so a first `clion .` finds the compilation database and the debug profile,
and the rest of `.idea/` stays ignored.

## Navigation

A build leaves `compile_commands.json` at the root,
naming the real source paths and the compiler under `.ufbt/toolchain/arm64-darwin/bin`,
and lists a new `.c` file with no further command.
`setup` generates the headers the sources include,
so the first open shows no unresolved include,
and go-to-declaration on an SDK include reaches `.ufbt/current/sdk_headers`.
CLion started from Toolbox navigates the same.

## Run configurations

`.run/` carries four, listed on open:

| Configuration | Runs |
|---|---|
| `Build` | the build, leaving `dist/f0_claude_remote.fap` |
| `Launch` | the build, the upload and the start on the device |
| `Debug` | a session on the device through the probe |
| `Test` | the host tests, its console showing the `ok` lines |

`Debug` needs the debug profile `Flipper`, from `.idea/debug-profiles.xml`,
picked once per clone; CLion keeps the choice.
[Debugging](debugging.md#in-clion) starts there.
Opening the clone and picking the profile leave `git status --short` empty.

## Checks

CLion takes its clang-tidy checks from `.clang-tidy`, in place of its own default set,
and `mise run clang-tidy` runs the same list.
