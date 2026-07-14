# KneeFlip Rehab

```text
          .-.
         /'v'\
        (/   \)
        ='="="=     

     K N E E F L I P
        R E H A B
```

KneeFlip Rehab is a Flipper Zero external FAP app for clinician-prescribed knee rehabilitation routines. It is a small timer and counter tool for following timed sessions and pacing user-configured exercise blocks.

> **Safety disclaimer:** KneeFlip Rehab is a timer and counter tool only. It does not provide medical advice, diagnosis, or treatment. Always follow the recovery plan provided by your doctor, surgeon, or physical therapist.

## Features

- Ice timer
- Elevation timer
- Quad sets timer for clinician-prescribed routines
- Heel slides manual counter
- Optional future GPIO sensor support

## App Screens

Implemented:

- Main menu
- Ice timer
- Elevation timer
- Quad sets timer
- Heel slides counter
- About screen with safety disclaimer

Screenshot placeholders:

- `screenshots/main-menu.png`
- `screenshots/ice-timer.png`
- `screenshots/quad-sets.png`
- `screenshots/about.png`

## Installation

During development, build and install the FAP with uFBT:

```sh
ufbt launch
```

For manual installation, copy the built FAP from `dist/kneeflip_rehab.fap` to the Flipper Zero SD card under:

```text
/ext/apps/Tools/kneeflip_rehab.fap
```

After catalog approval, the app should be installable through Flipper Lab and the Flipper Mobile Apps.

## Build

Install uFBT, then run:

```sh
ufbt
```

Useful checks:

```sh
ufbt format
ufbt lint
ufbt
```

## Development Status

KneeFlip Rehab currently focuses on timer-based blocks and a manual heel slides counter.

Current version: `0.1.0`

## Roadmap

- Stabilize timer screens on hardware
- Capture official qFlipper screenshots
- Prepare Flipper Application Catalog PR
- Explore optional GPIO sensor support without automatic medical interpretation

## Safety Notes

- The app does not suggest exercises.
- The app does not prescribe timing, repetitions, or recovery plans.
- The app does not diagnose, treat, prevent, or cure medical conditions.
- Exercise labels are timers/counters for routines already provided by a clinician.
- Do not include private medical data in public bug reports, screenshots, or pull requests.

## Catalog Submission

This repository contains the app source code and public project files. A best-effort catalog manifest draft lives at:

```text
docs/catalog/manifest.yml
```

Before opening a pull request to `flipperdevices/flipper-application-catalog`, replace the placeholder GitHub URL and commit SHA, capture official qFlipper screenshots, and validate the manifest in a local checkout of the catalog repository.

## License

MIT License. See [LICENSE](LICENSE).
