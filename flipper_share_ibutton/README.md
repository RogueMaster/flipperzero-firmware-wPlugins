# Flipper Share iButton — implementation specification

Status: **specification, not implemented**. This document is a complete, self-contained
work order for implementing `flipper_share_ibutton` — direct file transfer between two
Flipper Zeros over the 1-Wire iButton pad ("touch the pads together"), using the Flipper
Share v2 protocol. All design decisions below are final; implement as written unless
something is physically impossible, and record any forced deviation in this README.

The app must build with `ufbt` against the official firmware SDK and run on unmodified
official firmware. Every firmware API named below is exported to FAPs (verified against
`targets/f7/api_symbols.csv`, API 87.x); re-verify against the current SDK before coding.

## 1. Goal and scope

- Transfer a file between two Flipper Zeros over a single-wire 1-Wire link: iButton pad
  to iButton pad (the pad carries both data and GND contacts), or one jumper wire
  (pin 17 ↔ pin 17, the same PB14 net as the pad) plus GND.
- Roles: **receiver = 1-Wire host** (drives the bus and all timing), **sender = 1-Wire
  slave** (emulator, answers in read slots) — consistent with the other transports where
  the sender is the passive side (NFC listener, RFID tag).
- Standard-speed slots only (~13.7 kbit/s raw); expected effective throughput
  ~1.2 KB/s — an 8 KB file in ~7 s.

Out of scope (do not implement): overdrive mode (the slave is a software bit-banger in
critical sections; overdrive between two Flippers is timing-marginal), 1-Wire ROM
search/addressing (point-to-point link, exactly two devices), CRC8 per frame (the
protocol's packet CRC16 is sufficient).

## 2. Firmware API

Headers `lib/one_wire/one_wire_host.h` and `lib/one_wire/one_wire_slave.h`, all exported:

- Host: `onewire_host_alloc(const GpioPin*)`, `_free`, `_start`, `_stop`, `_reset`
  (returns presence, bool), `_read`, `_read_bytes`, `_write`, `_write_bytes`.
- Slave: `onewire_slave_alloc(const GpioPin*)`, `_free`, `_start`, `_stop`,
  `_send(bus, data, size)` (bool), `_receive(bus, data, size)` (bool),
  `_set_reset_callback(bus, cb, ctx)`, `_set_command_callback(bus, cb, ctx)`.
  Callback types: `bool OneWireSlaveResetCallback(bool is_short, void* ctx)`,
  `bool OneWireSlaveCommandCallback(uint8_t command, void* ctx)`.

Both `alloc()` take an arbitrary `const GpioPin*`. Default pin: `&gpio_ibutton`
(PB14 — the iButton pad, also header pin 17). The pin is a config constant
(`IBTN_TP_GPIO`); if pad-to-pad presence detection turns out unreliable on the bench
(pull-up topology), fall back to `&gpio_ext_pa7` + explicit wire — same code path.

Reference for slave-side usage: firmware `lib/ibutton/protocols/dallas/*` (emulation via
these exact callbacks). Note the slave command callback and everything called from it
(`onewire_slave_send/receive`) runs in **interrupt/critical-section context** — no
blocking furi calls, no mutexes, no storage I/O there.

## 3. Architecture and naming (shared convention for all new transports)

New Flipper Share apps (`flipper_share_uart`, `flipper_share_ibutton`,
`flipper_share_rfid`) do NOT continue the per-app prefix pattern of the three legacy apps
(`fs_`/`ish_`/`nsh_`). Instead they share one neutral prefix, so the engine files stay
**byte-identical** across the new apps and fixes port by file copy. The three legacy apps
must not be modified.

Rules:

1. Engine files `share.c`, `share.h`, `md5_hash.c`, `md5_hash.h` are derived from
   `../flipper_share_nfc/nfc_share.c`, `nfc_share.h`, `md5_hash.c`, `md5_hash.h` by a
   mechanical rename: `nsh_` → `fsh_`, `NSH_` → `FSH_`, `NfcShare` → `Share`,
   `nfc_share` → `share`. Engine log TAG becomes `"FShare"`.
2. All transport-tunable constants are MOVED out of the engine into a new per-app header
   `share_config.h` (see section 6). `share.h` does `#include "share_config.h"` at the
   top and defines none of those constants itself.
3. **If `flipper_share_uart/` or `flipper_share_rfid/` already exists in this repo, copy
   `share.c`, `share.h`, `md5_hash.c`, `md5_hash.h` from it verbatim instead of redoing
   the rename.** After this task, the engine files must be byte-identical across all new
   apps that exist. (Exception: `#ifdef FSH_CAROUSEL` guarded blocks added by the RFID
   app are part of the canonical engine; keep them — they compile out here.)
4. App shell files are renamed the same way and are also transport-neutral:
   `share_app.c/.h` (entry point symbol `share_app`), `scenes/share_scene_*.c/.h`.
   UI strings that named the transport ("Send via NFC" etc.) must use the
   `FSH_TRANSPORT_NAME` macro from `share_config.h` instead of a literal.
5. Per-app files (the only ones that differ between new apps): `application.fam`, the
   icon, `share_config.h`, and the transport layer (here: `ibutton_transport.*`).

The engine ↔ transport contract (same as the NFC app, keep it):

- Engine calls `cb_send_bytes(buf, len)` → wired to `ibutton_transport_send()`.
- Transport delivers each complete received packet via
  `void fsh_receive_callback(const uint8_t* buf, size_t size)` (declared in `share.h`),
  from a thread context (never from ISR).
- Scene `on_enter`/`on_exit` call `ibutton_transport_init(mode)` /
  `ibutton_transport_deinit()`; `mode ∈ {IbtnTransportModeSlave, IbtnTransportModeHost}`
  (sender / receiver), mirroring `NfcTransportMode` in the NFC app.

## 4. Files to produce

```
flipper_share_ibutton/
├── application.fam
├── ibutton_share.png         # 10x10 1-bit icon; copy ../flipper_share_nfc/nfc_share.png as placeholder
├── README.md                 # this file, updated with measured numbers after bench
├── share.h / share.c         # engine (rename of nfc_share.*, constants moved to config)
├── share_config.h            # all tunables, section 6
├── md5_hash.h / md5_hash.c   # verbatim copy
├── share_app.h / share_app.c
├── scenes/share_scene_*.c/.h # menu, file_browser, show_file, send, receive
├── ibutton_transport.h / ibutton_transport.c
└── .github/workflows/build.yml   # copy from ../flipper_share_ir/
```

`application.fam`:

```python
App(
    appid="flipper_share_ibutton",
    name="Flipper Share iButton",
    apptype=FlipperAppType.EXTERNAL,
    entry_point="share_app",
    stack_size=2 * 1024,
    fap_category="iButton",
    fap_version="0.1",
    fap_icon="ibutton_share.png",
    fap_description="Direct file transfer between flippers via iButton pads (1-Wire)",
    fap_author="@lomalkin",
    fap_weburl="https://github.com/lomalkin/flipper-zero-apps/blob/-/flipper_share_ibutton",
)
```

## 5. Transport design

### 5.1 Link layer: two custom commands

The host drives every transaction. A transaction is:
`onewire_host_reset()` → presence → one command byte → command-specific payload.

| Command | Direction after command byte | Payload |
|---|---|---|
| `IBTN_TP_CMD_POLL 0xA1` | slave → host | `len(1)` + `len` packet bytes; `len = 0x00` means "nothing queued" |
| `IBTN_TP_CMD_PUSH 0xA2` | host → slave | `len(1)` + `len` packet bytes |

- `len` must be `1..FSH_PACKET_MAX`; anything else aborts the transaction on both sides
  (the bus is left idle; the next reset resynchronizes).
- Packet bytes are a complete Flipper Share packet; integrity is the packet's own CRC16
  (validated by the engine) plus the final MD5. The link layer adds no CRC.
- Command codes avoid all standard 1-Wire ROM commands (0x33 READ ROM, 0xCC SKIP ROM,
  0xF0 SEARCH ROM, ...) so a foreign 1-Wire master touching the sender does nothing.

### 5.2 Slave side (sender)

- `onewire_slave_alloc(IBTN_TP_GPIO)`, set callbacks, `onewire_slave_start`.
- Reset callback: `return !is_short;` (participate in normal-speed resets only).
- Command callback (ISR context):
  - `IBTN_TP_CMD_POLL`: pick one outbound packet — control slot first, else data queue,
    else send `len = 0`. Send `len` then the bytes with `onewire_slave_send`; on `false`
    (bus error) just return — the packet is already dequeued and is lost; the protocol
    ARQ re-requests it.
  - `IBTN_TP_CMD_PUSH`: `onewire_slave_receive(&len, 1)`; validate `len`; receive `len`
    bytes into a local frame; `furi_message_queue_put(rx_queue, &frame, 0)` (0 timeout —
    ISR-safe); drop on full queue.
  - Any other command: return `false`.
- Outbound mailbox, same scheme as `../flipper_share_nfc/nfc_transport.c`: a 4-deep
  `FuriMessageQueue` for DATA packets — `ibutton_transport_send()` blocks up to
  `IBTN_TP_SEND_TIMEOUT_MS` when full (natural backpressure pacing the engine) — plus a
  single latest-wins control slot (ANNOUNCE) written under `FURI_CRITICAL_ENTER/EXIT`,
  drained before the data queue. Classify by packet type byte
  (`buf[FSH_HEADER_LENGTH - 1] != FSH_PKT_DATA` → control slot).
- `IbtnRxWorker` thread (stack 2048): blocks on `rx_queue`, calls
  `fsh_receive_callback(frame.buf, frame.len)` in thread context.

### 5.3 Host side (receiver)

`IbtnHostWorker` thread (stack 2048) owns the `OneWireHost` exclusively:

```
loop:
    if stop flag -> exit
    if outbound packet queued:            # receiver only produces REQUESTs
        reset; presence? -> write CMD_PUSH, len, bytes
    else:
        reset; presence? -> write CMD_POLL; len = read();
        if 1 <= len <= FSH_PACKET_MAX: read_bytes(buf, len); deliver to fsh_receive_callback()
    if no presence: sleep IBTN_TP_RECONNECT_MS; else sleep IBTN_TP_POLL_INTERVAL_MS
```

Host-side outbound uses the same mailbox structure as the slave (REQUEST goes to the
control slot; the host never sends DATA). `fsh_receive_callback` is called directly from
this worker thread — that is the thread context the engine already expects (NFC calls it
from the NfcWorker thread).

### 5.4 Timing budget

Standard slot ≈ 73 µs/bit → ~0.6 ms/byte. One DATA transaction: reset+presence ~1 ms +
cmd (1) + len (1) + packet (73) = 75 bytes ≈ 45 ms, plus `IBTN_TP_POLL_INTERVAL_MS` gap
→ ~19 packets/s × 64 file bytes ≈ **1.2 KB/s** expected payload throughput.

The slave services each transaction inside interrupt/critical context (~45 ms per DATA
frame). This is the transport's main systemic risk: keep `FSH_DATA_LENGTH` at 64 until
the bench confirms the UI, input, and BT stack stay healthy during a long transfer;
raising it to 128 is a pure config change afterwards.

## 6. `share_config.h` — all constants

| Constant | Value | Rationale |
|---|---|---|
| `FSH_TRANSPORT_NAME` | `"iButton"` | UI strings |
| `FSH_DATA_LENGTH` | `64` | 73-byte DATA packet ≈ 45 ms of bus time; tune after bench |
| `FSH_ANNOUNCE_INTERVAL_MS` | `1000` | host polls continuously; cheap |
| `FSH_ANNOUNCE_CONNECTED_MS` | `3000` | keep as NFC |
| `FSH_RX_TIMEOUT_MS` | `500` | REQUEST retry |
| `FSH_TX_TIMEOUT_MS` | `50` | keep as NFC |
| `FSH_IDLE_TICK_MS` | `20` | engine tick; pacing comes from the mailbox backpressure |
| `FSH_CONNECTED_IDLE_MS` | `5000` | keep as NFC |
| `FSH_REQUEST_JITTER_MS` | `50` | host drives the clock; collisions impossible |
| `FSH_PAYLOAD_THROUGHPUT_BPS` | `1200` | ETA estimate; REPLACE with measured value after bench |
| `FSH_STALL_MS` | `5000` | stall indicator |
| `IBTN_TP_GPIO` | `&gpio_ibutton` | PB14: iButton pad = header pin 17 |
| `IBTN_TP_CMD_POLL` | `0xA1` | see 5.1 |
| `IBTN_TP_CMD_PUSH` | `0xA2` | see 5.1 |
| `IBTN_TP_POLL_INTERVAL_MS` | `5` | gap between host transactions |
| `IBTN_TP_RECONNECT_MS` | `250` | retry period while no presence pulse |
| `IBTN_TP_SEND_TIMEOUT_MS` | `500` | blocking put into the data mailbox |
| `IBTN_TP_QUEUE_DEPTH` | `4` | outbound data mailbox and RX queue depth |

Engine constants that do not depend on the transport (`FSH_HASH_CHUNK_SIZE`,
`FSH_PARTS_COUNT`, `FSH_ETA_*`, packet structure sizes) stay in `share.h`. Add
`_Static_assert(FSH_PACKET_MAX <= 255, ...)` in `ibutton_transport.c` (single length
byte).

## 7. Scenes / UI

Five scenes as in the NFC app, renamed per section 3. Changes beyond the rename:

- All "via NFC" strings → `FSH_TRANSPORT_NAME`.
- Add one static hint line to the send and receive scenes' idle state:
  `"Touch iButton pads"`.
- No field to stop: drop the `stop_field()` calls; when the receiver finalizes, the host
  worker simply keeps polling until the scene exits (harmless), and the sender's slave
  stops answering after `ibutton_transport_deinit()`.

## 8. Edge cases

- **Pads separated mid-transfer**: host sees no presence, retries every
  `IBTN_TP_RECONNECT_MS`; on re-touch the block bitmap resumes the transfer. Verify in
  bench.
- **Corrupted transaction** (contact bounce): `onewire_slave_send/receive` returns false
  or the engine drops the packet on CRC16 — both silent; ARQ re-requests. Contact bounce
  at touch time is expected and harmless (reset/presence resynchronizes every
  transaction).
- **Foreign 1-Wire reader touches the sender**: standard ROM commands are not our command
  codes → command callback returns false; nothing leaks.
- **Both devices same role**: host+host → no presence pulses, both idle-retry.
  slave+slave → no master, silence. Document, nothing to code.
- **System load while slave streams**: see 5.4; acceptance requires the sender UI to stay
  responsive.

## 9. Testing

There is no host-testable modem here (the PHY is the firmware's 1-Wire driver); the link
layer is thin. Testing is bench-driven:

1. Wire link (pin 17 ↔ pin 17, GND ↔ GND): transfer an 8 KB file → MD5 OK; record
   wall-clock time; write measured B/s into `FSH_PAYLOAD_THROUGHPUT_BPS` and this README.
2. Pad-to-pad touch: repeat test 1. If presence detection is unreliable pad-to-pad,
   switch `IBTN_TP_GPIO` to `&gpio_ext_pa7`, note it in this README, and re-run.
3. Separate the devices for ~5 s mid-transfer, re-touch → transfer resumes, MD5 OK.
4. 64 KB file → completes; sender UI (progress redraw, Back button) stays responsive
   during the whole transfer; BT stays connected if it was connected.
5. Start receiver before sender and vice versa → both orders lock and complete.
6. Cancel on both sides mid-transfer → clean exit, no crash.

Acceptance = all six bench points pass + `ufbt` build is warning-clean.

## 10. Non-goals / v2 ideas

Overdrive slots (~8× throughput, needs bench proof that two software-timed sides hold
the tolerances); `FSH_DATA_LENGTH` 128 after load testing; a DS1996-emulation
compatibility mode (reading a whole 8 KB virtual iButton per chunk — zero custom PHY but
a different, clunkier session model; rejected for v1).

## 11. Implementation notes / forced deviations

Implemented as written except for the points below.

- **Engine ↔ transport TX symbol.** Section 3's contract says the engine's
  `cb_send_bytes` is "wired to `ibutton_transport_send()`". A byte-identical engine
  (rule 3) cannot reference a transport-specific name, so the engine's neutral TX
  symbol is **`fsh_transport_send`** (declared in `share.h`, defined in
  `ibutton_transport.c`) — the exact mirror of the existing neutral RX symbol
  `fsh_receive_callback`. There is no `ibutton_transport_send`. The other transport
  entry points keep the spec's names (`ibutton_transport_init` /
  `_deinit` / `_stop_field`, `IbtnTransportModeSlave` / `Host`) because only the
  per-app scenes call them, not the engine. This is the single naming deviation and it
  is what lets `share.c` / `share.h` stay byte-identical across `flipper_share_uart` /
  `_rfid` / `_ibutton`.

- **`nfc_transport` coupling removed from the engine.** The mechanical rename left
  `nfc_share.c`'s `#include "nfc_transport.h"` and `ps.send_bytes = nfc_transport_send`,
  which do not belong in a transport-neutral engine. Both were replaced by the neutral
  `fsh_transport_send` above; no transport header is included by the engine. All the
  transport-tunable constants were moved to `share_config.h` as specified.

- **"NFC" still appears in a few engine comments.** Those are the verbatim output of the
  mechanical rename (the token "NFC" is not one of the four renamed strings). They are
  left untouched on purpose so a future `uart` / `rfid` engine produced the same way is
  byte-identical to this one. Per-app files (scenes, transport) were reworded to
  "1-Wire".

- **Rule 3 was not applicable.** At implementation time `flipper_share_uart/` and
  `flipper_share_rfid/` contained only their README, so there was no existing engine to
  copy verbatim; this app defines the canonical `share.c` / `share.h` / `md5_hash.*` for
  the new-app family. No `#ifdef FSH_CAROUSEL` blocks exist yet.

- **API re-verified.** All named 1-Wire host/slave symbols and `gpio_ibutton` are still
  exported at the current SDK **API 88.2** (README was written against 87.x). The slave
  command/reset callbacks run in the GPIO EXTI interrupt inside a `FURI_CRITICAL`
  section, so the ISR paths use only `furi_message_queue_put/get(..., 0)` (which route to
  the `*FromISR` variants) and a `FURI_CRITICAL`-guarded control slot — no mutexes, no
  storage, no blocking, as required.

**Build status:** builds warning-clean with the firmware SDK (`fbt fap_flipper_share_ibutton`,
API 88.2); `APPCHK` passes (all imports resolve on unmodified official firmware). `ufbt`
was unavailable in the build environment, so the local firmware `fbt` was used instead —
same compiler, flags and API-symbol check as `ufbt`. The `.github/workflows/build.yml`
(copied from `flipper_share_ir`) builds with `ufbt` on CI.

**Bench:** not yet run (requires two devices). `FSH_PAYLOAD_THROUGHPUT_BPS` stays at the
`1200` estimate until section 9 is executed; replace it and the section 9 numbers with the
measured value then.
