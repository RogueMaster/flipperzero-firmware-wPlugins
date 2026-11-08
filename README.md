# CAN Commander

CAN Commander is a CAN bus toolkit for vehicle reverse engineering, diagnostics and signal injection. An ESP32 board does all of the CAN work; you drive it from a Flipper Zero app, or from a command line over USB or Wi-Fi.

- Buy the official board: <https://rabbit-labs.com/product/cancommander/>
- Firmware downloads and the DBC profile converter: <https://www.cancommander.com>

## Documentation

Read these in order:

1. **[Flipper app guide](CAN_Commander_FlipperZero/can_commander/README.md)**: using the app. Menus, step-by-step workflows, dashboards, profiles, Wi-Fi CLI setup.
2. **[Firmware reference](CAN_Commander_Firmware/CAN_Commander_Main/README.md)**: building and flashing the ESP32, hardware configuration, every CLI command and tool, and the Flipper UART protocol. A shorter [firmware quickstart](CAN_Commander_Firmware/CAN_Commander_Main/README_quickstart.md) covers the first session.
3. **[Build your own](CAN_Commander_Firmware/CAN_Commander_Main/docs/BUILD_YOUR_OWN.md)**: a single-bus CAN Commander from an ESP32-S3 dev board and an SN65HVD230 transceiver.

Reference formats for the Flipper app's saved profiles: [DBC decoding profiles (`.dbcprof`)](CAN_Commander_FlipperZero/can_commander/README_DBC_CONFIG_FORMAT.md) and [Smart Injection profiles (`.injprof`)](CAN_Commander_FlipperZero/can_commander/README_SLOT_SET_FORMAT.md). Release notes: [app changelog](CAN_Commander_FlipperZero/can_commander/CHANGELOG.md).

## How it works

```text
Flipper Zero app ---- UART ---->  +------------------+ ---> CAN0: built-in TWAI controller + transceiver
USB serial CLI   --------------> |  ESP32 firmware  |
Wi-Fi TCP CLI    --------------> +------------------+ ---> CAN1 (optional): MCP2517FD/MCP2518FD or MCP2515
```

- **The firmware does the CAN work**: receiving and filtering frames, decoding signals, transmitting, timing replays and OBD-II requests. It runs one tool at a time.
- **The Flipper app is the main interface.** It sends commands over the Flipper's GPIO UART, shows live dashboards, and stores profiles on the Flipper's SD card. It also powers the board from the Flipper's 5V pin.
- **The command line** offers the same tools as text commands over USB, or over a Wi-Fi access point the board can host (off by default). It suits laptops, phones and scripts.

## Features

### Monitor and discovery
- **Read all frames** and **filtered read** (ID mask/filter) with live frame views.
- **Unique IDs**: lists every ID seen on the bus.
- **CAN speed test**: messages per second.
- **Value tracker** and **bit tracker**: watch byte values and individual bits of one message change as you operate the vehicle.
- **Auto reverse engineer**: learns the bus's background noise, then shows only what changes when you perform an action. It jumps straight into the bit tracker or replay for the ID you pick.

### Control and injection
- **Write frames**: one-shot, burst or periodic transmission.
- **Smart Injection**: five slots that track the latest live frame for an ID and resend it with only your chosen bits, bytes or fields changed, so the rest of the message stays authentic. Supports multiplexed messages.
- **Replay**: records up to 384 frames of one ID and plays them back with the original timing.

### Vehicle diagnostics (OBD-II)
- Live PID polling (RPM, speed, coolant temperature, throttle, load, fuel level and more, or any PID by number).
- Read the VIN.
- Read stored, pending and permanent trouble codes from every responding module, and clear them.

### DBC decoding
- Live decoding of signals from DBC definitions: Intel and Motorola byte order, signed values, scaling, multiplexed signals.
- Load curated `.dbcprof` profiles on the Flipper (convert a `.dbc` file at <https://www.cancommander.com>), with per-signal dashboards and value-to-label mapping.

### Interfaces and hardware
- Flipper Zero app with dashboards for every tool.
- USB and Wi-Fi command line.
- One or two CAN buses, normal or listen-only mode. Bitrates from 5 kbit/s to 1 Mbit/s, including 33.3k and 83.3k.
- Status LEDs on the official board.

### Where each tool lives

| Flipper app (Tools menu) | Firmware tool | Purpose |
|---|---|---|
| Monitor & Discovery > Read All Frames | `read_all` | Stream every frame |
| Monitor & Discovery > Filter & Read Frames | `filtered` | Stream frames matching an ID mask/filter |
| Monitor & Discovery > Unique IDs | `unique_ids` | List IDs seen on the bus |
| Monitor & Discovery > Bit Tracker | `bittrack` | Bit-level changes for one ID |
| Monitor & Discovery > Auto Reverse Engineer | `reverse` | Find which IDs/bytes an action changes |
| Monitor & Discovery > Value Tracker | `valtrack` | Byte value changes and min/max for one stream |
| Monitor & Discovery > CAN Speed Test | `speed` | Messages per second |
| Control & Injection > Write Frames | `write` | Transmit a frame once, in a burst, or periodically |
| Control & Injection > Smart Injection | `custom_inject` | Modify and resend live frames from five slots |
| Control & Injection > Replay | `replay` | Record and replay one ID with original timing |
| Vehicle Diagnostics | `obd_pid` | OBD-II PIDs, VIN, read/clear DTCs |
| DBC & Databases > DBC Decode | `dbc_decode` | Decode signals from loaded DBC definitions |

## Hardware

| | Official CAN Commander board | Build your own |
|---|---|---|
| Microcontroller | ESP32-S3 | ESP32-S3 dev board (classic ESP32 also supported) |
| CAN buses | 2: CAN0 (right port) and CAN1 (left port, MCP2517FD/MCP2518FD) | 1: CAN0 |
| Transceiver | On board | SN65HVD230 module |
| Status LEDs | 2 | Optional |
| Flipper connection | Plugs onto the GPIO header | Four jumper wires |
| Get one | [Buy from Rabbit Labs](https://rabbit-labs.com/product/cancommander/) | [Build guide](CAN_Commander_Firmware/CAN_Commander_Main/docs/BUILD_YOUR_OWN.md) |

The firmware also supports other combinations, such as an MCP2515 module as the second bus or custom pin assignments. See "Hardware and Defaults" in the [firmware reference](CAN_Commander_Firmware/CAN_Commander_Main/README.md).

## Getting started

### Official board with a Flipper Zero
The official board is available from [Rabbit Labs](https://rabbit-labs.com/product/cancommander/).

1. Install the Flipper app (see [Building the Flipper app](#building-the-flipper-app)).
2. Plug the board onto the Flipper's GPIO header and connect the CAN wires. The right port is the primary bus (CAN0).
3. Open **Apps > GPIO > CAN Commander**. The app powers the board, connects and checks the firmware version.
4. Follow the [Flipper app guide](CAN_Commander_FlipperZero/can_commander/README.md), starting with a read-only workflow.

### Building the Flipper app
The app is a standard [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) project:

```bash
cd CAN_Commander_FlipperZero/can_commander
ufbt            # builds dist/can_commander.fap
ufbt launch     # installs and starts it on a Flipper connected over USB
```

### Building and flashing the firmware
The firmware is an Arduino sketch for the ESP32 Arduino core 3.3.x. For the official board:

```bash
cd CAN_Commander_Firmware/CAN_Commander_Main
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc .
arduino-cli upload  --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc -p <PORT> .
```

Required libraries, other hardware configurations and the full command reference are in the [firmware reference](CAN_Commander_Firmware/CAN_Commander_Main/README.md).

### Building your own board
Follow the [build guide](CAN_Commander_Firmware/CAN_Commander_Main/docs/BUILD_YOUR_OWN.md). It covers parts, wiring, the firmware build for a single-bus board, and a first test.

## Repository layout

```text
CAN_Commander_FlipperZero/
  can_commander/                    Flipper Zero app (ufbt project)
    README.md                       App guide
    README_DBC_CONFIG_FORMAT.md     .dbcprof (DBC decoding profile) format
    README_SLOT_SET_FORMAT.md       .injprof (Smart Injection profile) format
    CHANGELOG.md                    App release notes
    LICENSE                         GPL-3.0
    can_commander.c/.h              App core
    libraries/                      UART client for the firmware protocol
    scenes/, views/                 Menus and dashboards

CAN_Commander_Firmware/
  CAN_Commander_Main/               ESP32 firmware (Arduino sketch)
    README.md                       Firmware reference
    README_quickstart.md            First-session quickstart
    docs/BUILD_YOUR_OWN.md          DIY single-bus build guide
    src/                            Firmware source; hardware settings in src/BoardConfig.h
    test/host/                      Host regression tests (test/host/run.sh)
    scripts/build_matrix.sh         Builds every supported hardware configuration
    LICENSE.md                      PolyForm Noncommercial 1.0.0
    THIRD_PARTY_NOTICES.md          Libraries the firmware is built with
```

## Compatibility

| Component | Version |
|---|---|
| Flipper app | v2.3.1 |
| ESP32 firmware | v2.3 |
| Minimum firmware the app accepts | v2.2 (older firmware triggers an in-app warning) |
| ESP32 Arduino core | 3.3.x |

## Safety

CAN Commander can transmit on a live vehicle network. Injected or replayed frames can set fault codes, light warnings, or operate real vehicle functions. Start with read-only tools (or the firmware's listen-only mode), test transmissions on a bench or a parked vehicle, and stop transmitting tools when you are done. If you enable the Wi-Fi command line, set your own password first: anyone who joins the access point gets full control, including transmitting.

## License

Each part of the repository has its own license:

| Folder | License | In short |
|---|---|---|
| [`CAN_Commander_FlipperZero/`](CAN_Commander_FlipperZero/can_commander/LICENSE) | GNU GPL v3.0 | Free to use, modify and redistribute; modified versions you distribute must also be GPL-3.0 with source. |
| [`CAN_Commander_Firmware/`](CAN_Commander_Firmware/CAN_Commander_Main/LICENSE.md) | PolyForm Noncommercial 1.0.0 | Source-available. Free for personal, hobby, research, educational and other noncommercial use; selling devices that run the firmware, or other commercial use, requires permission from the author. |

The firmware is built with third-party libraries under their own licenses; see [THIRD_PARTY_NOTICES.md](CAN_Commander_Firmware/CAN_Commander_Main/THIRD_PARTY_NOTICES.md).
