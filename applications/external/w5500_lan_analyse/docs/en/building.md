[← Back to documentation index](README.md)

# Building from Source

## Prerequisites

- [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) (micro Flipper Build Tool)

Install ufbt:

```bash
pip install ufbt
```

## Build Commands

All commands are run from the project root directory:

```bash
ufbt build              # compile the .fap binary
ufbt launch             # build and deploy to connected Flipper via USB
ufbt install            # build and install .fap to Flipper's SD card
ufbt format             # auto-format code with clang-format
ufbt lint               # check formatting without modifying files
```

## Build Output

The compiled `.fap` file appears in `dist/`. You can copy it manually to the Flipper's SD card:

```
/ext/apps/GPIO/lan_tester.fap
```

## Manual Installation

1. Build the `.fap` file with `ufbt build`
2. Connect Flipper Zero via USB
3. Open the qFlipper app or mount the SD card
4. Copy `dist/lan_tester.fap` to `/ext/apps/GPIO/` on the SD card
5. Restart the Flipper or navigate to **GPIO** in the app menu

Alternatively, `ufbt launch` does all of this automatically and starts the app.

## Firmware Compatibility

The app targets **Flipper Zero Official Firmware (OFW)**. It is built against the latest stable OFW SDK via ufbt. Compatibility with custom firmwares (Unleashed, Momentum, etc.) is not guaranteed but generally works if the firmware tracks OFW API closely.

The SDK version is managed by ufbt automatically -- it downloads the matching SDK on first build.

## CI/CD

The project uses GitHub Actions for automated builds:

### CI (Pull Requests)

Defined in `.github/workflows/ci.yml`. On every PR to `main`:

- Builds the app with `ufbt build`
- Runs `ufbt lint` to check code formatting
- PRs that change only docs (`*.md`, `docs/**`, `LICENSE`) skip CI

### Release (Tags)

Defined in `.github/workflows/release.yml`. When a version tag (`v*.*.*`) is pushed:

- Builds the `.fap` binary
- Extracts release notes from `CHANGELOG.md` for the tagged version
- Creates a GitHub Release with the `.fap` file attached

## Development Workflow

1. Create a feature branch: `git checkout -b feat/my-feature`
2. Make changes in the project
3. Format code: `ufbt format`
4. Build and test: `ufbt build` or `ufbt launch`
5. Update documentation if needed (see [CONTRIBUTING.md](../../CONTRIBUTING.md))
6. Open a PR to `main`

### Code Style

- **Language**: C99
- **Naming**: `snake_case` for functions/variables, `UPPER_CASE` for macros/constants
- **Indentation**: 4 spaces, no tabs
- **Braces**: K&R style (opening brace on same line)
- **Formatting**: enforced by `.clang-format` in the project root; run `ufbt format` before committing

### Vendored Library

The `lib/ioLibrary_Driver/` directory contains a vendored copy of the WIZnet W5500 driver. **Do not modify** files in this directory. Only the following components are compiled:

- `Ethernet/W5500/*.c` -- W5500 chip driver
- `Ethernet/*.c` -- socket abstraction layer
- `Internet/DHCP/*.c` -- DHCP client
- `Internet/DNS/*.c` -- DNS resolver
- `Internet/ICMP/*.c` -- ICMP ping and traceroute
