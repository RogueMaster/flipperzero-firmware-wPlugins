# AGENTS.md — MeshCore Config (Flipper Zero FAP)

Context-first brief for anyone (human or agent) picking this repo up.

## What this is

A Flipper Zero application that drives a **MeshCore** node over the Flipper's
**hardware UART**, in two modes:

- **Configurator** — read and change radio parameters, node identity and role,
  apply profiles from the SD card, trigger a self-advert.
- **Messenger** — use the node as a radio: contact list, chats, composing and
  sending messages.
- **Logger** — field logging for link testing: record what nodes hear to CSV on
  the SD card, one file per metric, one directory per session.

**The radio is never on the Flipper.** The node does the meshing, holds the
keys and owns the identity; this app is a client for it. Nothing cryptographic
is duplicated on the Flipper — it reads and displays what the node reports.

Brand: **Greyrock Labs**. License: MIT.

Target nodes: **Heltec T114** (nRF52840) and **Heltec V4** (ESP32-S3). Both
modes need the same thing of the node: a *companion* firmware build with the
serial interface on a hardware UART. See "Node firmware requirements" below —
in particular the T114 caveat, which applies to the messenger exactly as it
does to the configurator.

`TASKS.md` tracks what is done, what is next and what is waiting on hardware.

## Stack

| Layer | What |
| --- | --- |
| Build | `ufbt` (Flipper micro build tool), official firmware SDK |
| Language | C (C99 for the vendored protocol code, Flipper's C dialect elsewhere) |
| UI | `SceneManager` + `ViewDispatcher`, `Submenu` and `VariableItemList` modules |
| Transport | `furi_hal_serial` on `FuriHalSerialIdUsart`, 115200 8N1 |
| Protocol | [SH3D/meshcore_c](https://github.com/SH3D/meshcore_c) — single-file C99 companion-protocol client, vendored (MIT) |

## Build and run

```sh
ufbt              # build the .fap into dist/
ufbt launch       # build, upload to a USB-connected Flipper and start the app
ufbt cli          # serial CLI to the Flipper (log output)
ufbt update       # pull/refresh the firmware SDK
```

`ufbt` comes from PyPI: `pip install ufbt`.

### Target firmware: Unleashed

**This app is built against the Unleashed SDK, not the official one.** Point
ufbt at the Unleashed index once per machine, before the first build:

```sh
ufbt update --index-url=https://up.unleashedflip.com/directory.json --channel=release
```

To go back to official firmware:

```sh
ufbt update --index-url=https://update.flipperzero.one/firmware/directory.json --channel=release
```

This matters because a FAP records the API version it was compiled against and
the firmware refuses to load one that asks for more than it provides. Official
1.4.3 is API **87.1**; Unleashed `unlshd-089` is API **87.8**. Same major, so a
FAP built against *official* still loads on Unleashed — but one built against
Unleashed will **not** load on official firmware. Nothing catches this at build
time; it fails on the device.

The choice lives in `~/.ufbt/current/ufbt_state.json`, which a repository
cannot pin — so check it whenever a build behaves unexpectedly:

```sh
cat ~/.ufbt/current/ufbt_state.json
```

The code itself uses no Unleashed-specific API, so it compiles against either.

**Do not call a step done until `ufbt` builds clean.**

### Host tests

The protocol layer also builds and runs on a PC:

```sh
pip install ziglang     # one-off; no admin rights needed
pwsh test/run.ps1
```

These cover framing, frame assembly, command layout, response parsing and the
request/reply logic in `meshcore_link.c` — everything that does not need radio.
Run them after any change under `protocol/`; they take about a second and they
are the only way to catch a wire-format mistake without a node on the bench.
See `test/README.md`.

## Layout

```
application.fam            app manifest (appid, entry point, icon, category)
meshcore_cfg.h/.c          MeshCoreApp state, ViewDispatcher/SceneManager wiring, entry point
meshcore_log.h/.c          mutex-guarded in-memory traffic log shown by scene_log
meshcore_10px.png          10x10 app icon
uart/
  meshcore_uart.h/.c       furi_hal_serial wrapper; bytes only, no protocol
protocol/
  meshcore_c/              vendored upstream library — do not patch, see VENDOR.md
  meshcore_link.h/.c       binds meshcore_c to the UART layer; blocking request/poll
  meshcore_route.h/.c      pure policy: reply vs stream vs unsolicited event
  meshcore_session.h/.c    long-lived worker owning the link (see below)
messenger/
  meshcore_contacts.h/.c   contact list mirrored from the node, ages, collector
  meshcore_messages.h/.c   RAM ring of recent messages, keyed by peer prefix
  meshcore_mailbox.h/.c    worker that drains incoming mail from the node
logger/
  meshcore_rxlog.h/.c      decodes the 0x88 RX log push; hex and SNR rendering
  meshcore_csv.h/.c        CSV on SD, synced line by line
  meshcore_logger.h/.c     LogNode, session dir, writer thread
scenes/
  meshcore_scene_config.h  X-macro list — the single place scenes are registered
  meshcore_scene.h/.c      generated scene enum + handler tables
  meshcore_scene_menu.c    main menu
  meshcore_scene_connect.c handshake on a worker thread, shows model/fw/radio
  meshcore_scene_contacts.c messenger entry point: the node's peers + last seen
  meshcore_scene_chat.c    one conversation, rebuilt when the store changes
  meshcore_scene_log.c     passthrough hex log, refreshed on the scene tick
test/
  run.ps1                  compile + run the host tests
  test_meshcore.c          framing, command layout, parsers, link logic
  fakes.c / fakes.h        fake UART, fake log, fake clock
  shims/furi.h             minimal host stand-in for the Flipper <furi.h>
```

Planned (not yet present):

```
profiles/                  JSON profile loading from the SD card
```

Adding a scene = one line in `scenes/meshcore_scene_config.h` +
`scenes/meshcore_scene_<name>.c` with the three handlers.

`application.fam` lists `sources` per directory rather than using the fbt
default `*.c*`. That default is globbed **recursively from the app root**, which
would compile `test/` into the firmware — host fakes and all, colliding with
the real UART layer at link time. **A new source directory needs a line in
`sources`,** or its files silently will not build.

## Wiring

Two non-overlapping four-pin blocks, one per UART:

```
block A (USART)    11 GND | 12 DETECT | 13 TX | 14 RX
block B (LPUART)   15 TX  | 16 RX     | 17 DETECT | 18 GND
```

The node connector is identical for both — **GND · DETECT · RX · TX** — so the
cables differ only in the order they land on the Flipper: cable A straight
(11·12·13·14), cable B rearranged (18·17·15·16).

| Signal | T114, header P1 | V4, header J2 |
| --- | --- | --- |
| GND | P1·4 | J2·1 |
| DETECT | P1·8 = GPIO33 | J2·12 = GPIO33 |
| RX | P1·12 = GPIO9 | J2·13 = GPIO47 |
| TX | P1·13 = GPIO10 | J2·14 = GPIO48 |

**DETECT** (`detect/meshcore_detect.c`) is driven high for the app's lifetime
and released on exit. A node samples it *at boot*: high means stay on serial,
low means bring up its own radio. Hence "plug in and reboot" / "unplug and
reboot" — there is no hot switching.

Pin 12 is PA13, also SWDIO, and the only free pin in block A. With the Flipper's
Debug mode enabled the debug peripheral owns it; the app reads the pin back and
warns on the Connect screen rather than assuming.

115200 8N1. Flipper GPIO is 3.3 V, same as both target nodes — no level
shifter needed. The Flipper has **no USB host**, so the node must expose the
companion protocol on a *hardware* UART, not on USB-CDC. See below.

Pins 13/14 are USART1 (`FuriHalSerialIdUsart`); pins 15/16 are LPUART. We use
USART.

**Gotcha for the UART layer:** the same USART backs the Flipper's expansion
module service. Before `furi_hal_serial_control_acquire(FuriHalSerialIdUsart)`
the app must disable it, and re-enable it on exit:

```c
Expansion* expansion = furi_record_open(RECORD_EXPANSION);
expansion_disable(expansion);
/* ... acquire / init / use / deinit / release the serial handle ... */
expansion_enable(expansion);
furi_record_close(RECORD_EXPANSION);
```

## Companion serial protocol

Wire format, verified against the MeshCore firmware
(`src/helpers/ArduinoSerialInterface.cpp`):

```
frame = [type:u8][len:u16 little-endian][payload:len bytes]

type 0x3C = '<'   app -> radio    (what this FAP sends)
type 0x3E = '>'   radio -> app    (what this FAP receives)

payload[0] = command code (outbound) / response or push code (inbound)
```

> **Note:** the direction of the two marker bytes is easy to get backwards.
> The firmware's `writeFrame()` emits `'>'` and its `checkRecvFrame()` waits
> for `'<'` — from the *node's* point of view. So the **FAP sends `'<'` and
> receives `'>'`**. `meshcore_c` already encodes this correctly
> (`MC_FRAME_APP_TO_RADIO == 0x3C`). Do not "fix" it.

`meshcore_c` does no I/O and no dynamic allocation — it is a frame assembler
(`mc_rx_feed` / `mc_rx_poll`), a set of payload builders (`mc_cmd_*`) that
write into a caller-owned buffer, `mc_frame_encode` to wrap a payload, and
`mc_parse` to decode one received payload into an `mc_event_t`. That maps
cleanly onto `furi_hal_serial_async_rx` + `furi_hal_serial_tx`.

Commands this app needs:

| Purpose | Builder | Reply |
| --- | --- | --- |
| Handshake / read current config | `mc_cmd_app_start` | `MC_RESP_SELF_INFO` → `mc_self_info_t` (name, `radio_freq/bw/sf/cr`, `tx_power`, `type`) |
| Model + firmware version | `mc_cmd_device_query` | `MC_RESP_DEVICE_INFO` → `mc_device_info_t` (`model`, `ver`, `fw_ver`) |
| Radio params | `mc_cmd_set_radio_params(freq_khz, bw, sf, cr)` | `MC_RESP_OK` / `MC_RESP_ERR` |
| TX power | `mc_cmd_set_tx_power(dbm)` | `MC_RESP_OK` / `MC_RESP_ERR` |
| Node name | `mc_cmd_set_advert_name(name)` | `MC_RESP_OK` / `MC_RESP_ERR` |
| Self advert | `mc_cmd_send_self_advert(MC_ADVERT_ZERO_HOP \| MC_ADVERT_FLOOD)` | `MC_RESP_OK` / `MC_RESP_ERR` |

Note `mc_cmd_set_radio_params` takes frequency in **kHz** (the header calls the
argument `freq_hz_x1000`).

Commands the messenger needs:

| Purpose | Builder | Reply |
| --- | --- | --- |
| Contact list | `mc_cmd_get_contacts(since_lastmod)` | `CONTACTS_START` (count) → `CONTACT` × N → `END_OF_CONTACTS` |
| Node clock | `mc_cmd_get_device_time` | `MC_RESP_CURR_TIME` |
| Drain a message | `mc_cmd_sync_next_message` | `CONTACT_MSG_RECV(_V3)` / `CHANNEL_MSG_RECV(_V3)`, or `NO_MORE_MESSAGES` |
| Send to a contact | `mc_cmd_send_txt_msg(...)` | `MC_RESP_SENT`, later `MC_PUSH_SEND_CONFIRMED` |
| Send to a channel | `mc_cmd_send_channel_text(...)` | `MC_RESP_SENT` |
| Channel config | `mc_cmd_get_channel(idx)` | `MC_RESP_CHANNEL_INFO` |

Three things about the messenger side are easy to assume wrongly:

1. **There is no subscription for incoming messages.** The node sends the
   `MC_PUSH_MSG_WAITING` (0x83) push — literally "drain me" — and the client
   then loops `SYNC_NEXT_MESSAGE` until it gets `NO_MORE_MESSAGES`. Handling
   incoming mail is a pull loop triggered by a push, not a callback.
2. **Delivery is two-step.** `SEND_TXT_MSG` is answered with `MC_RESP_SENT`,
   carrying `expected_ack` and `suggested_timeout`; actual delivery arrives
   later as `MC_PUSH_SEND_CONFIRMED` with a matching ack tag. "Sent" and
   "delivered" are different states and the chat UI should show both.
3. **Timestamps come from the node's clock.** `last_advert` and message
   timestamps are in the node's timebase, which drifts from the Flipper RTC.
   Read `MC_RESP_CURR_TIME` and compute ages against that — `scene_contacts`
   already does.

**Open question — role switching.** The companion protocol has no
"set role" command: in MeshCore, companion / repeater / room server are
separate firmware builds (see the `*_repeater`, `*_room_server`,
`*_companion_radio_*` PlatformIO envs). `mc_device_info_t.repeat` exists on
fw_ver ≥ 9, and `mc_cmd_send_cmd` can push a CLI string to a node. Before
building `scene_role`, confirm on hardware what a node actually accepts.

## Logger mode

Field logging for link testing: walk around with a node on the Flipper and end
up with CSV you can plot. The schemas match `meshlog.py` so existing pipelines
keep working.

### Both ports are universal

The Flipper has two hardware UARTs and either can take any node:

| Port | TX | RX |
| --- | --- | --- |
| `FuriHalSerialIdUsart` | pin 13 | pin 14 |
| `FuriHalSerialIdLpuart` | pin 15 | pin 16 |

So role and hardware are properties of the **node**, discovered per node, not
of the port. That is what `MeshCoreLogNode` exists for.

### Detection: two independent axes

1. **Interface — this decides the wire protocol, so it is the important one.**
   - *companion* → the binary companion protocol (`<`/`>`, meshcore_c). Full
     test set: RX log, telemetry, contacts, ping, send/receive.
   - *repeater* → does **not** speak the companion protocol at all. It has a
     text CLI, so it gets a different test set: poll stats over CLI (uptime,
     packets, battery, noise floor). Checking that a repeater actually relays
     is done with Trace Path *from a companion*, not from the repeater itself,
     and is out of scope for this mode.
2. **Hardware** — T114 (nRF52840) or V4 (ESP32-S3), from `SELF_INFO` /
   `DEVICE_INFO` on a companion, or `infos` on a repeater's CLI.

Detection on attach: send a companion self-info request. A framed reply means
companion; text, a CLI prompt, or a timeout on the framed reply means try the
CLI. Then read the model. *(Stage 4; stage 1 assumes companion on the USART.)*

### Where the data comes from

Companion, through meshcore_c:

| Metric | Source |
| --- | --- |
| SNR / RSSI / raw packet | `RX_LOG_DATA` push (0x88); `RAW_DATA` 0x84 on some builds |
| adverts, messages | `ADVERT`, `CONTACT_MSG_RECV`, `CHANNEL_MSG_RECV` |
| telemetry | `GET_STATS` with `MC_STATS_CORE` / `RADIO` / `PACKETS` |
| ping | `GET_CONTACTS` → `SEND_TXT_MSG` → wait for the ACK; RTT is the time to it |

**meshcore_c declares `RX_LOG_DATA`, `RAW_DATA`, `TRACE_DATA` and `TELEMETRY`
but does not decode them** — `mc_parse` returns 0. That is why
`meshcore_link_poll` reports undecoded frames instead of skipping them, and why
`logger/meshcore_rxlog.c` exists. Do not "fix" this by patching the vendored
library; decode in our layer.

The 0x88 layout is taken from the firmware (`MyMesh::logRxRaw`), and
`Dispatcher::checkRecv()` calls it for **every** raw frame off the radio with
no build flag and no runtime switch — so a stock companion build emits it for
everything it hears, including packets it cannot parse.

Repeater: a small text CLI handler, stage 5. meshcore_c is not applicable.
Command names drift between versions — type `help` / `set help` on the node.

### CSV schemas

One directory per session: `/ext/apps_data/meshcore_cfg/logs/<session>/`.
The `node,role,hw` tags go at the **end** of every row so the original
`meshlog.py` column order is untouched.

```
rx_log:    ts,snr,rssi,lat,lon,acc,raw,node,role,hw
telemetry: ts,batt_pct,voltage,noise_floor,rx_total,tx_total,recv_errors,lat,lon,acc,raw_bat,raw_radio,raw_pkts,node,role,hw
ping:      ts,target,seq,ok,rtt_ms,lat,lon,acc,node,role,hw
events:    ts,type,info,lat,lon,acc,raw,node,role,hw
```

Every row is synced to the card as it is written — pulling the power mid-walk
loses at most the row in flight.

### Rules that are easy to get wrong

- **Position comes from the node, never the Flipper.** The Flipper has no GPS.
  A node with no advertised position logs empty `lat`/`lon`/`acc`, as
  `meshlog.py` does. A node advertising exactly 0,0 is treated as *no*
  position — otherwise every walk lands in the Gulf of Guinea.
- **Do not power nodes from the Flipper.** GND and data only; the node runs off
  its own supply.
- **CSV rows are built on the session worker but written by a separate writer
  thread.** A synced SD write can take tens of milliseconds, and the producing
  thread is the one draining the UART — writing in place would overrun the
  receive buffer. The queue is deliberately shallow and drops rather than
  blocks; drops are counted and shown.

## Settings layers — which parameters can the Flipper actually change?

Traced through the MeshCore sources rather than assumed, because getting this
wrong means either a dead editor or a setting that silently does nothing.

| Parameter | Layer | How |
| --- | --- | --- |
| frequency, bandwidth, SF, CR | **runtime** | `CMD_SET_RADIO_PARAMS` (11) |
| TX power | **runtime** | `CMD_SET_RADIO_TX_POWER` (12) |
| node name | **runtime** | `CMD_SET_ADVERT_NAME` (8) |
| path hash bytes | **runtime** | `CMD_SET_PATH_HASH_MODE` (61) — *a separate command, not part of set-radio* |

**All four are editable on the Flipper.** Nothing in the preset needs to be a
read-only label.

### Path hash: the suspicion was wrong, and the name is overloaded

Three different things in MeshCore are called "path hash":

1. `#define PATH_HASH_SIZE 1` in `src/MeshCore.h` — compile-time, and **not
   overridable from any variant's build flags** (checked across `variants/`
   and `platformio.ini`). It sizes the hash field inside `Identity` and
   `Channel` structs. Not a network profile knob, and not what a preset means.
2. The **per-packet** hash width, carried on the wire: on receive the firmware
   derives it as `path_hash_size = (path_len >> 6) + 1`, so the top two bits of
   the path-length byte encode it. Valid range 1–3 (`Mesh::sendFlood` rejects
   0 and >3).
3. `path_hash_mode` in `NodePrefs` — **runtime**, which width this node uses
   when *sending*.

The mapping, from `MyMesh.cpp`: `sendFlood(pkt, delay, _prefs.path_hash_mode + 1)`.

```
path_hash_bytes = path_hash_mode + 1     mode 0 -> 1 byte
                                         mode 1 -> 2 bytes
                                         mode 2 -> 3 bytes
```

Setting it: payload `[61][0][mode]`, with `mode < 3` enforced by the firmware
(`ERR_CODE_ILLEGAL_ARG` otherwise). Note the mandatory zero second byte. The
value is persisted with `savePrefs()` and reported back in `DEVICE_INFO` when
`fw_ver >= 10`, which is how Apply can verify it took.

So a preset carrying `path_hash_bytes: 2` is applied by sending
`[61][0][1]` — separately from set-radio, exactly as the spec expected, but
because it is a *different command*, not because it is a different layer.

## Node firmware requirements

The node must run a **companion** build whose `ArduinoSerialInterface` is bound
to a hardware serial port.

**Heltec V4 (ESP32-S3)** — stock `heltec_v4_companion_radio_usb` calls
`serial_interface.begin(Serial)`. With no `ARDUINO_USB_CDC_ON_BOOT` override,
`Serial` is UART0 (GPIO43 TX / GPIO44 RX), the port behind the on-board
USB-serial bridge. Expected to work as shipped; confirm on hardware.

**Heltec T114 (nRF52840)** — **does not work as shipped.** On the nRF52 branch
of `examples/companion_radio/main.cpp` there is no `SERIAL_RX` case: it is
either BLE (`BLE_PIN_CODE`) or `serial_interface.begin(Serial)`, and on the
Adafruit nRF52 core `Serial` is **USB-CDC**. The Flipper cannot act as a USB
host, so the node must be rebuilt with the companion interface bound to
`Serial1` on the T114's exposed UART pins. Building that firmware is **out of
scope for this FAP** — it is a documented prerequisite.

## Conventions

- Prefix everything `meshcore_` / `MeshCore` to keep the global namespace clean.
- Classic Flipper look: inverted selected row (that is what `Submenu` and
  `VariableItemList` already do — don't hand-roll widgets).
- `VariableItemList` for value pickers; it renders the `‹ ›` arrows for free.
- Comments explain *why*, not *what*.

### Threading

Three contexts, and mixing them is the main way to break this app:

- **ISR** — `meshcore_uart_rx_isr` only. Drains the peripheral into a stream
  buffer and returns. Never allocate, never block, never touch a view.
- **Session worker** — one thread, `protocol/meshcore_session.c`, alive for as
  long as the app is connected. It owns the link and is the only thing that
  reads from it. Every frame is routed by `meshcore_route_event()` into either
  a reply for whoever is waiting, a frame for a streaming collector, or an
  unsolicited event handed to the app. The event callback and stream collector
  **run on this thread** — they must not block and must not touch a view.
- **Mailbox worker** — `messenger/meshcore_mailbox.c`, also alive for the app's
  lifetime. Sleeps on an event flag, woken by a `MSG_WAITING` push or by a 5 s
  timer, then drains with `SYNC_NEXT_MESSAGE`. It has to be a separate thread:
  the push lands on the session worker, which must not block, and draining
  blocks on that same worker's replies. Waking it only sets a flag, so there is
  no deadlock.
- **Scene worker** — short-lived, one per scene that talks to the node.
  `meshcore_session_request()` and friends block, so they belong here. Report
  back with `view_dispatcher_send_custom_event()`, which is safe across
  threads. See `scene_connect` and `scene_contacts` for the shape:
  `furi_thread_alloc_ex` in `on_enter`, join and free in `on_exit`.
- **GUI thread** — scene handlers. Never call into `protocol/` from here; a
  silent node would freeze the UI for the request timeout.

Why a long-lived session rather than a worker per request: the node pushes
`MSG_WAITING` whenever a message arrives, at a moment of its choosing. With a
worker that exists only for the duration of a request, nobody is listening in
between and the push is lost. The configurator never noticed; the messenger
would not work at all.

`TextBox` stores the `const char*` it is given rather than copying it, and the
GUI service redraws from that pointer on its own thread. So the string behind a
live `TextBox` must never be mutated — `scene_log` keeps two `FuriString`
buffers in `MeshCoreApp` and swaps between them. `Widget` elements do copy, so
a stack buffer is fine there.

## Working order

Incremental; each step must build under `ufbt` before the next begins.

1. ufbt skeleton: manifest, entry point, SceneManager, static `scene_menu`. ← **done**
2. UART layer + `scene_connect`: open the port, read self-info, show model and
   firmware version, plus a passthrough log for debugging. ← **done**
3. Vendor `meshcore_c`, wire the first real command (read config). ← **done**
   (folded into step 2: reading self-info *is* the config read, and the task
   forbids hand-rolling a frame parser, so the library had to land first.
   `MeshCoreNodeInfo` in `meshcore_cfg.h` is the config the editors will edit.)
4. `scene_radio` / `scene_identity` / `scene_role` editors on `VariableItemList`.
5. `scene_profiles` (JSON from `/ext/apps_data/meshcore_cfg/profiles/`) and
   `scene_apply` (send commands, show a result checklist).
