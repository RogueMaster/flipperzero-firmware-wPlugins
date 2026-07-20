# Flipper Share RFID — feasibility research

Status: **research only, not implemented**. This document captures the findings of a
feasibility study for a `flipper_share_rfid` app — file transfer between two Flipper Zeros
over the 125 kHz LF RFID coil, using the Flipper Share protocol (see `../flipper_share/README.md`)
with a custom LF RFID transport. It is intended as the starting point for a future
implementation.

All API availability claims below were verified against the official ufbt SDK export table
(`~/.ufbt/current/sdk_headers/f7_sdk/targets/f7/api_symbols.csv`, SDK for official firmware).
The app would build with `ufbt` and run on the official firmware — no firmware changes needed.

## Verdict

Feasible. One Flipper acts as an LF **reader** (drives the 125 kHz carrier and demodulates
load-modulation), the other acts as a passive **tag** (load-modulates the reader's field).
Realistic effective throughput is roughly **400–700 bytes/s** in the simplest ("carousel")
design — comparable to the Sub-GHz version — but the implementation cost is the highest of
all transports: a custom software modem must be written for at least one direction, and the
analog side needs bench tuning on real hardware. The two devices must be held with their
RFID coils essentially touching (~1–2 cm, alignment-sensitive).

## Role mapping

| Flipper Share role | LF RFID role | Mechanism |
|---|---|---|
| Sender ("Send via RFID") | Tag (emulator) | Passive load modulation, clocked by the reader's field |
| Receiver ("Receive via RFID") | Reader | Drives the 125 kHz carrier, captures demodulated pulses |

The emulate timer is clocked **by the external field** (TIM2 in external-clock mode from
`gpio_rfid_carrier`), so the sender can only transmit while the receiver's field is present.
This is convenient: bringing the devices together automatically starts the link, and
separating them pauses it — the protocol's block bitmap makes the transfer resume where it
stopped once the devices touch again.

## Exported HAL API (all callable from a .fap)

Everything needed is exported except `furi_hal_rfid_init`, which the system calls at boot
(the HAL context is already initialized for apps). Header:
`targets/f7/furi_hal/furi_hal_rfid.h`.

Reader side:
- `furi_hal_rfid_tim_read_start(float freq, float duty)` / `furi_hal_rfid_tim_read_stop()` —
  125 kHz carrier on/off (TIM1 → `gpio_rfid_carrier_out`).
- `furi_hal_rfid_tim_read_pause()` / `furi_hal_rfid_tim_read_continue()` — momentary field
  gaps (used by T5577 write; reusable as an OOK downlink).
- `furi_hal_rfid_tim_read_capture_start(FuriHalRfidReadCaptureCallback cb, void* ctx)` /
  `..._stop()` — demodulated envelope edges as `(bool level, uint32_t duration)` pairs,
  1 µs resolution (COMP1 → TIM2 input capture).

Tag side:
- `furi_hal_rfid_tim_emulate_dma_start(uint32_t* duration, uint32_t* pulse, size_t length,
  FuriHalRfidDMACallback cb, void* ctx)` / `..._stop()` — load modulation via double-buffered
  circular DMA; `duration`/`pulse` are TIM2 ARR/CCR3 values in **carrier cycles** (8 µs each);
  the callback fires at half/full transfer so the inactive half can be refilled live.
- `furi_hal_rfid_comp_start()` / `furi_hal_rfid_comp_set_callback(cb, ctx)` / `..._stop()` —
  raw comparator edges; independent of TIM2, usable to detect reader field gaps.
- `furi_hal_rfid_field_detect_start()` / `furi_hal_rfid_field_is_present(uint32_t* freq)` —
  coarse "is a field present" check (80–200 kHz), as used by the official
  `nfc_rfid_detector` app.
- `furi_hal_rfid_pins_reset()`, `furi_hal_rfid_pin_pull_release()/_pulldown()`.

Not usable for a live link:
- `lfrfid_worker_*` — locked to card protocols (EM4100 etc., 40-bit payloads).
- `lfrfid_raw_worker_*` — exported, but strictly **file-backed** (records/replays `.raw`
  files on storage); no live-buffer streaming API. A custom modem over the raw HAL is
  required instead.

## Hardware constraints

- **TIM2 is shared** between emulate-DMA, read-capture, and the field counter
  (`targets/f7/furi_hal/furi_hal_rfid.c`: `EMULATE_TIMER`, `RFID_CAPTURE_TIM`,
  `FIELD_COUNTER_TIMER` are all TIM2). On a single device, emulation and capture/field-detect
  cannot run simultaneously — any bidirectional scheme is strictly half-duplex with explicit
  mode turnarounds. The comparator callback is the only receive path that can coexist with
  emulation hardware-wise. (No conflict *between* two devices — only within one.)
- Coupling: both coils are small, low-Q, tuned for a card at 1–5 cm. Two Flippers need to be
  back-to-back with coils aligned; expect ~1–2 cm max and sensitivity to lateral offset.
  Weak coupling reduces modulation depth at the reader's comparator, which is what caps the
  usable symbol rate.

## Modulation and throughput estimates

- Carrier 125 kHz → 8 µs per carrier cycle. Capture resolution 1 µs.
- EM4100-style Manchester is the proven encoding; the firmware supports clock-per-bit of
  RF/64, RF/32, RF/16 (`lib/lfrfid/protocols/protocol_em4100.c`):
  - RF/64 → ~1.95 kbit/s raw (robust)
  - RF/32 → ~3.9 kbit/s raw
  - RF/16 → ~7.8 kbit/s raw (bit period 128 µs; upper end of what the comparator/envelope
    path recovers reliably)
- Uplink (tag→reader) budget with Flipper Share v2 framing (61-byte DATA packet carrying
  52 bytes of file data; larger blocks would improve the ratio): at RF/16, one packet ≈
  488 bits + preamble/sync ≈ 66 ms → **~780 B/s ceiling**; realistically **400–700 B/s**
  with resync gaps. At RF/32 roughly half that.
- Downlink (reader→tag), if implemented: OOK via field gaps, T5577-write style
  (`lib/lfrfid/tools/t5577.c` timings: start gap 30 cycles/240 µs, write gap 18 cycles/144 µs,
  data-0 24 cycles/192 µs, data-1 56 cycles/448 µs) → ~1–2 kbit/s, detected on the tag side
  via the comparator callback (edges stop during a gap).

## Design fork (open decision)

Two viable architectures, in order of recommendation at the time of the study:

1. **Carousel (recommended v1).** The sender continuously streams ANNOUNCE followed by all
   DATA blocks in a loop; the receiver silently fills its block bitmap, deduplicates, and
   finalizes on MD5 match. No REQUEST packets, no downlink modem, no turnarounds — roughly
   half the code, naturally resumable, and *faster* (no half-duplex overhead). Deviation from
   the classic protocol: the receiver never transmits, and the sender cannot display receiver
   progress or auto-stop (it loops until the user exits). Packet formats stay unchanged.
2. **Full protocol.** Receiver-driven REQUESTs sent as field-gap OOK, sender responds with
   the requested blocks — faithful to the classic Flipper Share flow, sender sees progress,
   but requires the downlink modem, gap detection during emulation, half-duplex turnaround
   logic, and ends up slower (~150–400 B/s effective).
3. **Hybrid:** carousel in v1, add the downlink (ACK/complete + targeted re-requests) in v2
   without breaking the packet format.

## Sketch of a future implementation

Mirror the structure of `flipper_share_ir` (which proved the transport seam):

- `rfid_share.c/.h` — protocol engine, fork of `../flipper_share_ir/ir_share.c` with the
  `rsh_`/`RSH_` prefix (carousel mode would strip the REQUEST path on the receiver side).
- `rfid_share_app.c/.h`, `scenes/rfid_share_scene_*` — GUI, same five scenes, texts
  "Send via RFID" / "Receiving via RFID..." etc.
- `rfid_transport.c/.h` — role-aware glue: reader = carrier + capture callback feeding the
  demodulator; tag = DMA emulation fed by the encoder (live refill in the half/full DMA
  callback, like `lfrfid_raw_worker.c` does from file).
- `rfid_modem.c/.h` + `rfid_modem_config.h` — pure, host-testable codec (like `ir_modem.c`):
  Manchester encoder to (duration, pulse) carrier-cycle pairs, and a pulse-train decoder with
  sync/preamble detection; all timings in one config header. The EM4100 encoder
  (`protocol_em4100.c`) and the raw worker's cycle math are the reference implementations.
- A host-side modem test harness (like `../flipper_share_ir/tools/modem_test.c`) is strongly
  recommended — the decode tolerances will need the same jitter-budget treatment the IR modem
  got.

Start at RF/32 for robustness, move to RF/16 after bench validation.
