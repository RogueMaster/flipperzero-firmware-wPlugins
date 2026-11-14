---
description: What the app needs, and how to install, update and remove it from a Release or from the source.
---

# Installation

## What it needs

- A Flipper Zero on firmware 1.5.1.
- A computer that takes a Bluetooth keyboard or, over a cable, a USB one.
  The app is tried only on a Mac,
  but nothing in it is specific to macOS.
- Claude Code; the app is tried on 2.1.311.
- To build from the source, an Apple Silicon Mac with [mise](https://mise.jdx.dev).
  `mise run setup` and the CLion debug profile are macOS-only.

## From a Release

Download `f0_claude_remote.fap` from the
[latest Release](https://github.com/petr-korobeinikov/f0-claude-remote/releases/latest).
In qFlipper's File manager, upload it to the SD card's `apps/Bluetooth/`.
The app then lives under Apps → Bluetooth on the device, as `Claude Remote`.

To update, upload the newer Release's file over the old one.

To remove, delete `apps/Bluetooth/f0_claude_remote.fap`,
and `apps_data/f0_claude_remote/`, which holds the pairing's keys.
Then forget `Claude Remote` on the computer.

## From the source

With the device unlocked on USB, on its desktop,
in a clone of the repository:

```shell
mise trust
mise install
mise run setup
mise run launch
```

`mise run setup` deploys the pinned Flipper SDK into the clone,
and `mise run launch` builds the app, uploads it to `apps/Bluetooth/` and starts it.

To update, `git pull` and run the last three again.

To remove, delete the app and its keys as for a Release,
and the clone, which holds the SDK and the build.
