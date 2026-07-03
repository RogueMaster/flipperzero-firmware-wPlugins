# Contributing

Thanks for helping improve KneeFlip Rehab.

## Build

Install uFBT, then run:

```sh
ufbt
```

To install and run on a connected Flipper Zero:

```sh
ufbt launch
```

## Test

Before opening a pull request, run:

```sh
ufbt format
ufbt lint
ufbt
```

When hardware is available, manually check:

- App opens from Tools
- Main menu navigation works
- Back exits every screen safely
- Timers can start, pause, resume, and cancel
- About screen shows the safety disclaimer

## Medical Wording Safety

KneeFlip Rehab is a timer and counter tool only. Contributions must not add:

- Medical recommendations
- Diagnosis logic
- Treatment claims
- Automated exercise prescriptions
- Claims that the app treats, prevents, cures, or diagnoses any condition

Use neutral wording such as "timer", "counter", "clinician-prescribed routine", and "follow your clinician's instructions".

## Issues

Open GitHub issues for bugs, build problems, UI problems, catalog metadata updates, or safety wording concerns. Do not include private medical information or identifiable health data in public issues.

## Pull Requests

Keep changes small when possible. Include:

- What changed
- How it was tested
- Any hardware or toolchain limitations
- Any safety wording impact
