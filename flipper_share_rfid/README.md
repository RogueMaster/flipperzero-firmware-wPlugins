# Flipper Share RFID — implementation specification

Status: **specification, not implemented** (rewritten from the earlier feasibility
research; all research findings were re-verified against the official firmware source and
the SDK export table, and the open design fork has been resolved — v1 is the carousel
design, see section 5). This document is a complete, self-contained work order for
implementing `flipper_share_rfid` — file transfer between two Flipper Zeros over the
125 kHz LF RFID coil, using the Flipper Share v2 protocol. All design decisions below are
final; implement as written unless something is physically impossible, and record any
forced deviation in this README.

The app must build with `ufbt` against the official firmware SDK and run on unmodified
official firmware. Every firmware API named below is exported to FAPs (verified against
`targets/f7/api_symbols.csv`, API 87.x); re-verify against the current SDK before coding.

## 1. Goal and scope

- Wireless file transfer, devices held back-to-back with RFID coils aligned (~1–2 cm,
  alignment-sensitive).
- Roles: **sender = tag** (passive load modulation, clocked by the reader's field),
  **receiver = reader** (drives the 125 kHz carrier, demodulates load modulation) —
  consistent with the other transports where the sender is the passive side.
- v1 is **one-way on the air** (carousel): the receiver never transmits. Realistic
  effective throughput ~**400 B/s** at RF/32 (an 8 KB file in ~20–30 s when coupling is
  good).
- The emulate timer is clocked by the external field (TIM2 in external-clock mode from
  `gpio_rfid_carrier`), so the sender transmits only while the receiver's field is
  present: bringing the devices together starts the link, separating them pauses it, and
  the protocol's block bitmap resumes the transfer on re-touch.

Out of scope for v1 (v2 candidates, do not implement): the reader→tag downlink (OOK via
field gaps), RF/16 clock, receiver-driven REQUESTs, PSK.

## 2. Firmware API (verified)

Header `targets/f7/furi_hal/furi_hal_rfid.h`. Everything needed is exported except
`furi_hal_rfid_init` (the system initializes the HAL at boot).

Reader side (receiver):
- `furi_hal_rfid_tim_read_start(float freq, float duty_cycle)` / `..._stop()` — 125 kHz
  carrier on/off (TIM1 → `gpio_rfid_carrier_out`). Use `freq = 125000.f`,
  `duty = 0.5f`.
- `furi_hal_rfid_tim_read_capture_start(FuriHalRfidReadCaptureCallback cb, void* ctx)` /
  `..._stop()` — demodulated envelope edges as `(bool level, uint32_t duration)` pairs at
  1 µs resolution (COMP1 → TIM2 input capture). This is the RX path.
- `furi_hal_rfid_tim_read_pause()` / `..._continue()` — field gaps (v2 downlink only).
- `furi_hal_rfid_pins_reset()` on teardown.

Tag side (sender):
- `furi_hal_rfid_tim_emulate_dma_start(uint32_t* duration, uint32_t* pulse, size_t
  length, FuriHalRfidDMACallback cb, void* ctx)` / `..._stop()` — load modulation via
  double-buffered circular DMA. `duration`/`pulse` are TIM2 ARR/CCR3 values in **carrier
  cycles** (8 µs each); the callback fires at DMA half/full transfer so the inactive half
  is refilled live. Reference for the refill pattern and the cycle math:
  `lib/lfrfid/lfrfid_raw_worker.c` (it divides microsecond timings by 8).
- `furi_hal_rfid_field_detect_start()` / `furi_hal_rfid_field_is_present(uint32_t*)` —
  coarse field presence, used for the sender's "waiting for receiver" UI state.

Findings that constrain the design (verified in `targets/f7/furi_hal/furi_hal_rfid.c`):
- **TIM2 is shared** between emulate-DMA, read-capture, and the field counter
  (`EMULATE_TIMER`, `RFID_CAPTURE_TIM`, `FIELD_COUNTER_TIMER` are all TIM2). On one
  device, emulation and capture/field-detect cannot run simultaneously — the sender must
  stop field-detect before starting emulation. (No conflict between two devices.)
- The emulate path is duration/pulse OOK/ASK only — **PSK TX is not available** (PSK
  exists only in read-side protocol decoders). Manchester ASK it is.
- `lfrfid_worker_*` is locked to card protocols (40-bit payloads) and
  `lfrfid_raw_worker_*` is strictly file-backed (records/replays `.raw` files, no live
  buffer streaming) — neither is usable; a custom modem over the raw HAL is required.
  (The "Debug mode" requirement for RAW RFID mentioned in firmware docs is UI-only and
  irrelevant here.)
- Weak coil coupling reduces modulation depth at the reader's comparator; that — not the
  API — caps the usable symbol rate. Both coils are small, low-Q, tuned for a card at
  1–5 cm.

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
   `share_config.h` (see section 7). `share.h` does `#include "share_config.h"` at the
   top and defines none of those constants itself.
3. **If `flipper_share_uart/` or `flipper_share_ibutton/` already exists in this repo,
   copy `share.c`, `share.h`, `md5_hash.c`, `md5_hash.h` from it verbatim as the starting
   point instead of redoing the rename**, then add the `FSH_CAROUSEL` blocks (section 5).
   The carousel blocks become part of the canonical engine: backport them verbatim to the
   other new apps' engine copies (they compile out there — `FSH_CAROUSEL` is defined only
   in this app's `share_config.h`), so the copies stay byte-identical.
4. App shell files are renamed the same way and are also transport-neutral:
   `share_app.c/.h` (entry point symbol `share_app`), `scenes/share_scene_*.c/.h`.
   UI strings that named the transport ("Send via NFC" etc.) must use the
   `FSH_TRANSPORT_NAME` macro from `share_config.h` instead of a literal.
5. Per-app files (the only ones that differ between new apps): `application.fam`, the
   icon, `share_config.h`, and the transport layer (here: `rfid_transport.*`,
   `rfid_modem.*`).

The engine ↔ transport contract (same as the NFC app, keep it):

- Engine calls `cb_send_bytes(buf, len)` → wired to `rfid_transport_send()`.
- Transport delivers each complete received packet via
  `void fsh_receive_callback(const uint8_t* buf, size_t size)` (declared in `share.h`),
  from a thread context (never from ISR).
- Scene `on_enter`/`on_exit` call `rfid_transport_init(mode)` /
  `rfid_transport_deinit()`; `mode ∈ {RfidTransportModeTag, RfidTransportModeReader}`
  (sender / receiver), mirroring `NfcTransportMode` in the NFC app.
- `rfid_transport_stop_field()` — receiver only: stop the carrier after the transfer
  finalizes without tearing the transport down (mirrors `nfc_transport_stop_field`).

## 4. Files to produce

```
flipper_share_rfid/
├── application.fam
├── rfid_share.png            # 10x10 1-bit icon; copy ../flipper_share_nfc/nfc_share.png as placeholder
├── README.md                 # this file, updated with measured numbers after bench
├── share.h / share.c         # engine (rename of nfc_share.* + FSH_CAROUSEL blocks)
├── share_config.h            # all tunables, section 7
├── md5_hash.h / md5_hash.c   # verbatim copy
├── share_app.h / share_app.c
├── scenes/share_scene_*.c/.h # menu, file_browser, show_file, send, receive
├── rfid_transport.h / rfid_transport.c   # HAL glue: DMA refill (tag), capture feed (reader)
├── rfid_modem.h / rfid_modem.c           # pure codec, no furi includes (host-testable)
├── rfid_modem_config.h                   # all modem timings/tolerances
├── tools/modem_test.c        # host test harness, see section 10
└── .github/workflows/build.yml           # copy from ../flipper_share_ir/
```

`application.fam`:

```python
App(
    appid="flipper_share_rfid",
    name="Flipper Share RFID",
    apptype=FlipperAppType.EXTERNAL,
    entry_point="share_app",
    stack_size=2 * 1024,
    fap_category="RFID",
    fap_version="0.1",
    fap_icon="rfid_share.png",
    fap_description="Direct file transfer between flippers via 125 kHz RFID coil",
    fap_author="@lomalkin",
    fap_weburl="https://github.com/lomalkin/flipper-zero-apps/blob/-/flipper_share_rfid",
)
```

## 5. Carousel mode (engine change, `#ifdef FSH_CAROUSEL`)

v1 has no uplink from the receiver, so the classic ANNOUNCE→REQUEST→DATA flow is replaced
by a broadcast carousel. All changes to `share.c` are guarded by `#ifdef FSH_CAROUSEL`
(defined in this app's `share_config.h` only) so the engine stays a single canonical
source across the new apps. Packet formats are unchanged.

Sender (`fsh_idle`, sender branch):
- After file selection and MD5 hashing, enter ANNOUNCING and stream continuously: every
  `RFID_CAROUSEL_ANNOUNCE_EVERY`-th frame is an ANNOUNCE, all other frames are DATA
  blocks in round-robin order (0, 1, ..., N-1, 0, ...). One `fsh_send_data`-equivalent
  call per idle tick; pacing comes from the transport's blocking send (section 6.2).
- REQUEST handling and the CONNECTED state are compiled out; the sender never leaves
  ANNOUNCING and loops until the user exits. The send scene shows blocks-sent and
  loop-count counters instead of receiver progress (which is unknowable in v1).

Receiver:
- Never transmits: the REQUEST path (`fsh_send_request` calls and the RX-timeout
  re-request logic) is compiled out; `cb_send_bytes` may be NULL.
- Everything else is untouched: lock to the first valid ANNOUNCE's `tx_id`, fill the
  block bitmap, deduplicate, finalize on bitmap-complete with the MD5 check.

Resume works naturally: separating the devices pauses the stream; on re-touch the
receiver keeps its bitmap and picks up the missing blocks as the carousel comes around.

## 6. Transport design

### 6.1 Modem (`rfid_modem.c/.h` — pure C, host-testable, no furi includes)

- Line code: **Manchester ASK at RF/32** — bit period 32 carrier cycles = 256 µs; each
  half-bit is 16 cycles. (RF/16 is a v2 upgrade after bench validation.) Reference
  encodings: `lib/lfrfid/protocols/protocol_em4100.c`.
- Frame: `[preamble: 16 bits of logical 1][sync byte 0x7E][len byte][packet bytes]`,
  bits LSB-first within bytes (consistent with the IR modem). `len` is the Flipper Share
  packet length; valid range `1..FSH_PACKET_MAX`. No modem CRC — the packet CRC16 and
  final MD5 do the filtering; a false sync lock wastes at most one frame, which the
  carousel re-delivers next cycle.
- Inter-frame gap: `RFID_MODEM_IFG_BITS` (4) bit periods of no modulation. On the reader
  side any silence longer than ~2.5 bit periods resets the decoder to sync-hunt state.
- Encoder (used by the tag): incremental, pull-based — the DMA refill callback asks for
  the next `(duration, pulse)` pairs. Output pairs are in carrier cycles: each pair is
  one high-run + low-run of the Manchester waveform (`pulse` = high run, `duration` =
  high + low run); runs are 16 or 32 cycles, merged across half-bit boundaries.
  API sketch: `rfid_modem_enc_set_frame(enc, bytes, len)` /
  `bool rfid_modem_enc_next(enc, uint32_t* duration, uint32_t* pulse)` (returns false at
  frame end; encoder then emits the inter-frame gap and reports idle).
- Decoder (used by the reader): fed with capture events,
  `rfid_modem_dec_feed(dec, bool level, uint32_t duration_us)` → returns a complete
  `[len][packet]` when a frame closes. Classify runs into half-bit (256/2 = 128 µs) and
  full-bit (256 µs) buckets with ±`RFID_MODEM_TOLERANCE_PCT` (25%) windows; rebuild the
  bit stream; hunt for preamble+sync with a sliding bit window. **The decoder must be
  polarity-agnostic**: comparator polarity is not guaranteed, so if sync-hunt fails on
  the direct phase, retry on the inverted phase (Manchester phase ambiguity).
- All numeric constants live in `rfid_modem_config.h`.

### 6.2 Tag side (sender) — `rfid_transport.c`

- Init: stop field-detect if it was started (TIM2 conflict), allocate two DMA
  half-buffers of `RFID_TP_DMA_HALF_PAIRS` (256) pairs, prefill both halves from the
  encoder (idle pattern if no frame queued), then `furi_hal_rfid_tim_emulate_dma_start`.
- DMA callback (ISR): refill the inactive half from the encoder. When the encoder is
  idle, emit unmodulated carrier-cycle pairs (load off).
- `rfid_transport_send(buf, len)`: single-slot mailbox. Blocks up to
  `RFID_TP_SEND_TIMEOUT_MS` (1000) while the encoder is busy with the previous frame —
  this is the backpressure that paces the engine's carousel loop. On timeout (no field →
  DMA not clocked → encoder never drains) the packet is dropped; the carousel re-sends it
  next cycle.
- Before emulation starts (scene idle state), use
  `furi_hal_rfid_field_detect_start`/`furi_hal_rfid_field_is_present` for a "Waiting for
  receiver..." UI hint; stop field-detect before `furi_hal_rfid_tim_emulate_dma_start`.

### 6.3 Reader side (receiver) — `rfid_transport.c`

- Init: `furi_hal_rfid_tim_read_start(125000.f, 0.5f)`, then
  `furi_hal_rfid_tim_read_capture_start(capture_cb, ctx)`.
- Capture callback (ISR): pack `(level, duration)` into a `uint32_t` and push into a
  `FuriStreamBuffer` (size `RFID_TP_RX_STREAM_SIZE`) — same event-packing pattern as
  `../flipper_share_ir/ir_transport.c`.
- `RfidRxWorker` thread (stack 2048): drain the stream buffer, feed the decoder, deliver
  complete packets to `fsh_receive_callback`.
- `rfid_transport_stop_field()`: stop capture + carrier (called by the receive scene
  after finalization, like the NFC app's `stop_field`); `rfid_transport_deinit` also
  calls `furi_hal_rfid_pins_reset()`.

### 6.4 Throughput budget (for ETA and sanity checks)

At RF/32: DATA frame = 16 + 8 + 8 + 73×8 = 616 bits ≈ 158 ms + gap ≈ 160 ms → ~6.2
DATA frames/s × 64 file bytes ≈ **~400 B/s**. ANNOUNCE frame (61-byte packet) ≈ 133 ms.
Full carousel cycle for an 8 KB file (128 blocks + 4 ANNOUNCEs) ≈ 21 s — that is also the
worst-case wait for a single missed block.

## 7. `share_config.h` — all constants

| Constant | Value | Rationale |
|---|---|---|
| `FSH_TRANSPORT_NAME` | `"RFID"` | UI strings |
| `FSH_CAROUSEL` | defined | enables carousel engine mode (section 5) |
| `FSH_DATA_LENGTH` | `64` | 158 ms frame at RF/32; frame-error rate rises with length — raise to 128 only after bench shows <10% frame loss |
| `FSH_ANNOUNCE_INTERVAL_MS` | `1000` | compiled out under carousel; keep nominal |
| `FSH_ANNOUNCE_CONNECTED_MS` | `3000` | compiled out under carousel; keep nominal |
| `FSH_RX_TIMEOUT_MS` | `500` | compiled out under carousel; keep nominal |
| `FSH_TX_TIMEOUT_MS` | `0` | sender streams unconditionally |
| `FSH_IDLE_TICK_MS` | `20` | engine tick; pacing comes from blocking send |
| `FSH_CONNECTED_IDLE_MS` | `5000` | compiled out under carousel; keep nominal |
| `FSH_REQUEST_JITTER_MS` | `0` | receiver never transmits |
| `FSH_PAYLOAD_THROUGHPUT_BPS` | `380` | ETA estimate; REPLACE with measured value after bench |
| `FSH_STALL_MS` | `15000` | must exceed one carousel cycle for small files; tune |
| `RFID_CAROUSEL_ANNOUNCE_EVERY` | `32` | one ANNOUNCE per 32 DATA frames (~5 s lock latency) |
| `RFID_TP_DMA_HALF_PAIRS` | `256` | ~65 ms of waveform per half buffer |
| `RFID_TP_SEND_TIMEOUT_MS` | `1000` | blocking send; expires only when no field |
| `RFID_TP_RX_STREAM_SIZE` | `4096` | capture events headroom (bytes) |

Modem constants (`RFID_MODEM_RF_K` = 32, half-bit 128 µs, `RFID_MODEM_TOLERANCE_PCT` =
25, preamble length 16, sync `0x7E`, `RFID_MODEM_IFG_BITS` = 4) live in
`rfid_modem_config.h`.

## 8. Scenes / UI

Five scenes as in the NFC app, renamed per section 3. Changes beyond the rename:

- All "via NFC" strings → `FSH_TRANSPORT_NAME`.
- Add a hint line to both send and receive idle states: `"Hold backs together"`.
- Send scene: replace receiver-progress UI with `sent N blk, loop M` counters (carousel
  has no return channel); keep the file name/size display. Sender shows "Waiting for
  field..." until `furi_hal_rfid_field_is_present` first fires.
- Receive scene: unchanged behavior; call `rfid_transport_stop_field()` after
  finalization (mirrors the NFC receive scene's `stop_field` call site).

## 9. Edge cases

- **Devices separated mid-transfer**: tag DMA stops being clocked (no field) → blocking
  send times out → frames drop; receiver bitmap keeps state; on re-touch the carousel
  refills the gaps. Verify in bench.
- **Misalignment / weak coupling**: shows up as frame CRC failures → dropped frames →
  slower effective rate, not corruption (CRC16 + MD5).
- **Foreign reader polls the sender** (e.g., a door reader): it sees Manchester noise,
  not a valid card — no interop concerns. A foreign 125 kHz tag near the receiver adds
  decoder noise; CRC16 filters it.
- **TIM2 ownership**: never run field-detect and emulation simultaneously on the sender
  (section 6.2); the receiver's capture owns TIM2 for the whole scene.
- **Sender exit**: user-initiated only (carousel has no completion signal in v1) — the
  send scene's Back handling must stop the engine worker before
  `rfid_transport_deinit()` (same contract as NFC).

## 10. Testing

Host tests (no hardware) — MANDATORY, this transport lives or dies by decode tolerances
(same lesson as the IR modem):

- `tools/modem_test.c`, compiled with `cc tools/modem_test.c rfid_modem.c` (pattern:
  `../flipper_share_ir/tools/modem_test.c`). Encode frames → convert pairs to
  (level, duration_us) events → perturb → decode:
  - Clean pass: 0 failures over 10000 random frames (1..FSH_PACKET_MAX = 73-byte
    packets).
  - Realistic pass (duration jitter ±30 µs, occasional split/merged edges): 0 failures.
  - Harsh pass (jitter > half-bit tolerance): expected to fail — record the error budget.
  - Polarity-inverted stream: must decode (section 6.1).
  - Truncated frame + immediate next frame: decoder resynchronizes on the next preamble.

Bench (two Flippers, backs together):
1. 1 KB file → completes, MD5 OK; record time.
2. 8 KB file → completes, MD5 OK; write measured B/s into `FSH_PAYLOAD_THROUGHPUT_BPS`
   and this README; record observed frame-loss rate at good alignment.
3. Separate devices ~5 s mid-transfer, re-touch → resumes and completes, MD5 OK.
4. Start receiver first and sender first → both orders work (receiver locks on the next
   ANNOUNCE frame, ≤ ~5 s).
5. Deliberate misalignment (~1 cm lateral offset) → transfer still completes (slower).
6. Cancel both sides mid-transfer → clean exit, no crash, `furi_hal_rfid_pins_reset()`
   restores pins (verify the stock 125 kHz Read app still works afterwards).

Acceptance = host tests clean+realistic pass, bench points 1–6 pass, `ufbt` build
warning-clean. If RF/32 frame loss at good alignment exceeds ~10%, do NOT chase RF/16 —
fix tolerances first.

## 11. Non-goals / v2 roadmap

- **Downlink (reader→tag)** for ACK/progress/targeted re-requests: OOK via
  `furi_hal_rfid_tim_read_pause/continue` field gaps (T5577-write style timings in
  `lib/lfrfid/tools/t5577.c`); tag-side detection via `furi_hal_rfid_comp_start` +
  `furi_hal_rfid_comp_set_callback` (the comparator is the only RX path that can coexist
  with emulation), or by detecting the emulate-DMA callback stall (the external-clock TIM2
  stops when the field drops). ~1–2 kbit/s; enables sender-side progress and auto-stop
  without breaking the packet format.
- RF/16 (~2× throughput) after tolerance validation; `FSH_DATA_LENGTH` 128.
- Adaptive announce interleave (denser ANNOUNCEs until first DATA cycle completes).

## 12. Implementation notes / forced deviations

Implemented as written except for the points below.

- **Canonical engine now carries the `FSH_CAROUSEL` blocks (section 5).** `share.c` /
  `share.h` are the byte-identical engine started from `flipper_share_ibutton` verbatim,
  with the carousel blocks added under `#ifdef FSH_CAROUSEL` (defined only in this app's
  `share_config.h`): the sender's `fsh_idle` streams round-robin DATA with an interleaved
  ANNOUNCE every `RFID_CAROUSEL_ANNOUNCE_EVERY` frames, and the receiver's REQUEST /
  re-request path is compiled out. New engine state (`c_next_block`, `c_frame_counter`,
  `c_blocks_sent`, `c_loop_count`) sits in an `#ifdef FSH_CAROUSEL` tail of `fsh_ctx_t`.
  These blocks compile out in the UART app, whose engine copy is byte-identical to this one.
- **`flipper_share_ibutton`'s engine copy is not byte-identical here (yet).** It was
  shipped before the carousel blocks existed and its branch/PR were left untouched by this
  task. To complete the backport rule, the same `#ifdef FSH_CAROUSEL` blocks should be
  copied verbatim into the ibutton engine (they compile out there). UART and RFID engines
  ARE byte-identical.
- **Engine TX symbol is the neutral `fsh_transport_send`** (mirror of `fsh_receive_callback`),
  not `rfid_transport_send`, for the same byte-identical-engine reason documented in the
  ibutton app. The receiver never transmits, so on the reader `fsh_transport_send` is a
  no-op; `cb_send_bytes` stays non-NULL (wired to it) so `fsh_init` still accepts the
  receiver.
- **One canonical-engine robustness fix:** the classic re-request jitter used
  `furi_get_tick() % FSH_REQUEST_JITTER_MS`, which is a modulo-by-zero (compile error under
  `-Werror`, or a runtime fault) for the full-duplex transports that set the jitter to 0
  (UART). It now reads the constant into a local and guards `if(jitter)`, so a zero jitter
  simply skips the backoff. Behaviour is unchanged where the jitter is non-zero.
- **Modem validated on the host.** `tools/modem_test.c` (`cc tools/modem_test.c
  rfid_modem.c`, `-Wall -Wextra -Werror`) passes 0/22006 mandatory checks: fixed vectors,
  10000 clean random frames, 10000 at +/-30 us jitter, 2000 polarity-inverted, and the
  truncation-resync case. The harsh +/-45 us pass is informational and fails by design
  (a single run outside the +/-25% window drops the whole frame; the carousel re-delivers).
- **API re-verified at SDK release 1.4.3 (API 87.1).** All `furi_hal_rfid_*` symbols named
  in section 2 are exported. The tag follows the firmware's own `lfrfid_raw_worker` split:
  the emulate-DMA interrupt only posts a half/full flag to a stream buffer, and the tag
  worker thread does the refill (ARR = period cycles - 1, CCR3 = load-on cycles), so no
  heavy work runs in the ISR.

**Build status:** builds warning-clean with `ufbt` (SDK release 1.4.3 / API 87.1); `APPCHK`
passes on unmodified official firmware. `tools/` is kept out of the FAP via
`sources=["*.c*", "!tools"]` in `application.fam`.

**Bench:** not yet run (needs two devices). `FSH_PAYLOAD_THROUGHPUT_BPS` stays at the `380`
estimate until section 10 is executed.
