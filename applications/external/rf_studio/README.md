# RF Studio

A Flipper Zero application for building and operating virtual Sub-GHz remotes.

Create a remote, press **Add Button**, press a button on the original remote, name it, and
repeat. Each remote can hold as many buttons as you need, shown as a scrolling list. Signals are
stored as standard `.sub` files and can be shared between buttons and remotes.

Project specification and decisions live outside this repository in `../documentation/`.

## Features

- Remotes with any number of named buttons (tested with 100), operated from a scrolling list.
- Guided capture: automatic frequency detection, decoding with every protocol in the firmware,
  and a raw recording fallback when nothing decodes.
- The next button on a remote defaults to the frequency the remote already uses.
- Signal Library: import `.sub` files, rename, test, mark raw signals as working, export back to
  the Sub-GHz folder, delete with a check for buttons that still use the signal.
- Remote library: search, favorites, categories, sort by name, category or last used, duplicate,
  export and import (with signals), delete.
- Firmware-driven radio capabilities: frequencies, presets and transmit permission come from the
  installed firmware. Internal and external CC1101 radios, with automatic selection.
- Rolling-code signals are identified but never replayed. Raw captures are marked untested until
  you confirm they work.
- Diagnostics screen, optional error log file, and library backup.

## Controls

| Screen | Up / Down | Left / Right | OK | Hold OK | Back |
|---|---|---|---|---|---|
| Remote | Select button | Page up / down | Send | Remote and button menu | Leave (cancels a send in progress) |
| Capture | | Options / Detect again | Keep raw recording | | Cancel |

## Building

Requires uFBT.

```sh
../ubft/.venv/bin/ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json --channel=release
../ubft/.venv/bin/ufbt            # build dist/rf_studio.fap
../ubft/.venv/bin/ufbt launch     # install to the connected Flipper and start it
```

Run these from the repository root. Any uFBT install works; the paths above use the project's
virtual environment one level up.

The SDK must match the firmware on the device. To build for official firmware instead, run
`ufbt update --channel=release` (or `--channel=rc`).

### Firmware support

The same source builds against official firmware (1.5.1 and 1.5.1-rc, API 88.2) and Momentum
(`mntm-012`, API 87.1). It has been run on Momentum; official firmware is build-checked only.
Firmware-specific features are detected at compile time: on Momentum, the app uses
`subghz_devices_check_tx` to explain why a transmission is blocked. See `docs/architecture.md`.

## Flipper Application Catalog

Store files live in the repository root: `changelog.md`, `screenshots/`,
`catalog/description.md`, and a draft `catalog/manifest.yml` to copy into the catalog repository.

## License

MIT. See `LICENSE`.

## Data on the SD card

```text
/ext/apps_data/rf_studio/
    config/settings.conf
    remotes/<remote id>.remote
    signals/<signal id>.sub
    signals/signals.index
    exports/  backups/  logs/  tmp/
```

Formats are described in `docs/storage-format.md`.
