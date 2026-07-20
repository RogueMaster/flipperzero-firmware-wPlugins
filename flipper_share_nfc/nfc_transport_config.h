#pragma once

// NFC transport tunables. This is the single place to tweak the transport;
// changing anything here requires a recompile.

// Identity of the emulated ISO14443-3A card (sender/listener side). Arbitrary
// but stable values; SAK 0x00 (no ISO14443-4) so foreign readers don't try to
// send RATS. The receiver accepts any card and filters by frame content.
#define NFC_TP_UID_LEN 7
#define NFC_TP_UID \
    { 0xF5, 0x0F, 0x5A, 0xAE, 0x00, 0x00, 0x01 }
#define NFC_TP_ATQA \
    { 0x44, 0x00 }
#define NFC_TP_SAK 0x00

// Frame layout inside an ISO14443-3A standard frame (CRC-A handled by the
// stack): [hdr(1)][flipper-share packet(0..NSH_PACKET_MAX)].
#define NFC_TP_HDR_POLL 0x00 // no packet in this direction (keep-alive poll)
#define NFC_TP_HDR_PKT 0x01 // one flipper-share packet follows
#define NFC_TP_HDR_LEN 1u

// The firmware NFC transport buffer is 256 bytes including the 2-byte CRC-A.
#define NFC_TP_FRAME_PAYLOAD_MAX 254u

// Poller frame wait time, in 13.56 MHz carrier cycles (~74 ms): must cover the
// listener's software response latency plus the ~21 ms air time of a full-size
// response frame. Official pollers use 60000 fc for hardware cards; the
// emulated listener answers from a thread whose reply can be tail-delayed by
// logging/notifications/scheduling, so give it ample headroom — a too-short FWT
// turns that jitter into spurious exchange errors and ~100 ms field-reset
// cycles. Only genuinely lost exchanges pay the full wait.
#define NFC_TP_FWT_FC 1000000u

// Pacing delay between poller exchanges, ms (0 = back-to-back). Each exchange
// carries at most one packet each way, so this directly bounds throughput.
#define NFC_TP_POLL_PERIOD_MS 2u

// TX mailbox depth (packets) and how long nfc_transport_send() may block
// waiting for a free slot. In normal operation this is the backpressure that
// paces the sender's block stream (put unblocks within one exchange); the
// timeout only bites on a genuine stall, so it also bounds cancel latency.
#define NFC_TP_QUEUE_LEN 4u
#define NFC_TP_SEND_TIMEOUT_MS 500u
