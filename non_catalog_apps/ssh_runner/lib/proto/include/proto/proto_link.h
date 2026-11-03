/*
 * SSH Runner UART protocol — reliability layer (docs/PROTOCOL.md §2).
 *
 * Stop-and-wait per direction:
 *   - every frame with bit 7 clear (request or event) is "reliable": the peer must
 *     answer it with exactly one reply frame (bit 7 set) carrying the same seq;
 *   - the sender keeps at most one reliable frame outstanding and retransmits it on
 *     timeout, up to `max_retries` times;
 *   - the receiver drops duplicates (same seq as the last reliable frame it accepted)
 *     and re-sends its cached reply instead of processing them again.
 *
 * No allocation, no threads. The owner calls proto_link_rx() for received bytes and
 * proto_link_poll() periodically; time is passed in explicitly (milliseconds).
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#pragma once

#include "proto.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PROTO_LINK_TIMEOUT_MS  200u
#define PROTO_LINK_MAX_RETRIES 3u

typedef struct ProtoLink ProtoLink;

typedef struct {
    /* Write raw bytes to the wire. */
    void (*write)(void* ctx, const uint8_t* data, size_t len);
    /* A new reliable frame arrived (request/event). The owner must call
     * proto_link_reply() for it, now or later, before the peer gives up. */
    void (*on_frame)(void* ctx, const ProtoFrame* frame);
    /* The reply to our outstanding frame arrived. */
    void (*on_reply)(void* ctx, const ProtoFrame* reply, uint8_t request_type);
    /* Our outstanding frame got no reply after all retries. */
    void (*on_timeout)(void* ctx, uint8_t request_type, uint16_t seq);
    void* ctx;
} ProtoLinkCallbacks;

typedef struct {
    uint32_t tx_frames;
    uint32_t retransmits;
    uint32_t timeouts;
    uint32_t duplicates;
    uint32_t stale_replies;
} ProtoLinkStats;

struct ProtoLink {
    ProtoLinkCallbacks cb;
    uint32_t timeout_ms;
    uint8_t max_retries;

    /* TX side */
    uint16_t next_seq;
    bool outstanding;
    uint8_t out_type;
    uint16_t out_seq;
    uint8_t retries;
    uint32_t sent_at;
    size_t out_len;
    uint8_t out_wire[PROTO_MAX_WIRE];

    /* RX side */
    bool have_rx_seq;
    uint16_t rx_seq;
    bool reply_pending; /* accepted a frame, owner has not replied yet */
    size_t reply_len;
    uint8_t reply_wire[PROTO_MAX_WIRE];

    ProtoDecoder dec;
    ProtoLinkStats stats;
};

void proto_link_init(ProtoLink* link, const ProtoLinkCallbacks* cb);

/* Forget peer state (after a baud change or a HELLO from a restarted peer). */
void proto_link_reset_peer(ProtoLink* link);

bool proto_link_busy(const ProtoLink* link);

/* Send a reliable frame. Returns false if one is already outstanding or the
 * payload is too large. *seq_out (optional) receives the seq used. */
bool proto_link_send(
    ProtoLink* link,
    uint8_t type,
    const uint8_t* payload,
    uint16_t len,
    uint32_t now_ms,
    uint16_t* seq_out);

/* Answer the last accepted reliable frame. `type` must have PROTO_REPLY_BIT set. */
bool proto_link_reply(ProtoLink* link, uint8_t type, const uint8_t* payload, uint16_t len);
bool proto_link_ack(ProtoLink* link);

/* Feed received bytes. */
void proto_link_rx(ProtoLink* link, const uint8_t* data, size_t len, uint32_t now_ms);

/* Drive retransmissions. Call at least every timeout_ms / 4. */
void proto_link_poll(ProtoLink* link, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
