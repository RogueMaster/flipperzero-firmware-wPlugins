# Contributing to Time Clock

Thanks for your interest in improving Time Clock! This document explains how to
build the app, the conventions we follow, and how to propose changes.

## Ground rules

- **Be respectful.** All participation is covered by our
  [Code of Conduct](CODE_OF_CONDUCT.md).
- **English only.** All code, comments, identifiers, UI strings, file names and
  documentation are written in English.
- **Stay in scope.** This app performs **badge identification only** (reading a
  UID). Pull requests that add badge **emulation**, cloning, or anything meant to
  **bypass access-control/authentication** will not be accepted. See
  [SECURITY.md](SECURITY.md).

## Getting set up

This is a Flipper Zero external app (FAP) for the **official firmware**, built
with [`ufbt`](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
python3 -m pip install --upgrade ufbt
```

From the project root (the folder with `application.fam`):

```bash
ufbt            # build the .fap (output in dist/)
ufbt launch     # build, upload to a connected Flipper, and run it
ufbt cli        # open the Flipper CLI for logs while testing
```

## Code style

- Follow the existing style: 4-space indentation, braces on the same line,
  Flipper naming (`snake_case` functions, `PascalCase` types).
- Prefixes: `tc_*` for storage/PIN helpers, `timeclock_*` for app/scene
  functions, `TimeClock*` / `Tc*` for types and enums.
- Keep the radio-specific code isolated in `scenes/timeclock_scene_scan.c`.
- If your toolchain provides it, format C sources with `ufbt format` (clang-format)
  before committing.

## Adding a screen (scene)

1. Add an entry to `scenes/timeclock_scene_config.h`
   (`ADD_SCENE(timeclock, my_scene, MyScene)`).
2. Create `scenes/timeclock_scene_my_scene.c` implementing
   `timeclock_scene_my_scene_on_enter/on_event/on_exit`.
3. The X-macro tables in `timeclock_scene.c` and the `TimeClockScene` enum update
   automatically.

## Commit messages

- Use clear, imperative subject lines: `Add weekly summary to Today screen`.
- Reference issues where relevant: `Fix #12: ...`.
- Keep unrelated changes in separate commits/PRs.

## Pull requests

1. Fork the repository and create a feature branch.
2. Make sure the app **builds** (`ufbt`) and, ideally, runs on a device or in the
   emulator.
3. Update `README.md` / docs if behavior changes.
4. Open a PR and fill in the template. Describe what you changed, how you tested
   it, and on which firmware/version.

## Reporting bugs & requesting features

Open a GitHub issue with:

- what you expected vs. what happened,
- your Flipper firmware name and version,
- badge technology involved (NFC / LF RFID) if relevant,
- steps to reproduce.

For **security** issues, do **not** open a public issue - follow
[SECURITY.md](SECURITY.md).
