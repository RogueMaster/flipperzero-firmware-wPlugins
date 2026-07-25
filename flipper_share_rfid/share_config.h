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
#define FSH_ANNOUNCE_INTERVAL_MS 1000u
#define FSH_ANNOUNCE_CONNECTED_MS 3000u

// Receiver re-request timeout and CONNECTED-idle revert (both compiled out under
// carousel — the receiver never transmits). Kept nominal.
#define FSH_RX_TIMEOUT_MS 500u
#define FSH_CONNECTED_IDLE_MS 5000u

// Sender streams unconditionally (no post-RX gap in carousel).
#define FSH_TX_TIMEOUT_MS 0u

// Engine tick period. Pacing comes from the transport's blocking send, not this.
#define FSH_IDLE_TICK_MS 20u

// Receiver never transmits, so there is nothing to desynchronize.
#define FSH_REQUEST_JITTER_MS 0u

// Nominal payload throughput for the ETA estimate before a measured rate exists.
// REPLACE with the measured value after the bench.
#define FSH_PAYLOAD_THROUGHPUT_BPS 380u

// No new block for this long -> the receiver GUI shows "stalled". Must exceed one
// carousel cycle for small files (a missed block only comes back next pass).
#define FSH_STALL_MS 15000u

// ===== Carousel / RFID transport internals ===================================

// One ANNOUNCE per this many frames (~5 s lock latency at RF/32).
#define RFID_CAROUSEL_ANNOUNCE_EVERY 32u

// Emulate-DMA half buffer size in (duration, pulse) pairs (~65 ms of waveform).
#define RFID_TP_DMA_HALF_PAIRS 256u

// Tag-side single-slot mailbox: how long rfid_transport_send() blocks while the
// encoder is still draining the previous frame (backpressure). Expires only when
// there is no field to clock the DMA out; the carousel re-sends the frame.
#define RFID_TP_SEND_TIMEOUT_MS 1000u

// Reader-side capture-event stream buffer size, in bytes (headroom for bursts).
#define RFID_TP_RX_STREAM_SIZE 4096u
