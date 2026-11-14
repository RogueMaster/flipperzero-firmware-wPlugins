# Architecture

[← Documentation index](README.md)

## Contents

- [Overview](#overview)
- [Repository Layout](#repository-layout)
- [Flipper App Data Flow](#flipper-app-data-flow)
- [Build Outputs](#build-outputs)
- [Cleaning Up](#cleaning-up)

## Overview

```text
[target host] ──RJ45── [W5500] ──SPI──┐
                                      ├─ [ESP32-S3] ══UART══ [Flipper Zero]
[target host] ~~WiFi AP~~ (built-in) ─┘   TCP/IP (lwIP)       UI, hosts,
                                          SSH (libssh)        scripts, viewer
```

The ESP32-S3 does all networking and cryptography; the Flipper app is the user interface and
keeps hosts, keys and scripts on the SD card. They talk over UART with the protocol in
[PROTOCOL.md](PROTOCOL.md).

## Repository Layout

| Path | What |
|---|---|
| `proto/` | Shared UART codec: COBS framing + CRC-16, typed payload codecs, stop-and-wait reliability. Spec: [PROTOCOL.md](PROTOCOL.md). |
| `flipper-app/` | The SSH Runner app (uFBT). `lib/proto/` is a committed copy of `proto/`, because uFBT compiles only files below the app directory; `tools/sync_proto.py` updates it and the checks fail if it drifts. |
| `flipper-app/core/` | Furi-free logic, host-tested: host, recent and command files, ANSI stripping, output buffer, backend interface, mock backend, session state machine. |
| `flipper-app/scenes/`, `views/` | One scene per screen (SceneManager + ViewDispatcher); custom Home view and keyboard. |
| `flipper-app/backend_uart.c` | Real backend: `proto/` over USART1, Expansion disabled while the app runs. |
| `firmware-esp32/` | ESP-IDF project for the ESP32-S3. |
| `firmware-esp32/components/` | `libssh` (vendored), `net` (WiFi + W5500), `ssh` (client task), `keystore` (NVS + host-key policy), `link` (UART framing + request validation), `bootinfo` (boot counter and reset reasons). |
| `firmware-esp32/main/` | Flipper request handler (`app.c`), USB console (`console.c`). |
| `cli/` | `sshrunner`, the PC tool (Go). |
| `tools/check.ps1`, `tools/check.sh` | Build and test everything (Windows / Linux, WSL); CI runs the same steps. |
| `tools/flash.py` | Flash the firmware build with esptool; refuses any image that would burn eFuses. |
| `tools/flipper/` | RPC tooling for on-device tests, read-only by default; every session is snapshot, test, restore, verify. |
| `tools/hil/` | Hardware-in-the-loop tests driven from the ESP32's USB console against a test sshd. |
| `hardware/` | The add-on board: KiCad 10 sources, fabrication files and their tools ([`hardware/README.md`](../hardware/README.md)). |

## Flipper App Data Flow

```text
scenes ──calls──▶ Backend (mock | uart) ──events──▶ backend_event() ──▶ Session (core/session.c)
   ▲                                                                       │ change bits
   └──────────── EvtChanged on the GUI thread, every 20 ms tick ◀──────────┘
```

All backend events are delivered on the GUI thread (the tick pumps the backend), so scenes
never need locks. Output credit for `STREAM_DATA_EVT` is granted from the same tick.

## Build Outputs

`tools/check.sh` (or `tools\check.ps1`) builds everything below.

- **Flipper app:** `flipper-app/dist/unleashed/ssh_runner.fap` and
  `flipper-app/dist/official/ssh_runner.fap`. Next to each, `ssh_runner_demo.fap` is the demo
  build for [demo mode](../flipper-app/README.md#without-the-board) (a simulated board, used
  for the screenshots); it is not part of a release.
- **ESP32 firmware:** built in the pinned `espressif/idf:v5.5.6` container into
  `.cache/build/esp32/`. Flash it with `tools/flash.py flash --port <port>`, or with esptool
  directly: `esptool.py --chip esp32s3 -p <port> write_flash @flash_args` from that folder.
- **PC tool:** `cli/` (Go); `cli/scripts/package.sh` packages it for every platform into
  `.cache/cli-dist/`.
- **README images:** `tools/readme_media.py` builds `docs/media/hero.png` and
  `docs/media/demo.gif` from the screenshots in `docs/screenshots/`.
- **PC tool figures:** `tools/cli_figures.py` captures `sshrunner` on demo data in a terminal
  (in the pinned `golang` container) and draws `docs/media/cli/*.svg` with Charm's freeze.
- **On-device UI tests:** `tools/flipper/session.py` (read-only unless `--allow-write`).

## Cleaning Up

Everything the build creates stays in two folders inside the repository, and outside it only
in Docker:

- `venv/`: the Python virtual environment.
- `.cache/`: toolchains, build trees and caches; delete it at any time, the next check run
  rebuilds it.
- `docker rmi espressif/idf:v5.5.6 golang:1.27.2`: the pinned toolchain images (about 10 GB;
  `golang` is only pulled when Go is not installed).

## See Also

- [UART Protocol](PROTOCOL.md): the messages between the app and the board
- [Security](SECURITY.md): how secrets are stored
