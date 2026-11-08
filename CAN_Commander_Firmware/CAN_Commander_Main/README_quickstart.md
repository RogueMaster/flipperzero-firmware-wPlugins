# CAN Commander v2 Quickstart

This is the fast path to get CAN Commander v2 running over USB CLI, optional Wi-Fi AP CLI, and Flipper UART.

For full details, see `README.md`.

## 1) Prerequisites
- ESP32-S3 (official board) or classic ESP32 with a 3.3 V CAN transceiver
- `arduino-cli` with the esp32 core 3.3.x
- Libraries: ACAN2517FD, autowp-mcp2515, Adafruit NeoPixel (see `README.md` for tested versions)

## 2) Build

```bash
# Official board (ESP32-S3): USB CDC On Boot must be enabled
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc .

# Classic ESP32 DevKit (CAN0 on GPIO5/4, Flipper UART on GPIO17/16)
arduino-cli compile --fqbn esp32:esp32:esp32 .
```

Pins and hardware options are `BOARDCFG_*` macros in `src/BoardConfig.h`. Change them there or pass them through `compiler.cpp.extra_flags` (never `build.extra_flags`, which drops the board's USB flags):

```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc \
  --build-property "compiler.cpp.extra_flags=-DBOARDCFG_FLIPPER_UART_TX_PIN=17 -DBOARDCFG_FLIPPER_UART_RX_PIN=18" .
```

## 3) Flash

```bash
arduino-cli upload \
  --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc \
  -p <YOUR_PORT> \
  .
```

## 4) Open USB CLI
- Baud: `115200`
- On boot, you should see:
  - `CAN Commander v2.3 booting...`
  - `CAN0 initialized; CAN1 backend=<...> state=<ready|disabled>`
  - if a configured second controller is missing: `WARN: CAN1 (<backend>) not available: ...` (CAN0 still works)
  - `Type 'help' for commands.`

## 4.1) Hardware selection (`src/BoardConfig.h`)

```text
BOARDCFG_CAN1_DRIVER        0 = none, 1 = MCP2517FD/MCP2518FD, 2 = MCP2515
BOARDCFG_CAN1_CRYSTAL_MHZ   MCP2517FD/2518FD: 4, 20, 40   MCP2515: 8, 16, 20
BOARDCFG_NEOPIXEL_ENABLED   0 | 1
BOARDCFG_CAN0_TX_PIN / BOARDCFG_CAN0_RX_PIN
BOARDCFG_SPI_CS_PIN / _MOSI_PIN / _SCK_PIN / _MISO_PIN
BOARDCFG_FLIPPER_UART_TX_PIN / BOARDCFG_FLIPPER_UART_RX_PIN
BOARDCFG_WIFI_AP_ENABLED    0 | 1 (default 1: compiled in, not started)
BOARDCFG_WIFI_AP_AUTOSTART  0 | 1 (default 0)
```

Common setups:
```text
CAN0 only:                  BOARDCFG_CAN1_DRIVER=0
CAN0 + MCP2515 @ 8 MHz:     BOARDCFG_CAN1_DRIVER=2 BOARDCFG_CAN1_CRYSTAL_MHZ=8
CAN0 + MCP2518FD @ 40 MHz:  BOARDCFG_CAN1_DRIVER=1 BOARDCFG_CAN1_CRYSTAL_MHZ=40
```

The build stops with an explanation if pins collide or if the Flipper UART would share UART0 with the USB CLI.

Behavior with CAN1 disabled or missing:
- `bus=can1` commands and tools return an error.
- `bus=both` uses available buses (CAN0 only in this mode).

## 5) First CLI Session (copy/paste)

```text
help
status
bus show
bus set bus=can0 bitrate=500000 mode=normal
# run next line only when CAN1 backend is enabled
bus set bus=can1 bitrate=500000 mode=normal
tool start read_all bus=both ascii=0
```

Stop active tool:

```text
tool stop
```

## 6) Most Useful Tool Commands

Read all traffic:
```text
tool start read_all bus=both ascii=0
```

Filtered read:
```text
tool start filtered bus=can0 mask=7FF filter=273 ext_match=0 ext=0 ascii=0
```

Send one frame:
```text
send bus=can0 id=1D5 ext=0 rtr=0 dlc=8 data=102030AABBCCDDEE
```

Periodic TX tool:
```text
tool start write bus=can0 id=1D5 ext=0 rtr=0 dlc=8 data=102030AABBCCDDEE period_ms=100 count=50
```

Speed test:
```text
tool start speed bus=can0
```

Unique IDs:
```text
tool start unique_ids bus=both
tool config snapshot=1
```

Bit tracker:
```text
tool start bittrack bus=can0 id=273
```

Value tracker:
```text
tool start valtrack bus=can0 strict=1
tool config show=both
```

Reverse tool:
```text
tool start reverse bus=can0 calibration_ms=10000 monitor_ms=20000
tool config exclude_pre=1
```

OBD PID tool (RPM):
```text
tool start obd_pid bus=can0 pid=0C interval_ms=200
tool start obd_pid bus=can0 pid=rpm interval_ms=200
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

Custom inject tool (track latest + inject modified):
```text
tool start custom_inject bus=can0
tool config cmd=add slot=1 id=1D5 ext=0 name=throttle mux_start=0 mux_len=4 mux_value=2
tool config cmd=field slot=1 start=10 len=3 value=5
tool config cmd=modify slot=1 name=throttle_override
tool config cmd=inject slot=1 count=5 interval_ms=100
tool config cmd=list
```

Wi-Fi AP injection trigger shortcut (after `custom_inject` is already loaded/running):
```text
inject slot=1
inject slot=1 count=5 interval_ms=100
inject list
inject cancel
```

## 6.1) Optional Wi-Fi AP CLI
The AP is compiled in but off by default. The default password `cancommander` is public, so set your own first:

```text
wifi set pass=<your-8-to-63-char-password>
wifi start
```

1. On start, note:
```text
WiFi AP ready ssid=<ssid> ip=<ap_ip> cli_port=<port>
```
2. Join the AP from your phone/laptop.
3. Open a TCP client to `<ap_ip>:<port>` (for example: `nc <ap_ip> 7777`).
4. Use the same CLI commands as USB.

To start the AP on every boot: `wifi set autostart=1`.

Runtime Wi-Fi commands:
```text
wifi show
wifi set ssid=MyAP pass=StrongPass123 autostart=1 ap=1
wifi stop
wifi start
```

## 7) Quick DBC Decode Flow

1. Clear existing descriptors:
```text
dbc clear
```

2. Add a signal descriptor:
```text
dbc add bus=can0 id=1D5 ext=0 start=0 len=16 order=intel sign=u factor=0.1 offset=0 min=0 max=6553.5 unit=kph sid=1
# optional mux-gated descriptor (decode only when mux field matches)
dbc add bus=can0 id=3C2 ext=0 start=8 len=8 order=intel sign=u factor=1 offset=0 min=0 max=255 unit=raw sid=2 mux_start=0 mux_len=4 mux_value=1
```

3. Start decoder:
```text
tool start dbc_decode bus=can0
```

4. Check loaded descriptors:
```text
dbc list
```

## 8) Flipper UART Quick Bring-Up

### Link Settings
- UART: `921600`, `8N1`
- Protocol: COBS-framed packets, `0x00` terminator
- Packet body before COBS:
  - `ver:u8`
  - `kind:u8`
  - `id:u8`
  - `seq:u8`
  - `len:u16_le`
  - `payload[len]`
  - `crc16_ccitt_false:u16_le` over `[ver..payload]`

### Minimal Bring-Up Sequence
1. Send `PING` command (`kind=CMD`, `id=0x01`, empty payload).
2. Confirm response with:
   - `kind=RESP`
   - same `id` and `seq`
   - `payload[0] == 0` (`OK`)
3. Send `GET_INFO` (`id=0x02`) and read status response.
4. Send `BUS_GET_CFG` (`id=0x11`) payload `[bus]` (`0=can0`, `1=can1`), then read `STATUS_EVT`.
5. Start a tool with `TOOL_START` (`id=0x20`) payload:
   - byte0 = `tool_id` (for `read_all`, use `1`; for `custom_inject`, use `11`)
   - bytes1..N = ASCII args string, e.g. `bus=both ascii=0`

### Most Important IDs
Commands:
- `0x01 PING`
- `0x02 GET_INFO`
- `0x10 BUS_SET_CFG`
- `0x11 BUS_GET_CFG`
- `0x14 BUS_SEND_FRAME`
- `0x20 TOOL_START`
- `0x21 TOOL_STOP`
- `0x23 TOOL_CONFIG`
- `0x31 DBC_ADD_SIGNAL`
- `0x50 WIFI_GET_CFG`
- `0x51 WIFI_SET_CFG` (ASCII args: `ssid=... pass=... autostart=... ap=...`)
- `0x40 STATS_GET`

Events:
- `0x80 CAN_FRAME_EVT`
- `0x81 TOOL_EVT`
- `0x82 DBC_DECODE_EVT`
- `0x83 STATUS_EVT`
- `0x84 DROPS_EVT`

Status codes (`RESP payload[0]`):
- `0 OK`
- `1 BAD_CMD`
- `2 BAD_ARG`
- `3 BUS_ERR`
- `4 LOCKED`
- `5 NO_MEM`
- `6 CRC_FAIL`
- `7 NOT_ACTIVE`

## 9) Ownership Rules**
- One active tool globally.
- Flipper can preempt an active CLI tool.
- If Flipper owns mutation lock, CLI mutating commands are blocked.
- Exception: if `custom_inject` is active, CLI `inject ...`, `inject list`, and `inject cancel`
  are still allowed while Flipper owns the session.
- Flipper owner timeout is `10s` without valid packets.

## 10) Useful Health Checks

```text
status
bus show
tool status
stats
led show
```
