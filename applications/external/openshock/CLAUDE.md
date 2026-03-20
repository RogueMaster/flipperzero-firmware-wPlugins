# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Flipper Zero external application (FAP) for controlling OpenShock shockers via 433 MHz OOK Sub-GHz. Written in C using the FURI framework and Flipper Zero SDK. Supports 5 shocker models ported from the [OpenShock ESP32 firmware](https://github.com/OpenShock/Firmware).

## Build

Requires [UFBT](https://github.com/flipperdevices/flipperzero-ufbt) (installed via `pip install ufbt`).

```bash
python -m ufbt        # Build the FAP
python -m ufbt launch # Build and launch on connected Flipper Zero
```

No test tooling is configured. Lint via `python -m ufbt lint`. Builds use `-Werror`.

## Architecture

- **`application.fam`** — FAP manifest (ID: `openshock_app`, category: Sub-GHz, requires: `gui`, `storage`, libs: `subghz`, stack: 8 KB)
- **`openshock.c`** — Main app: ViewPort-based UI with four screens (List, Transmit, Edit, Receive). Mutex-protected state, flash messages for feedback.
- **`protocols.h` / `protocols.c`** — OOK protocol encoders and decoders for all 5 shocker models. Each encoder converts command parameters into an array of `OokPulse` (high/low microsecond timing pairs). Decoders use sliding-window matching with 30% timing tolerance. Ported from the ESP32 firmware's RMT-based encoders.
- **`transmit.h` / `transmit.c`** — Sub-GHz transmission using `furi_hal_subghz` API with the internal CC1101 radio. Configures OOK 650kHz preset at 433.92 MHz, feeds pulse timings via async TX callback. Background thread with message queue.
- **`receiver.h` / `receiver.c`** — Sub-GHz reception. ISR callback accumulates OOK pulses into a ring buffer (256 max), background thread decodes on silence gaps (>5 ms).
- **`storage.h` / `storage.c`** — Save/load shocker configurations to SD card as `.shk` files using Flipper Format. Up to 32 saved shockers in `APP_DATA_PATH("shockers")`.
- **`images/`** — PNG assets auto-compiled into `<openshock_app_icons.h>` by UFBT
- **`screenshots/`** — Screenshots and GIF for README documentation

## Supported Shocker Models

| Model | Commands | Notes |
|-------|----------|-------|
| CaiXianlin | Shock, Vibrate, Sound, Light | 4-bit channel, intensity 0-99 |
| Petrainer | Shock, Vibrate, Sound | No channel select, intensity 0-100 |
| Petrainer998DR | Shock, Vibrate, Sound, Light | CH1/CH2 via channel param |
| T330 (WellturnT330) | Shock, Vibrate, Sound | CH1/CH2 via channel param |
| D80 | Shock, Vibrate, Sound | CH1/CH2/Both, 4-bit intensity (auto-scaled) |

Protocol details are documented in the [OpenShock wiki](https://wiki.openshock.org/hardware/shockers/caixianlin/#rf-specification) and the [firmware encoder sources](https://github.com/OpenShock/Firmware).

## Threading Model

- **Main thread**: GUI, input handling, state management (mutex-protected)
- **TX thread**: Background async transmission via message queue (`transmit.c`)
- **RX thread**: Background packet decoding from ISR-collected pulses (`receiver.c`)

## Key Flipper Zero Conventions

- Entry point signature: `int32_t openshock_app(void* p)`
- Core APIs: `<furi.h>` (OS primitives), `<furi_hal.h>` (hardware), `<gui/gui.h>` (UI framework)
- Sub-GHz: use `furi_hal_subghz_*` functions for radio control
- Stack size is 8 KB (set in `application.fam`)

## CI

GitHub Actions workflow (`.github/workflows/build.yml`) builds and lints FAPs for both dev and release SDK channels using `flipperdevices/flipperzero-ufbt-action@v0.1`. Runs on push to master, PRs, and a daily schedule. Pushes/PRs that only change `*.md` files are ignored.
