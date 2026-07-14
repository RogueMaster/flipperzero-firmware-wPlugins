# Flipper Zero — FAP Manifest Reference

> Source: https://developer.flipper.net/flipperzero/doxygen/apps_on_sd_card.html

## Overview

FAPs (Flipper Application Packages) are `.fap` files — ELF executables with metadata and resources bundled in. They run from the SD card independently of specific firmware versions.

## Project Setup with ufbt

```bash
mkdir claude_remote && cd claude_remote
ufbt update --channel=dev
ufbt create APPID=claude_remote
```

This generates:
- `application.fam` — manifest
- `claude_remote.c` — main source
- `claude_remote.png` — 10x10 1-bit icon
- `images/` — asset directory

## application.fam Fields

```python
App(
    appid="claude_remote",              # unique ID, used in build commands
    name="Claude Remote",              # display name in Flipper menus
    apptype=FlipperAppType.EXTERNAL,   # runs from SD card
    entry_point="claude_remote_app",   # int32_t function_name(void* p)
    requires=["gui", "bt"],            # system service dependencies
    stack_size=2 * 1024,               # FreeRTOS stack (bytes)
    fap_category="Bluetooth",          # menu category on Flipper
    fap_icon="icon.png",              # 10x10 monochrome icon
    fap_icon_assets="images",          # directory for additional icons
)
```

### Field Details

| Field | Required | Description |
|-------|----------|-------------|
| `appid` | Yes | Unique identifier. Lowercase, underscores. Used in build: `./fbt fap_claude_remote` |
| `name` | Yes | Human-readable name shown in menus |
| `apptype` | Yes | `FlipperAppType.EXTERNAL` for SD card apps |
| `entry_point` | Yes | Function name: `int32_t name(void* p)` |
| `requires` | Yes | List of firmware services needed |
| `stack_size` | Yes | Thread stack size in bytes |
| `fap_category` | No | Menu category: "Bluetooth", "Tools", "Games", "GPIO", etc. |
| `fap_icon` | No | Path to 10x10 1-bit PNG |
| `fap_icon_assets` | No | Directory containing additional image assets |
| `fap_libs` | No | Additional libraries to link |

## Build Commands

| Command | Description |
|---------|-------------|
| `ufbt` | Build the app (outputs `dist/app.fap`) |
| `ufbt launch` | Build, upload, and run on connected Flipper |
| `ufbt flash_usb` | Flash firmware via USB |
| `ufbt vscode_dist` | Generate VS Code project config |
| `./fbt fap_claude_remote` | Build specific app (full firmware tree) |
| `./fbt launch APPSRC=applications_user/claude_remote` | Build & upload (full tree) |

## FAP Assets

- Images must be 1-bit (monochrome) PNGs.
- Place in the directory specified by `fap_icon_assets`.
- Access in code via: `#include "claude_remote_icons.h"`
- Follow Flipper naming conventions for animated icons.

## How FAPs Execute

1. App Loader copies `.fap` from SD card to RAM.
2. Verifies integrity and API version compatibility.
3. Processes relocations using firmware symbol table.
4. Resolves imported symbols to concrete addresses.
5. Starts execution.

**Note:** Available heap is reduced vs firmware-built apps since FAP code lives in RAM.

## API Versioning

- Firmware maintains `api_symbols.csv` with available functions/variables.
- Semantic versioning: major (breaking changes) / minor (additions).
- FAPs declare minimum API version; loader rejects incompatible apps.

## Debugging

```bash
./fbt          # build firmware
./fbt flash    # flash to device
./fbt debug    # attach gdb
```

Debug and release builds must match between firmware and app.
