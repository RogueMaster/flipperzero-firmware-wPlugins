---
description: The daily commands that build the app, start it on the device and lint its sources.
---

# Build and run

```shell
mise run build
mise run launch
mise run clang-tidy
```

From `.mise/tasks/`:

- `build` builds the app.
- `launch` builds it, uploads it to the device and starts it.
  `ufbt flash` is not the same thing: it rewrites the device's firmware.
- `clang-tidy` builds it, then checks every `.c` and `.h` under `src/` against `.clang-tidy`.

A build leaves `dist/f0_claude_remote.fap` for hardware target 7,
its log reading `Target: 7, API: 88.2`.

`launch` needs the device on USB, unlocked and on its desktop:
a PIN lock drops its USB console, and the task fails.
Its log reads `Launching app: /ext/apps/Bluetooth/f0_claude_remote.fap`,
and the app opens across the whole display as `Claude Remote`, listed under Apps → Bluetooth.
Holding Back leaves it.

Run ufbt through mise, from the repository root:

```shell
mise exec -- ufbt        # build
mise exec -- ufbt lint   # check the sources against Flipper's format
```

Lint checks `src/` and `test/` against the SDK's `.clang-format`, which the repository carries.
Setup, build and lint change no tracked file and leave no untracked one.
