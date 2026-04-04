# FlipPar

FlipPar is a Flipper Zero external app for tracking golf or disc golf rounds on-device. It lets you set the number of holes and players, rename players, record par and score values hole-by-hole, and export a plain-text score sheet to the SD card.

| Release Download |
| --- |
| **Ready to install on your Flipper?** Download the packaged `.fap` build from the latest release: **[FlipPar Main Release](https://github.com/jsammarco/FlipPar/releases/tag/Main)** |

## Features

- Supports 1 to 27 holes
- Supports 1 to 10 players
- Editable player names
- Per-hole par tracking
- Per-player hole scores
- Automatic current-round persistence and restore
- Running total view with leader summary relative to par
- Plain-text score-sheet export to the SD card

## Screenshots

| Splash | Setup 1 |
| --- | --- |
| ![Loading screen](https://raw.githubusercontent.com/jsammarco/FlipPar/refs/heads/main/Screenshots/Loading%20Screen.png) | ![First setup screen](https://raw.githubusercontent.com/jsammarco/FlipPar/refs/heads/main/Screenshots/Menu1.png) |
| Startup splash while the app initializes. | First setup screen with round options and quick actions. |

| Setup 2 | Main |
| --- | --- |
| ![Second setup screen](https://raw.githubusercontent.com/jsammarco/FlipPar/refs/heads/main/Screenshots/Menu2.png) | ![Main view without grid](https://raw.githubusercontent.com/jsammarco/FlipPar/refs/heads/main/Screenshots/Main%20View%20No%20Grid.png) |
| Second setup screen with additional round actions and export options. | Main round view before switching to the full score grid. |

| Grid | Totals |
| --- | --- |
| ![Score grid view](https://raw.githubusercontent.com/jsammarco/FlipPar/refs/heads/main/Screenshots/Grid%20View.png) | ![Totals view](https://raw.githubusercontent.com/jsammarco/FlipPar/refs/heads/main/Screenshots/Totals.png) |
| Hole-by-hole score entry with par and player rows. | Running totals with the leaderboard relative to par. |

## Controls

### Setup screen

- `Up` / `Down`: move between setup fields
- `Left` / `Right`: change holes, players, or selected player name slot
- `OK` on `Name`: open the text editor for that player
- `OK` on `Start Round`: begin score entry
- `OK` on `New Game`: clear the current scorecard after confirmation
- `OK` on `Save Score Sheet`: export the current round

### Score grid

- `Left` / `Right`: move between holes
- `Up` / `Down`: move between par row and player rows
- Short `OK`: increase the selected par or score value
- Long `OK`: decrease the selected par or score value
- `Back`: return to setup

## Save Location

Exported score sheets are written to:

`/ext/apps_data/flippar`

Files are created with a date-and-time-based name such as:

`FlipPar_2026-3-31_14-05-09.txt`

If a file for that exact timestamp already exists, FlipPar appends a numeric suffix.

The in-progress round is also auto-saved to:

`/ext/apps_data/flippar/current_round.bin`

That file is used to restore the current scorecard if the app is closed and reopened.

## Project Layout

- `flippar.c` - main application source
- `application.fam` - Flipper Zero app manifest
- `icon.png` - Flipper package icon; must be `10x10`
- `assets/splash_128x64.png` - bundled splash image shown at startup

## Building

This repository contains a standard Flipper Zero external app layout:

- `application.fam` defines the app metadata and entry point
- `flippar.c` contains the app implementation

To build it, place this project in your Flipper Zero firmware external-apps workflow and compile it with your preferred Flipper build toolchain. If you already build external `.fap` apps, this project is ready to drop into that process as-is.

This repo also includes a helper script, `build.ps1`, that mirrors this project into your firmware tree and runs `fbt`.

Default usage:

```powershell
.\build.ps1
```

Preview the sync without copying, deleting, or building:

```powershell
.\build.ps1 -PreviewSync
```

Override the source or target directories explicitly:

```powershell
.\build.ps1 `
  -SourceDir C:\Users\Joe\Projects\FlipPar `
  -FirmwareDir C:\Users\Joe\Projects\flipperzero-firmware `
  -TargetDir C:\Users\Joe\Projects\flipperzero-firmware\applications_user\flippar
```

The script prints the resolved `SourceDir`, `FirmwareDir`, `TargetDir`, and `AppSrc` before syncing. It also refuses to mirror if the target is the same as the source or if the target is outside the selected firmware directory.

If you copy this app into `applications_user/flippar`, copy the whole folder contents except `.git` and `README.md`. The required image files are:

- `icon.png` at the app root
- `assets/splash_128x64.png` in the assets folder

## Installing

After building:

1. Copy the generated `.fap` to your Flipper Zero SD card.
2. Launch `FlipPar` from the Apps menu.

## Notes

- Default round setup is 18 holes and 2 players.
- Default player names are `P1` through `P10`.
- Par and score values are clamped between `0` and `99`.

## Author

Created by ConsultingJoe.
