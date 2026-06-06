# Build From Source

## Requirements

- Python 3.8 or newer
- `ufbt` ≥ 0.2.6 ([Flipper's Micro-FBT](https://github.com/flipperdevices/flipperzero-ufbt))
- macOS, Linux, or Windows (WSL)
- USB cable for the Flipper (only required for `ufbt launch`)

## One-time setup

```bash
# Install ufbt
pip install --user ufbt

# Bootstrap Flipper SDK (downloads ~150 MB the first time)
ufbt update --channel=dev
```

The `--channel=dev` flag pulls the latest mntm-dev SDK, which is what we develop against. For stable releases use `--channel=release`.

Verify the install:

```bash
ufbt --version
ufbt status      # shows current SDK channel + version
```

## Build commands

```bash
ufbt              # build only — produces dist/pingequa_rf_toolkit.fap
ufbt launch       # build + sideload to connected Flipper + auto-launch
ufbt cli          # interactive Flipper CLI session
ufbt vscode_dist  # generate compile_commands.json for VSCode IntelliSense
ufbt format       # format C files using project clang-format rules
```

## Project layout

```
pingequa_rf_toolkit/
├── application.fam              FAP manifest
├── pingequa_app.{c,h}           Entry point
├── core/                        Hardware abstraction layer
├── scenes/                      Scene handlers
├── views/                       Custom Views
├── icon.png                     App icon
└── dist/                        Build output
```

## Editing for a different firmware target

```bash
# Stable Momentum
ufbt update --channel=release

# Specific commit
ufbt update --hw-target=7 --branch=mntm-012-3.4.5

# Custom SDK ZIP
ufbt update --local=path/to/sdk.zip
```

The build is firmware-agnostic — same source, different target SDKs.

## Static checks

```bash
# Build clean
ufbt

# FAP size budget
SIZE=$(wc -c < dist/pingequa_rf_toolkit.fap)
if [ "$SIZE" -gt 30720 ]; then echo "FAP too big: $SIZE bytes"; fi
```

## Debugging

### View live logs over USB

```bash
ufbt cli
> log debug
```

This subscribes the CLI to the Flipper firmware log. App logs are tagged with `Pq*` prefixes (e.g. `PqApp`, `PqScannerScene`).

### Memory profiling

Flipper firmware exposes free heap via CLI:

```bash
> free
```

Run with the app open vs closed to see allocation. Our app uses ~50 KB peak.

## Releasing

For maintainers:

1. Bump version in `application.fam` (`fap_version="0.3.0"`)
2. Update `CHANGELOG.md` — move `[Unreleased]` items to a new `[0.3.0]` section
3. Tag and push: `git tag v0.3.0 && git push --tags`
4. Create a GitHub Release pointing to that tag — CI will attach the FAP automatically (see [.github/workflows/build.yml](.github/workflows/build.yml))

---

🛒 **[Get the PINGEQUA 2-in-1 RF Devboard](https://www.pingequa.com/products/flipper-zero-nrf24-cc1101-2-in-1-rf-devboard?utm_source=github&utm_medium=docs_build&utm_campaign=rflab)** — the hardware this app is built for.
