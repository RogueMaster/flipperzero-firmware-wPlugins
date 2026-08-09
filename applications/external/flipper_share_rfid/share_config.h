#pragma once

// Per-app tunables for Flipper Share over the 125 kHz LF RFID coil. This is the
// single place to tweak the transport and the transport-dependent engine knobs;
// changing anything here requires a recompile. The engine (share.c/share.h) and
// the shared scenes read these but define none of them, so the engine files
// stay byte-identical across all new Flipper Share apps.

// ===== Engine-facing tunables (consumed by share.c / the scenes) =============

// Name of this transport, substituted into every UI string ("Send via ...").
#define FSH_TRANSPORT_NAME "RFID"

// Enables the one-way broadcast "carousel" engine mode (see README section 5).
// Defined ONLY in this app; the #ifdef FSH_CAROUSEL blocks in share.c compile
// out in every other new app, so the engine file stays byte-identical.
#define FSH_CAROUSEL

// File-data bytes per DATA packet. 64 -> a ~158 ms frame at RF/32. Frame-error
// rate rises with length; raise to 128 only after the bench shows <10% loss.
#define FSH_DATA_LENGTH 64u

// Announce cadence (compiled out under carousel — the sender streams unconditionally
// and interleaves ANNOUNCEs via RFID_CAROUSEL_ANNOUNCE_EVERY). Kept nominal.
#define FSH_ANNOUNCE_INTERVAL_MS  1000u
#define FSH_ANNOUNCE_CONNECTED_MS 3000u

// Receiver re-request timeout and CONNECTED-idle revert (both compiled out under
// carousel — the receiver never transmits). Kept nominal.
#define FSH_RX_TIMEOUT_MS     500u
#define FSH_CONNECTED_IDLE_MS 5000u

// Sender streams unconditionally (no post-RX gap in carousel).
#define FSH_TX_TIMEOUT_MS 0u

// Engine tick period. Pacing comes from the transport's blocking send, not this.
#define FSH_IDLE_TICK_MS 20u

// Receiver never transmits, so there is nothing to desynchronize.
#define FSH_REQUEST_JITTER_MS 0u

// Measured payload throughput (bench): ~315 B/s at RF/32 with the coils held at
// the ~2 cm sweet spot — steady on both a 1 KB and a 32 KB transfer (32 KB in
// 1:44). Used for the ETA estimate.
#define FSH_PAYLOAD_THROUGHPUT_BPS 315u

// No new block for this long -> the receiver GUI shows "stalled". Must exceed one
// carousel cycle for small files (a missed block only comes back next pass).
#define FSH_STALL_MS 15000u

// ===== Carousel / RFID transport internals ===================================

// One ANNOUNCE per this many carousel frames. The receiver can only lock on an
// ANNOUNCE, and on a noisy 2-Flipper coil link the per-frame decode success is
// low, so a rare ANNOUNCE (the original 32) almost never got decoded (observed
// v>0 but a==0). Send one every 4 frames: 25% overhead but a lock within a couple
// of seconds. Once locked the receiver only needs DATA, so this only bounds the
// initial lock latency (and re-lock after a restart).
#define RFID_CAROUSEL_ANNOUNCE_EVERY 4u

// Emulate-DMA half buffer size in (duration, pulse) pairs (~65 ms of waveform).
#define RFID_TP_DMA_HALF_PAIRS 256u

// Tag-side single-slot mailbox: how long rfid_transport_send() blocks while the
// encoder is still draining the previous frame (backpressure). Expires only when
// there is no field to clock the DMA out; the carousel re-sends the frame.
#define RFID_TP_SEND_TIMEOUT_MS 1000u

// Reader-side capture-event stream buffer size, in bytes. Sized well above one
// frame's worth of edges so a burst of comparator noise (~50k edges/s) cannot
// overflow it and drop real frame edges before the worker drains them.
#define RFID_TP_RX_STREAM_SIZE 16384u
