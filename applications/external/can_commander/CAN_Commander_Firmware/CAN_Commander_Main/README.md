# CAN Commander Firmware

**Firmware version:** `2.3` (defined in `src/BoardConfig.h` as `FIRMWARE_VERSION_STRING`)

CAN Commander is an ESP32 firmware for dual-bus CAN exploration. It runs on the official CAN Commander board (ESP32-S3) and on your own hardware (ESP32-S3 or classic ESP32 with a CAN transceiver, plus an optional MCP2515 or MCP2517FD/MCP2518FD second controller). It provides:
- USB CDC CLI (`Serial`) for developer control
- Optional Wi-Fi AP CLI (`WiFiServer`, TCP) for remote control
- Flipper UART control/data protocol (`Serial1`)
- Modular tool system (one active tool at a time) with 12 tools: traffic capture, filtered read, periodic write, speed, value/bit tracking, unique-ID discovery, reverse-engineering, OBD-II PID/VIN/DTC, DBC decode, custom injection, and record/replay
- DBC signal descriptor loading + live decode
- NeoPixel status LEDs with runtime brightness control
- Version reporting via CLI (`ver`) and Flipper (`GET_INFO` -> `STATUS_EVT`)

## Hardware and Defaults
All hardware settings live in `src/BoardConfig.h` as `BOARDCFG_*` macros. Edit the defaults there, or override any of them at build time (see [Building](#building)).

Building your own single-bus board from an ESP32-S3 dev board and an SN65HVD230 transceiver: see [docs/BUILD_YOUR_OWN.md](docs/BUILD_YOUR_OWN.md).

### Supported targets and default pins

| Setting | ESP32-S3 (official board) | Classic ESP32 (DevKit) |
|---|---|---|
| CAN0 (built-in TWAI) TX / RX | `BOARDCFG_CAN0_TX_PIN` 2 / `BOARDCFG_CAN0_RX_PIN` 1 | 5 / 4 |
| CAN1 backend `BOARDCFG_CAN1_DRIVER` | 1 = MCP2517FD/MCP2518FD | 0 = none |
| CAN1 SPI CS / MOSI / SCK / MISO | 10 / 11 / 12 / 13 | 15 / 23 / 18 / 19 |
| CAN1 interrupt `BOARDCFG_CAN1_INT_PIN` | 255 (polled) | 255 (polled) |
| Flipper UART TX / RX | `BOARDCFG_FLIPPER_UART_TX_PIN` 43 / `BOARDCFG_FLIPPER_UART_RX_PIN` 44 | 17 / 16 |
| NeoPixels `BOARDCFG_NEOPIXEL_ENABLED` | 1 (pin 8, 2 LEDs) | 0 |

Other ESP32 targets have no default pin map; define every pin macro listed in the `#error` message `BoardConfig.h` prints.

CAN1 backends (`BOARDCFG_CAN1_DRIVER`):
- `0` none: CAN0 only.
- `1` MCP2517FD/MCP2518FD via ACAN2517FD. `BOARDCFG_CAN1_CRYSTAL_MHZ` = `4` (uses the 10x PLL), `20` (default) or `40`.
- `2` MCP2515 via autowp-mcp2515. `BOARDCFG_CAN1_CRYSTAL_MHZ` = `8` (default), `16` or `20`.

The build fails with a clear message if pins collide (CAN0, CAN1 SPI, Flipper UART, NeoPixel), if a classic ESP32 pin is one of the flash pins GPIO6-11, or if the Flipper UART would share UART0 with the USB CLI.

Other defaults:
- Bitrates: CAN0 and CAN1 start at `500000`, normal (transmitting) mode.
  - CAN0 supports 1000k, 800k, 500k, 250k, 200k, 125k, 100k, 83.333k (`83333`), 80k, 50k, 40k, 33.333k (`33333`), 31.25k, 25k, and on ESP32-S3 also 20k, 10k and 5k.
  - MCP2515 supports the autowp-mcp2515 rates for its crystal (8 MHz: 5k-1000k without 83.3k/95k; 16 MHz: all, including `83333` and `95238`; 20 MHz: 33.3k and up).
  - MCP2517FD/MCP2518FD accepts any rate the ACAN2517FD bit-timing calculator can reach.
  - An unsupported bitrate is rejected before the bus is touched; the current configuration stays active.
- USB CLI: 115200 baud (USB CDC on ESP32-S3, UART0 on classic ESP32).
- Flipper UART: 921600 baud, 8N1.
- Wi-Fi AP CLI: compiled in (`BOARDCFG_WIFI_AP_ENABLED=1`) but **not started** by default (`BOARDCFG_WIFI_AP_AUTOSTART=0`). SSID `CAN_Commander`, password `cancommander`, TCP port `7777`. Anyone who joins the AP gets full CLI control, including CAN transmit, so change the password (`wifi set pass=...`) before enabling it.
- DBC signal capacity: `128`. Output queue depth per sink (CLI/Flipper): `256`.

### Building

Requirements:
- Arduino CLI or Arduino IDE with the **esp32 core 3.3.x** (tested 3.3.4).
- Libraries (Library Manager): **ACAN2517FD** (tested 2.1.16), **autowp-mcp2515** (tested 1.3.1), **Adafruit NeoPixel** (tested 1.15.4). Only the libraries for the backends you enable are used.
- ESP32-S3: enable **USB CDC On Boot** (FQBN option `CDCOnBoot=cdc`). Without it, the USB CLI is UART0 on GPIO43/44, the Flipper UART pins, and the build stops with an explanation.

```bash
# Official board (ESP32-S3)
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc .

# Classic ESP32 DevKit with a transceiver on GPIO5/4
arduino-cli compile --fqbn esp32:esp32:esp32 .

# Overrides: pass BOARDCFG_* macros through compiler.cpp.extra_flags
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc \
  --build-property "compiler.cpp.extra_flags=-DBOARDCFG_CAN1_DRIVER=2 -DBOARDCFG_CAN1_CRYSTAL_MHZ=8 -DBOARDCFG_NEOPIXEL_ENABLED=0" .
```

Do not override `build.extra_flags`: it replaces the board's own USB/CDC flags.

Example configurations (macros to set):
```text
CAN0 only, no LEDs:         BOARDCFG_CAN1_DRIVER=0 BOARDCFG_NEOPIXEL_ENABLED=0
CAN0 + MCP2515, 8 MHz:      BOARDCFG_CAN1_DRIVER=2 BOARDCFG_CAN1_CRYSTAL_MHZ=8
CAN0 + MCP2518FD, 40 MHz:   BOARDCFG_CAN1_DRIVER=1 BOARDCFG_CAN1_CRYSTAL_MHZ=40
Custom SPI pins:            BOARDCFG_SPI_CS_PIN=14 BOARDCFG_SPI_SCK_PIN=15 BOARDCFG_SPI_MISO_PIN=16 BOARDCFG_SPI_MOSI_PIN=17
Custom Flipper UART pins:   BOARDCFG_FLIPPER_UART_TX_PIN=17 BOARDCFG_FLIPPER_UART_RX_PIN=18
Wi-Fi compiled out:         BOARDCFG_WIFI_AP_ENABLED=0
```

`scripts/build_matrix.sh` compiles the supported configurations, and `test/host/run.sh` runs the host regression tests (decoding, parsing, OBD, tools, protocol).

Boot behavior when CAN1 is configured but not detected: CAN0 keeps working and the boot log prints a `WARN: CAN1 ... not available` line. Build with `BOARDCFG_CAN1_DRIVER=0` if your board has no second controller.

Behavior when CAN1 is disabled or missing:
- `bus=can1` commands and tools return an error (`bus can1 unavailable`).
- `bus=both` applies only to available buses.


## Status LEDs (NeoPixel)
When `NEOPIXEL_ENABLED=1`, two NeoPixel LEDs indicate system state.

**LED 0 (status):**
| State | Color |
|-------|-------|
| CAN0 unavailable (init failed or recovering) | Blinking red (500ms) |
| Boot | Amber (700ms) |
| Tool active + Wi-Fi client | Alternating green/blue (600ms) |
| Tool active | Solid green |
| Wi-Fi client connected | Blinking blue (600ms) |
| Wi-Fi AP up, no client | Solid blue |
| Idle (no AP) | Dim white |

**LED 1 (activity):**
| Event | Color | Duration |
|-------|-------|----------|
| CAN RX | Blue | 70ms |
| CAN TX | Red | 70ms |
| Queue drop | Amber | 150ms |

Brightness is adjustable at runtime:
- CLI: `led set brightness=<1-10>` (1 = 10%, 10 = 100%, default 5)
- Flipper: `LED_SET_CFG` (`0x60`), payload `[0] brightness:u8`

## Repo Layout
- Main sketch: `CAN_Commander_Main.ino` (only calls `App::setup()`/`App::loop()`)
- Hardware configuration: `src/BoardConfig.h`
- Core app runtime: `src/App.h`, `src/App.cpp`; CAN backends in `src/can/`, tools in `src/tools/`
- Host regression tests: `test/host/` (`test/host/run.sh`)
- Build matrix: `scripts/build_matrix.sh`
- License: `LICENSE.md` (PolyForm Noncommercial 1.0.0), third-party notices in `THIRD_PARTY_NOTICES.md`

## Runtime Model
At boot, firmware initializes buses and stays idle until commands arrive.

Main loop order:
1. Parse CLI + Flipper input
2. Dispatch commands
3. Poll CAN buses
4. Feed active tool
5. Flush output queues (bounded and non-blocking; see [Backpressure / Drops](#backpressure--drops))
6. Update LEDs

### Ownership Rules
- One owner for mutating commands: `NONE`, `CLI`, or `FLIPPER`
- One active tool globally
- Flipper can preempt CLI tool sessions
- CLI cannot mutate while Flipper owns session
- Exception: while `custom_inject` is active, CLI `inject ...`, `inject list`, and `inject cancel`
  are allowed even if Flipper owns the session (shared control for injection trigger path)
- Flipper ownership auto-releases after 10s without valid packets

Read-only commands while locked are still allowed:
- CLI: `help`, `status`, `ver`, `stats`, `bus show`, `tool status`, `dbc list`, `led show`, `wifi show`
- Flipper: `PING`, `GET_INFO`, `BUS_GET_CFG`, `TOOL_STATUS`, `DBC_LIST`, `STATS_GET`, `WIFI_GET_CFG`

## USB CLI

### Command Format
- Commands are newline-delimited, up to 191 characters. A longer line is discarded entirely and prints `ERR: line too long`.
- Arguments are `key=value` tokens separated by spaces.
- Hex values are parsed as base16 for relevant fields (`id`, `mask`, `filter`, etc.).
- Values are validated strictly: the whole value must parse (`id=7FFx`, `data=1G`, `count=-1`, `ext=2` are rejected), and values never get silently shortened. An invalid optional argument rejects the command instead of falling back to a default; for example `bus=can2` is an error, not CAN0.
- Standard IDs must be `<= 7FF` and extended IDs `<= 1FFFFFFF`.
- A failed command prints `ERR: <STATUS> (<reason>)`, e.g. `ERR: BAD_ARG (missing or invalid argument)`. Some commands print a more specific line first.
- If CAN1 is disabled in `BoardConfig.h`, `can1` target commands fail and `both` behaves as CAN0-only.

### Quick Start

```text
help
status
ver
bus set bus=can0 bitrate=500000 mode=normal
tool start read_all bus=both ascii=0
tool stop
tool start replay bus=can0 id=2C1
tool config cmd=record
tool config cmd=stop
tool config cmd=replay
led set brightness=5
```

### Full CLI Commands
- `help`
- `status`
- `ver` (alias: `version`): prints the firmware version
- `bus set bus=<can0|can1|both> bitrate=<int> [mode=<normal|listen>]` (supported bitrates: see [Hardware and Defaults](#hardware-and-defaults); an unsupported rate leaves the bus unchanged)
- `bus show`
- `bus filter set bus=<can0|can1> mask=<hex> filter=<hex> ext_match=<0|1> ext=<0|1>`
- `bus filter clear bus=<can0|can1>`
- `send bus=<can0|can1> id=<hex> ext=<0|1> rtr=<0|1> dlc=<0-8> data=<hexbytes>` (`data` may not be longer than `dlc`)
- `tool start <tool_name> [tool_args]` (if the new tool rejects its arguments, the running tool keeps running)
- `tool stop`
- `tool status`
- `tool config <args>`
- `inject slot=<1-5> [count=<int>] [interval_ms=<int>]` (shortcut for `tool config cmd=inject ...`)
- `inject list` (shortcut for `tool config cmd=list`)
- `inject cancel` (shortcut for `tool config cmd=cancel`)
- `wifi show`
- `wifi start` | `wifi stop`
- `wifi set ssid=<text_no_spaces> pass=<text_no_spaces> [autostart=<0|1>] [ap=<0|1>]` (SSID 1-32 chars, password 8-63 chars; nothing is changed unless every field is valid)
- `wifi config ssid=<text_no_spaces> pass=<text_no_spaces> autostart=<0|1> ap=<0|1>`
- `dbc clear`
- `dbc add bus=<can0|can1> id=<hex> ext=<0|1> start=<0-63> len=<1-64> order=<intel|motorola> sign=<u|s> factor=<float> offset=<float> min=<float> max=<float> unit=<text> sid=<int> [mux_start=<0-63> mux_len=<1-64> mux_value=<int|0xhex>]`
- `dbc remove sid=<int>`
- `dbc list`
- `stats` (also reports CAN frames lost before reaching tools, skipped output, Flipper RX errors and free heap)
- `led show`
- `led set brightness=<1-10>`

## Wi-Fi AP CLI (Optional)

The AP and TCP CLI are compiled in by default (`BOARDCFG_WIFI_AP_ENABLED=1`) but do **not start** until you ask (`BOARDCFG_WIFI_AP_AUTOSTART=0`). Start it once with `wifi start`, or persist it with `wifi set autostart=1`. Build-time defaults:

```cpp
#define BOARDCFG_WIFI_AP_ENABLED 1        // 0 compiles Wi-Fi out entirely
#define BOARDCFG_WIFI_AP_SSID "CAN_Commander"
#define BOARDCFG_WIFI_AP_PASSWORD "cancommander"
#define BOARDCFG_WIFI_CLI_PORT 7777
#define BOARDCFG_WIFI_AP_AUTOSTART 0
```

Security: the Wi-Fi CLI has the same authority as USB, including CAN transmit, and the default password is public. Set your own password (`wifi set pass=<8-63 chars>`) before enabling the AP. Settings are stored in NVS.

Boot log prints AP IP and CLI port once started:

```text
WiFi AP ready ssid=<ssid> ip=<ap_ip> cli_port=<port>
```

Connect a phone/laptop to the AP, then open a raw TCP client (for example `nc <ap_ip> 7777`) to `<ap_ip>:<port>`.
Commands are the same CLI commands as USB, including injection shortcuts once `custom_inject` is active.
One client at a time. A client that stops reading for 5 seconds is disconnected so it cannot stall the firmware; lines it could not accept in time are skipped (counted in `stats`).

Runtime config/persistence:
- `wifi set` / `wifi config` updates SSID/password/autostart and stores to NVS.
- `wifi start` / `wifi stop` controls AP immediately.
- `autostart=1` means AP comes up on next power cycle; `autostart=0` keeps AP off until started.

Example trigger flow (profile already loaded):

```text
inject slot=1
inject slot=2 count=5 interval_ms=100
inject list
inject cancel
```

## Tool Reference
Only one tool can be active at a time. Starting a new tool stops the previous one.

### 1) `read_all`
Streams all frames from selected bus.

Args:
- `bus=<can0|can1|both>` (default `both`)
- `ascii=<0|1>` (default `0`)

Example:
```text
tool start read_all bus=both ascii=1
```

### 2) `filtered`
Streams frames matching `(id & mask) == (filter & mask)`.

Args:
- `bus=<can0|can1|both>`
- `mask=<hex>` (default `7FF`)
- `filter=<hex>` (default `0`)
- `ext_match=<0|1>` (default `0`)
- `ext=<0|1>` (default `0`)
- `ascii=<0|1>`

Example:
```text
tool start filtered bus=can0 mask=7FF filter=273 ext_match=0 ascii=0
```

### 3) `write`
Sends one-shot or periodic TX frame.

Args:
- `bus=<can0|can1>` (default `can0`; `both` is rejected)
- `id=<hex>` (required; `<= 7FF` standard, `<= 1FFFFFFF` with `ext=1`)
- `ext=<0|1>`
- `rtr=<0|1>`
- `dlc=<0-8>` (default `8`; raised to the `data` length if `data` is longer)
- `data=<hexbytes>` (no spaces, e.g. `11223344AABBCCDD`; missing bytes are 0)
- `period_ms=<int>` (optional; `0`/omitted = send once)
- `count=<int>` (optional, `>= 1`; requires `period_ms>0` when greater than 1; omitted with `period_ms>0` = continuous until `tool stop`)

Examples:
```text
tool start write bus=can0 id=1D5 dlc=8 data=102030AABBCCDDEE
tool start write bus=can1 id=18DAF110 ext=1 dlc=8 data=02010C0000000000 period_ms=100 count=50
```

### 4) `speed`
Reports `msg/sec` once per second.

Args:
- `bus=<can0|can1|both>` (default `both`)

Example:
```text
tool start speed bus=can0
```

### 5) `valtrack`
Tracks byte changes and min/max for one frame stream.

Args:
- `bus=<can0|can1|both>`
- `id=<hex>` and `ext=<0|1>` (optional): the stream to track
- `strict=<0|1>` (default `1`)
  - With `id=`: tracks that ID; a DLC change resets the tracker (including min/max).
  - Without `id=`, `strict=1`: locks onto the first frame seen (prints `valtrack locked id=0x...`) and ignores other IDs. Use a bus filter or `id=` to choose the stream.
  - Without `id=`, `strict=0`: compares every frame with the previous one, whatever its ID (use behind a bus filter).

Change lines: `byte <n> changed 0x<old>(<old>)->0x<new>(<new>)`.

Runtime config (`tool config`):
- `show=min`
- `show=max`
- `show=both`

Example:
```text
tool start valtrack bus=can0 id=1D5
tool config show=both
```

### 6) `unique_ids`
Tracks and reports unique CAN IDs. Standard/extended IDs and the two buses are tracked separately (up to 512 entries; a full table is reported once).

Args:
- `bus=<can0|can1|both>`

Runtime config:
- `snapshot=1` prints the full current ID set (`id=0x<ID> <std|ext> bus=<bus>` lines between `unique_id snapshot start` and `unique_id snapshot end`). Large snapshots are emitted over several loop iterations so none of it is lost to output queue overflow.

Example:
```text
tool start unique_ids bus=both
tool config snapshot=1
```

### 7) `bittrack`
For one target ID, emits byte binary representation when payload changes.

Args:
- `bus=<can0|can1|both>` (default `can0`)
- `id=<hex>` (required)
- `ext=<0|1>` (optional; omitted = both formats). Each bus and format is tracked separately.

Example:
```text
tool start bittrack bus=can0 id=273
```

### 8) `reverse`
Reverse-engineering helper with calibration + monitoring phases, plus read mode.
Tracks up to 256 distinct CAN IDs (per bus) in auto mode.

Monitoring output:
- `change id=0x<ID> b<n> <old>-><new>` for a calibrated-stable byte that changes (with ` bus=<bus>` appended when `bus=both`).
- `new id=0x<ID> <std|ext> during monitoring data: ...` once for an ID that was not seen during calibration.
- The calibration summary (`id=0x... stable: ...` lines after `reverse calibration complete`) is emitted over several loop iterations.

Args:
- `bus=<can0|can1|both>` (default `can0`)
- `calibration_ms=<int>` (default `10000`)
- `monitor_ms=<int>` (default `20000`)
- `mode=read` (optional)
- `id=<hex>` and `ext=<0|1>` for read mode
- `bytes=all` or `bytes=0,1,2` for read mode (indexes 0-7; anything else is rejected)

Runtime config:
- `restart=1` resets auto mode
- `exclude_pre=1` (or `exclude_pre_action=1`) disqualifies bytes that changed during early monitoring before your action
- `mode=read id=<hex> ext=<0|1> bytes=all|0,1,2`
- `mode=auto`

Examples:
```text
tool start reverse bus=can0 calibration_ms=10000 monitor_ms=20000
tool config exclude_pre=1
tool config mode=read id=273 ext=0 bytes=0,1,2
```

### 9) `obd_pid`
Supports periodic Mode 01 PID polling plus one-shot VIN/DTC operations over OBD (`0x7DF`).

Args:
- `bus=<can0|can1>` (default `can0`; `both` is rejected)
- `pid=<hex|name|special>` (default `0C`)
  Mode 01 names (case-insensitive): `rpm`, `speed`, `coolant`, `throttle`, `load`, `fuel`, `iat`, `baro`, `odometer`
  Special one-shot requests: `vin`, `dtc`/`dtc_all` (stored+pending+permanent), `dtc_stored`, `dtc_pending`, `dtc_permanent`, `clear_dtcs`
  DTC requests on functional ID (`7DF`) collect responses from all responding modules.
- `interval_ms=<int>` (default `250`, minimum `10`, Mode 01 only)
- `req_id=<hex>` request CAN ID override, 11-bit (default `7DF`; use `7E0` for strict physical addressing simulators/ECUs)

DTC output (one line per responding ECU): `<stored|pending|permanent> DTCs: P0301 P0420 ecu=0x7E8`, or `... DTCs: none ecu=0x7E8`. Codes beyond what fits on one line are summarized as `... DTCs: +<n> more`. Multi-frame (ISO-TP) responses from several ECUs at once are reassembled per ECU.

Runtime config:
- `request=1` re-send current one-shot request (or trigger immediate Mode 01 request)
- `interval_ms=<int>` update Mode 01 interval
- `req_id=<hex>` update request CAN ID

Example:
```text
tool start obd_pid bus=can0 pid=0C interval_ms=200
tool start obd_pid bus=can0 pid=RPM interval_ms=200
tool start obd_pid bus=can0 pid=speed interval_ms=200
tool start obd_pid bus=can0 pid=vin
tool start obd_pid bus=can0 pid=vin req_id=7E0
tool start obd_pid bus=can0 pid=dtc
tool start obd_pid bus=can0 pid=dtc_all
tool start obd_pid bus=can0 pid=dtc_stored
tool start obd_pid bus=can0 pid=dtc_pending
tool start obd_pid bus=can0 pid=dtc_permanent
tool start obd_pid bus=can0 pid=clear_dtcs
```

### 10) `dbc_decode`
Decodes live frames using loaded signal descriptors. Every descriptor stored for an ID is evaluated (including many mux variants). Remote (RTR) frames are not decoded.

Args:
- `bus=<can0|can1|both>` (default `both`)

Example:
```text
dbc add bus=can0 id=1D5 ext=0 start=0 len=16 order=intel sign=u factor=0.1 offset=0 min=0 max=6553.5 unit=kph sid=1
dbc add bus=can0 id=3C2 ext=0 start=8 len=8 order=intel sign=u factor=1 offset=0 min=0 max=255 unit=raw sid=2 mux_start=0 mux_len=4 mux_value=1
tool start dbc_decode bus=can0
```

### 11) `custom_inject`
Tracks up to 5 configured signals and stores each slot's latest live frame.
Injection uses the most recent tracked frame with your bit/byte modifications applied.

Start args:
- `bus=<can0|can1|both>` default bus for `cmd=add` when `bus` is omitted (default `can0`)

Runtime config (`tool config`):
- `cmd=add slot=<1-5> bus=<can0|can1|both> id=<hex> ext=<0|1> [name=<text_no_spaces>] [mask=<hexbytes>] [value=<hexbytes>] [xor=<hexbytes>] [mux_start=<0-63> mux_len=<1-64> mux_value=<int|0xhex>] [sig_start=<0-63> sig_len=<1-64> sig_value=<int|0xhex>]`
- `cmd=remove slot=<1-5>`
- `cmd=clear`
- `cmd=list`
- `cmd=modify slot=<1-5> [name=<text_no_spaces>] [name_clear=<0|1>] [mask=<hexbytes>] [value=<hexbytes>] [xor=<hexbytes>] [mux=<0|1>] [mux_start=<0-63> mux_len=<1-64> mux_value=<int|0xhex>] [sig_start=<0-63> sig_len=<1-64> sig_value=<int|0xhex>]`
- `cmd=bit slot=<1-5> bit=<0-63> value=<0|1>` (bit index: byte0 LSB is bit 0)
- `cmd=field slot=<1-5> start=<0-63> len=<1-64> value=<int|0xhex>` (easy multi-bit setter)
- `cmd=clearbit slot=<1-5> bit=<0-63>`
- `cmd=inject slot=<1-5> [count=<int>] [interval_ms=<int>]` (defaults: `count=1`, `interval_ms=0`)
- `cmd=cancel`

Mux filter behavior:
- If `mux_start/mux_len/mux_value` is configured, the slot only stores latest frames where that mux field matches. The mux field must lie inside the frame's DLC.
- Changing a slot's mux setting drops its cached frame (and cancels a pending sequence for that slot); the next matching frame becomes the template.
- `cmd=modify slot=<n> mux=0` disables mux filtering for that slot.

Template behavior:
- The template is always the latest matching data frame (RTR frames are ignored). There is no age limit.
- `add` and `modify` are all-or-nothing: if any argument is invalid, the slot is unchanged.
- Replacing a slot with `cmd=add` cancels a pending sequence for that slot.
- An injection is rejected if mask/xor bits fall beyond the template's DLC (they would have no effect).

Name behavior:
- `name` is optional and persisted in the slot state (up to 31 characters; longer names are shortened).
- `cmd=list` includes the slot name.
- `name_clear=1` removes a previously stored name.

Modification math per byte:
- `modified = ((latest & ~mask) | (value & mask)) ^ xor`

Examples:
```text
tool start custom_inject bus=can0
tool config cmd=add slot=1 id=1D5 ext=0 name=throttle
tool config cmd=bit slot=1 bit=10 value=1
tool config cmd=field slot=1 start=20 len=3 value=5
tool config cmd=modify slot=1 mux_start=0 mux_len=4 mux_value=2
tool config cmd=inject slot=1
tool config cmd=inject slot=1 count=5 interval_ms=100
inject slot=1
inject slot=1 count=5 interval_ms=100
tool config cmd=list
```

### 12) `replay`
Records CAN frames for a specific ID, then replays them with original timing.
Intended for short recordings (e.g., capturing a window button press for a few seconds, then replaying it).

Start args:
- `bus=<can0|can1>` (default `can0`)
- `id=<hex>` (required)
- `ext=<0|1>` (default `0`)

Runtime config (`tool config`):
- `cmd=record` starts recording (clears any previous recording)
- `cmd=stop` stops recording or replay
- `cmd=replay [count=<int>] [speed=<10-1000>]` plays back recorded frames
  - `count` number of loops (default `1`; `0` is rejected)
  - `speed` percentage of original speed (default `100`; `200` = double speed, `50` = half speed; outside 10-1000 is rejected)
- `cmd=status` shows current state, frame count, and buffer capacity
- `cmd=clear` clears recorded frames (must be idle)

Buffer: 384 frames max. If the buffer fills during recording, it auto-stops with a warning and preserves captured frames for replay.

Replay timing: each frame's gap is taken from its microsecond capture timestamp (when it was read from the controller), and playback follows the recorded timeline. If the firmware falls behind, the timeline shifts rather than bursting frames to catch up. Send failures are retried (1ms backoff); after ~32 consecutive failures replay aborts to avoid runaway flooding. `bus=both` is not supported for replay (record/replay requires a single target bus).

Examples:
```text
tool start replay bus=can0 id=2C1
tool config cmd=record
# [hold window button for 2 seconds]
tool config cmd=stop
# recording stopped frames=47
tool config cmd=replay
# [replays 47 frames with original timing]
# replay complete

# Replay 3 times at double speed
tool config cmd=replay count=3 speed=200

# Check status
tool config cmd=status

# Clear and re-record
tool config cmd=clear
tool config cmd=record
```

## CLI Response Reference (Injection + Live Decode)

This section lists the exact CLI line formats emitted by firmware for DBC/live decode and injection-related commands.

### Common wrappers
- Tool events are emitted as:
```text
[<tool_name>] <message>
```
- `custom_inject` messages therefore appear as:
```text
[custom_inject] <message>
```

### `dbc` command family
- `dbc clear`
```text
dbc cleared
```

- `dbc add ...` (non-mux)
```text
dbc add sid=<sid> bus=<can0|can1|both> id=0x<ID_HEX>
```

- `dbc add ...` (mux-enabled)
```text
dbc add sid=<sid> bus=<can0|can1|both> id=0x<ID_HEX> mux=[start=<start> len=<len> val=0x<MUX_HEX>]
```

- `dbc remove sid=<sid>`
```text
dbc removed sid=<sid>
```
or (if the SID is not loaded / the argument is invalid):
```text
ERR: NOT_ACTIVE (nothing to act on)
ERR: BAD_ARG (missing or invalid argument)
```

- `dbc add ...` with an impossible descriptor (bit range outside 8 bytes, ID out of range, non-finite factor/offset/min/max) returns `ERR: BAD_ARG ...`; a full store (128 signals) returns `ERR: NO_MEM (storage full)`.

- `dbc list`
```text
dbc count=<N>
sid=<sid> bus=<can0|can1|both> id=0x<ID_HEX> <std|ext> start=<start> len=<len> order=<intel|motorola> sign=<u|s> factor=<f4> offset=<f4>
```
or mux-enabled row:
```text
sid=<sid> bus=<can0|can1|both> id=0x<ID_HEX> <std|ext> start=<start> len=<len> order=<intel|motorola> sign=<u|s> factor=<f4> offset=<f4> mux=[start=<start> len=<len> val=0x<MUX_HEX>]
```

### `dbc_decode` tool
- Start:
```text
tool started: dbc_decode
```

- Status (`tool status` while active):
```text
dbc_decode bus=<can0|can1|both>
```

- Live decoded event lines:
```text
dbc sid=<sid> bus=<can0|can1> id=0x<ID_HEX> raw=<signed_int64> value=<float4> <unit> <ok|oor>
```

### `custom_inject` tool lifecycle
- Start:
```text
tool started: custom_inject
```

- Status (`tool status`):
```text
custom_inject slots=<0-5> pending_slot=none
```
or when pending:
```text
custom_inject slots=<0-5> pending_slot=<1-5> remaining=<n> interval_ms=<ms>
```

- Stop:
```text
tool stopped
```

### `tool config ...` for `custom_inject`
- `tool config cmd=add ...`
```text
[custom_inject] slot=<1-5> tracking bus=<can0|can1|both> id=0x<ID_HEX> <std|ext> mux=off name=<name|->
```
or mux-enabled:
```text
[custom_inject] slot=<1-5> tracking bus=<can0|can1|both> id=0x<ID_HEX> <std|ext> mux=[start=<start> len=<len> val=0x<MUX_HEX>] name=<name|->
```
If the slot had a pending sequence, it is canceled first:
```text
[custom_inject] pending injection canceled (slot replaced)
```

- `tool config cmd=remove slot=<n>`
```text
[custom_inject] slot=<n> removed
```

- `tool config cmd=clear`
```text
[custom_inject] all slots cleared
```

- `tool config cmd=list`
```text
[custom_inject] custom_inject slots_used=<0-5>
```
plus one line per used slot:
```text
[custom_inject] slot=<n> bus=<can0|can1|both> id=0x<ID_HEX> <std|ext> latest=<0|1> mux=off name=<name|->
```
or mux-enabled:
```text
[custom_inject] slot=<n> bus=<can0|can1|both> id=0x<ID_HEX> <std|ext> latest=<0|1> mux=[start=<start> len=<len> val=0x<MUX_HEX>] name=<name|->
```
and, if an injection sequence is active:
```text
[custom_inject] pending slot=<n> remaining=<count> interval_ms=<ms>
```

- `tool config cmd=modify ...`
```text
[custom_inject] slot=<n> modify updated (mask=<0|1> value=<0|1> xor=<0|1> mux=<0|1> sig=<0|1> name=<0|1>)
```
If the mux setting changed while a sequence was pending for the slot:
```text
[custom_inject] pending injection canceled (mux changed)
```

- `tool config cmd=bit ...`
```text
[custom_inject] slot=<n> bit=<0-63> set to <0|1>
```

- `tool config cmd=field ...`
```text
[custom_inject] slot=<n> field set start=<0-63> len=<1-64> value=0x<VALUE_HEX>
```

- `tool config cmd=clearbit ...`
```text
[custom_inject] slot=<n> bit=<0-63> passthrough
```

- `tool config cmd=inject slot=<n> [count=...] [interval_ms=...]`
```text
[custom_inject] inject scheduled slot=<n> count=<count> interval_ms=<ms>
```
then async TX lines during `tick()`:
```text
[custom_inject] inject tx slot=<n> bus=<can0|can1|both> id=0x<ID_HEX> remaining=<remaining_after_tx>
```
and completion:
```text
[custom_inject] inject sequence complete
```
rejected at scheduling time (nothing is scheduled):
```text
[custom_inject] inject rejected: <slot not configured|no matching frame received yet|cached frame no longer matches slot|modified bits beyond frame DLC>
```
possible async errors:
```text
[custom_inject] inject aborted: <reason, as above>
[custom_inject] inject send failed
```

- `tool config cmd=cancel`
```text
[custom_inject] pending injection canceled
```

### `inject` shortcut commands
CLI aliases are parsed to `tool config`:
- `inject slot=<...> [count=...] [interval_ms=...]` -> `cmd=inject ...`
- `inject list` -> `cmd=list`
- `inject cancel` -> `cmd=cancel`

So they emit the same `[custom_inject] ...` lines above.

### Injection/live decode error lines you will see
- If using `inject ...` while `custom_inject` is not active:
```text
ERR: inject requires active custom_inject tool
```

- If `inject ...` / `inject list` / `inject cancel` fails argument/state checks:
```text
ERR: inject command failed (check slot/latest frame/args)
```

- If Flipper currently owns mutating session (except shared `inject/list/cancel` path when `custom_inject` is active):
```text
LOCKED: Flipper owns active session
```

## DBC Descriptor Workflow
Firmware does not parse full DBC files directly. It accepts normalized signal descriptors from CLI/Flipper.
You can load a full DBC file onto the flipper and it will handle all of the parsing and initialization.

Descriptor conventions (same as the DBC file format):
- Bits are numbered `byte * 8 + bit`, bit 0 = LSB of each byte.
- `order=intel` (`@1`): `start` is the least significant bit.
- `order=motorola` (`@0`): `start` is the most significant bit, and the signal runs in the DBC "sawtooth" order. Copy the start bit straight from the `SG_` line.
- The mux field (`mux_start/mux_len`) uses the same byte order as the signal.
- `bus=both` decodes the signal on either bus.
- `min=0 max=0` means "no range" (always `ok`).

Typical flow:
1. `dbc clear`
2. Add one or more descriptors with `dbc add ...`
3. `tool start dbc_decode ...`
4. Monitor decoded events on CLI and Flipper event stream

## Flipper Communication

### Link
- UART: `921600`, `8N1`
- Uses UART1 on the configured Flipper RX/TX pins (`BOARDCFG_FLIPPER_UART_RX_PIN` / `BOARDCFG_FLIPPER_UART_TX_PIN`)

### Packet Framing
Each packet is:
- COBS encoded
- terminated by `0x00`

Decoded packet body layout:
- `ver:u8`
- `kind:u8` (`CMD=0x01`, `RESP=0x02`, `EVENT=0x03`, `ERROR=0x04`)
- `id:u8`
- `seq:u8`
- `len:u16_le`
- `payload[len]`
- `crc16:u16_le` (CRC-CCITT-FALSE over `[ver..payload]`, init `0xFFFF`, poly `0x1021`)

Packets with a bad CRC, bad length or a protocol version other than `1` are discarded (counted as `flipper_rx_errors` in `stats`).

### Command IDs
- `0x01` `PING`
- `0x02` `GET_INFO`
- `0x10` `BUS_SET_CFG`
- `0x11` `BUS_GET_CFG`
- `0x12` `BUS_SET_FILTER`
- `0x13` `BUS_CLEAR_FILTER`
- `0x14` `BUS_SEND_FRAME`
- `0x20` `TOOL_START`
- `0x21` `TOOL_STOP`
- `0x22` `TOOL_STATUS`
- `0x23` `TOOL_CONFIG`
- `0x30` `DBC_CLEAR`
- `0x31` `DBC_ADD_SIGNAL`
- `0x32` `DBC_REMOVE_SIGNAL`
- `0x33` `DBC_LIST`
- `0x40` `STATS_GET`
- `0x50` `WIFI_GET_CFG`
- `0x51` `WIFI_SET_CFG` (ASCII args like `ssid=... pass=... autostart=... ap=...`)
- `0x60` `LED_SET_CFG`

### Event IDs
- `0x80` `CAN_FRAME_EVT`
- `0x81` `TOOL_EVT`
- `0x82` `DBC_DECODE_EVT`
- `0x83` `STATUS_EVT`
- `0x84` `DROPS_EVT`

### Response Status Codes
Responses include status in first payload byte:
- `0` `OK`
- `1` `BAD_CMD`
- `2` `BAD_ARG`
- `3` `BUS_ERR`
- `4` `LOCKED`
- `5` `NO_MEM`
- `6` `CRC_FAIL`
- `7` `NOT_ACTIVE`

### Flipper Command Payload Layouts
All integer fields are little-endian unless noted.

- `PING`, `GET_INFO`, `TOOL_STOP`, `TOOL_STATUS`, `DBC_CLEAR`, `DBC_LIST`, `STATS_GET`
  - payload: empty
  - `GET_INFO` additionally emits a `STATUS_EVT` containing firmware version bytes (`major`, `minor`, version string). See `STATUS_EVT` layouts below.

- `BUS_SET_CFG` (`0x10`)
  - `[0] bus` (`0=can0,1=can1,2=both`)
  - `[1..4] bitrate:u32`
  - `[5] listen_only:u8`

- `BUS_GET_CFG` (`0x11`)
  - `[0] bus`

- `BUS_SET_FILTER` (`0x12`)
  - `[0] bus`
  - `[1..4] mask:u32`
  - `[5..8] filter:u32`
  - `[9] require_ext_match:u8`
  - `[10] ext:u8`

- `BUS_CLEAR_FILTER` (`0x13`)
  - `[0] bus`

- `BUS_SEND_FRAME` (`0x14`)
  - `[0] bus`
  - `[1..4] id:u32`
  - `[5] flags` (`bit0=ext`, `bit1=rtr`)
  - `[6] dlc:u8`
  - `[7..14] data[8]`

- `TOOL_START` (`0x20`)
  - `[0] tool_id` (example: `read_all=1`, `custom_inject=11`, `replay=12`)
  - `[1..] args ASCII string` (same `key=value` format as CLI tool args)

- `TOOL_CONFIG` (`0x23`)
  - `[0..] args ASCII string` (tool-specific)

- `DBC_ADD_SIGNAL` (`0x31`)
  - `[0] bus`
  - `[1..4] id:u32`
  - `[5] ext:u8`
  - `[6] start_bit:u8`
  - `[7] bit_len:u8`
  - `[8] order:u8` (`0=intel,1=motorola`)
  - `[9] sign:u8` (`0=unsigned,1=signed`)
  - `[10..13] factor:f32`
  - `[14..17] offset:f32`
  - `[18..21] min:f32`
  - `[22..25] max:f32`
  - `[26..37] unit[12]`
  - `[38..39] sid:u16`
  - Optional mux extension for backward-compatible multiplex support:
  - `[40] mux_enabled:u8` (`0|1`)
  - `[41] mux_start_bit:u8`
  - `[42] mux_bit_len:u8`
  - `[43..50] mux_value:u64`

- `DBC_REMOVE_SIGNAL` (`0x32`)
  - `[0..1] sid:u16`

- `WIFI_SET_CFG` (`0x51`)
  - `[0..] args ASCII string` (`ssid=... pass=... autostart=... ap=...`)

- `LED_SET_CFG` (`0x60`)
  - `[0] brightness:u8` (`1-10`, each step = 10% brightness)

### Event Payload Layouts
- `CAN_FRAME_EVT` (`0x80`, 19 bytes)
  - `[0..3] ts_ms:u32`
  - `[4] bus`
  - `[5..8] id:u32`
  - `[9] flags` (`bit0=ext, bit1=rtr`)
  - `[10] dlc`
  - `[11..18] data[8]`

- `TOOL_EVT` (`0x81`)
  - `[0] tool_id`
  - `[1] tool_event_code` (`INFO=0, DELTA=1, SUMMARY=2, WARNING=3, ERROR=4`)
  - `[2] text_len`
  - `[3..] text`

- `DBC_DECODE_EVT` (`0x82`, 32 bytes)
  - `[0..1] sid:u16`
  - `[2] bus`
  - `[3..6] frame_id:u32`
  - `[7..14] raw:i64`
  - `[15..18] value:f32`
  - `[19] in_range:u8`
  - `[20..31] unit[12]`

- `STATUS_EVT` (`0x83`)
  - Multiple payload shapes depending on source command:
    - `status` command: 4 bytes
      - `[0] owner`, `[1] active_tool_id`, `[2..3] dbc_count:u16`
    - `bus get cfg`: 16 bytes
      - `[0] bus`, `[1..4] bitrate`, `[5] listen_only`, `[6] filter_enabled`, `[7..10] mask`, `[11..14] filter`, `[15] ext`
    - `dbc list` (Flipper-origin command): 2 bytes
      - `[0..1] dbc_count:u16`
    - `stats`: 44 bytes (11 u32 counters)
    - `get_info` / `ver`: 3 + N bytes (firmware version)
      - `[0] major:u8`, `[1] minor:u8`, `[2] string_len:u8`, `[3..3+string_len]` version string (e.g. `"2.3"`)

- `DROPS_EVT` (`0x84`, 8 bytes)
  - `[0..3] cli_dropped:u32`
  - `[4..7] flipper_dropped:u32`

### Response Behavior
Current implementation sends a response for each Flipper command with:
- same `id` as command
- same `seq`
- payload `[status]` only (no additional response body)

Detailed data is delivered via events. Responses travel in their own queue and are sent ahead of events, so a burst of CAN traffic cannot push out the reply to a command.

`TOOL_START`/`TOOL_CONFIG`/`WIFI_SET_CFG` ASCII arguments may use the full 219-byte payload; longer arguments are rejected (`BAD_ARG`), never truncated.

## Backpressure / Drops
The main loop never blocks on output, so CAN reception and timed transmissions keep running however slow the consumers are.
- Both output queues (CLI, Flipper) are lossy ring buffers: on overflow the oldest entry is dropped, the per-sink drop counter increments, and the firmware emits `DROPS_EVT` and a CLI `drops:` line.
- Each loop writes a bounded amount, and only what the USB / Flipper UART buffers can accept immediately.
- A USB host that stops reading for 250 ms (terminal open but paused) stops receiving lines until it drains; the lines it misses are counted as `usb_stalled`.
- The Wi-Fi client gets non-blocking writes; a line that cannot be sent immediately is skipped for Wi-Fi (`wifi_busy`), and a client that makes no progress for 5 s is disconnected.
- CAN frames lost before reaching the tools are reported separately by `stats` (`rx lost: queue=... can0_missed=... can0_overrun=... can1_overflow=...`).

## Notes
- CAN1 runs in classic mode (DLC up to 8 on tool/event path), not CAN FD payload operation.
- CAN tool/event data path currently uses max DLC 8.
- Bus filters are software-applied after RX in `CanBusManager`.
- `tool start` replaces the currently active tool once the new tool has accepted its arguments; a rejected start leaves the current tool running.
- Transmitting tools (`write`, `custom_inject`, `replay`) keep running when the Flipper's 10 s ownership lease expires or a Wi-Fi client disconnects; stop them with `tool stop`.

## License
The CAN Commander firmware is source-available under the
[PolyForm Noncommercial License 1.0.0](LICENSE.md). Personal, hobby, research,
educational and other noncommercial use, modification and redistribution are
permitted. Commercial use, including selling devices that run this firmware,
requires separate permission from the author.

Third-party components used to build the firmware keep their own licenses; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
