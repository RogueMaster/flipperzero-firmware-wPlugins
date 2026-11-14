# Contributing to SSH Runner

[← Documentation index](docs/README.md)

Thanks for your interest in SSH Runner. Bug reports, ideas and pull requests are welcome.

## Contents

- [Reporting a Bug](#reporting-a-bug)
- [Suggesting a Feature](#suggesting-a-feature)
- [Development Setup](#development-setup)
- [Running the Checks](#running-the-checks)
- [Making a Change](#making-a-change)
- [Style](#style)
- [License](#license)

## Reporting a Bug

Open an issue with:

- the part affected (Flipper app, ESP32 firmware or `sshrunner`) and the release version;
- the Flipper firmware (Unleashed or official, and its version) and the board;
- what you did, what you expected and what happened;
- the message on the Flipper's screen, or the ESP32's console output (`status`), if any.

Remove passwords, keys and private addresses before posting. For a security problem, do not
open an issue: follow [Reporting a Vulnerability](docs/SECURITY.md#reporting-a-vulnerability).

## Suggesting a Feature

Open an issue describing the use case first; for anything larger than a small fix, agree on
the approach there before writing code.

## Development Setup

- [Docker](https://docs.docker.com/get-docker/): the ESP-IDF, uFBT and Go toolchains run in
  pinned containers, so nothing else is installed globally.
- Python 3 with a virtual environment in the repository (`venv/`), from `requirements.txt`.
- For on-device work: a Flipper Zero and a Waveshare ESP32-S3-ETH wired as in
  [Hardware and Wiring](docs/HARDWARE.md).

Repository layout and build outputs: [Architecture](docs/ARCHITECTURE.md).

## Running the Checks

One command builds and tests everything (the Flipper app for both firmwares, the ESP32
firmware, the PC tool, and the host and tool tests). CI runs the same steps.

```sh
tools/check.sh                 # Linux or WSL
```

```powershell
powershell -ExecutionPolicy Bypass -File tools\check.ps1   # Windows
```

A pull request is ready to merge when the checks pass.

## Making a Change

- Keep a pull request to one change, with tests where the code is testable without hardware
  (`proto/tests/`, `flipper-app/tests/`, `firmware-esp32/test_host/`, `tools/tests/`, `cli/`).
- The UART protocol lives in `proto/`; the Flipper app builds a copy in `flipper-app/lib/proto/`.
  Edit `proto/`, then run `python tools/sync_proto.py`; the checks fail if the copy differs.
  A protocol change also updates [docs/PROTOCOL.md](docs/PROTOCOL.md).
- A change users notice gets a line under `## [Unreleased]` in [CHANGELOG.md](CHANGELOG.md).
  Cutting a release from it: [Releasing](docs/RELEASING.md).
- A change to a screen updates its screenshots in `docs/screenshots/` (the walks in
  `tools/flipper/walks.py`, run in demo mode) and, if they change, the README images
  (`python tools/readme_media.py`).
- Every screen of `sshrunner` in the documentation is generated from its real output by
  `python tools/cli_figures.py` (in Docker; `--check` tells whether they are current), never
  drawn by hand.
- Firmware builds must never enable flash encryption, secure boot or NVS encryption: those
  burn eFuses on the board's first boot.

## Style

- C: 4-space indent, matching the surrounding code; `proto/` stays portable C99 with no
  allocation; no compiler warnings (the host tests build with `-Werror`).
- C is formatted with clang-format (`.clang-format` from the Flipper firmware;
  `tools/format_c.sh` formats, the checks fail otherwise).
- Go: `gofmt`, and `go vet` clean.
- Python: standard library first, type hints on new functions.
- Documentation: English (US spelling), Title Case headings, menu paths written as
  `Settings > Developer > Mock Backend` with the app's exact labels.
- Commit messages: a short summary line, then what changed and why.

## License

By contributing you agree that your contribution is licensed under
[GPL-3.0-only](LICENSE), like the rest of the project.
