#pragma once

// Per-app tunables for Flipper Share over the iButton 1-Wire pad. This is the
// single place to tweak the transport and the transport-dependent engine knobs;
// changing anything here requires a recompile. The engine (share.c/share.h) and
// the shared scenes read these but define none of them, so the engine files
// stay byte-identical across all new Flipper Share apps.

// ===== Engine-facing tunables (consumed by share.c / the scenes) =============

// Name of this transport, substituted into every UI string ("Send via ...").
#define FSH_TRANSPORT_NAME "iButton"

// File-data bytes per DATA packet. A 73-byte DATA packet is ~45 ms of bus time
// that the sender bit-bangs inside an interrupt/critical section (see README
// section 5.4), so keep this at 64 until the bench confirms the UI, input and
// BT stack stay healthy during a long transfer; 128 is a pure config change.
#define FSH_DATA_LENGTH 64u

// Sender announce cadence: fast while discovering, slower while serving a peer.
#define FSH_ANNOUNCE_INTERVAL_MS  1000u // idle sender discovery interval
#define FSH_ANNOUNCE_CONNECTED_MS 3000u // announce interval while serving a transfer

// Receiver re-request timeout and sender post-RX gap before streaming DATA.
#define FSH_RX_TIMEOUT_MS 500u // receiver REQUEST retry timeout
#define FSH_TX_TIMEOUT_MS 50u // sender gap after last RX before streaming

// Engine tick period (the mailbox backpressure, not this, paces the stream) and
// how long the sender stays CONNECTED after the last RX before re-announcing.
#define FSH_IDLE_TICK_MS      20u
#define FSH_CONNECTED_IDLE_MS 5000u

// Small random backoff before each (re)REQUEST. The host drives the clock, so
// collisions are impossible; this only desynchronizes periodic retry bursts.
#define FSH_REQUEST_JITTER_MS 50u

// Nominal payload throughput used for the ETA estimate before the measured
// session rate is available. REPLACE with the measured value after the bench.
#define FSH_PAYLOAD_THROUGHPUT_BPS 1200u

// No new block for this long -> the receiver GUI shows "stalled".
#define FSH_STALL_MS 5000u

// ===== 1-Wire transport internals (consumed by ibutton_transport.c) ==========

// Bus pin. PB14 = the iButton pad (also header pin 17). If pad-to-pad presence
// detection proves unreliable on the bench, switch to &gpio_ext_pa7 with an
// explicit wire (same code path) and note it in the README.
#define IBTN_TP_GPIO (&gpio_ibutton)

// Custom link-layer command bytes. Deliberately outside every standard 1-Wire
// ROM command (0x33 READ ROM, 0xCC SKIP ROM, 0xF0 SEARCH ROM, ...) so a foreign
// 1-Wire master touching the sender does nothing.
#define IBTN_TP_CMD_POLL 0xA1 // slave -> host: len(1) + len packet bytes (len 0 = nothing queued)
#define IBTN_TP_CMD_PUSH 0xA2 // host -> slave: len(1) + len packet bytes

// Host pacing: gap between transactions, and retry period while no presence.
#define IBTN_TP_POLL_INTERVAL_MS 5u
#define IBTN_TP_RECONNECT_MS     250u

// Outbound DATA mailbox: how long fsh_transport_send() blocks when it is full
// (this backpressure paces the sender's block stream) and the mailbox / RX
// queue depth.
#define IBTN_TP_SEND_TIMEOUT_MS 500u
#define IBTN_TP_QUEUE_DEPTH     4u
