/* SPDX-License-Identifier: GPL-3.0-only */
#include "proto/proto_link.h"
#include "proto/proto_msg.h"

#include <string.h>

void proto_link_init(ProtoLink* link, const ProtoLinkCallbacks* cb) {
    memset(link, 0, sizeof(*link));
    link->cb = *cb;
    link->timeout_ms = PROTO_LINK_TIMEOUT_MS;
    link->max_retries = PROTO_LINK_MAX_RETRIES;
    link->next_seq = 1;
    proto_decoder_init(&link->dec);
}

void proto_link_reset_peer(ProtoLink* link) {
    link->have_rx_seq = false;
    link->reply_pending = false;
    link->reply_len = 0;
    proto_decoder_init(&link->dec);
}

bool proto_link_busy(const ProtoLink* link) {
    return link->outstanding;
}

bool proto_link_send(
    ProtoLink* link,
    uint8_t type,
    const uint8_t* payload,
    uint16_t len,
    uint32_t now_ms,
    uint16_t* seq_out) {
    if(link->outstanding || PROTO_IS_REPLY(type)) return false;
    uint16_t seq = link->next_seq++;
    size_t n = proto_frame_encode(type, seq, payload, len, link->out_wire, sizeof(link->out_wire));
    if(n == 0) return false;
    link->out_len = n;
    link->out_type = type;
    link->out_seq = seq;
    link->retries = 0;
    link->sent_at = now_ms;
    link->outstanding = true;
    link->stats.tx_frames++;
    if(seq_out) *seq_out = seq;
    link->cb.write(link->cb.ctx, link->out_wire, n);
    return true;
}

bool proto_link_reply(ProtoLink* link, uint8_t type, const uint8_t* payload, uint16_t len) {
    if(!link->reply_pending || !PROTO_IS_REPLY(type)) return false;
    size_t n = proto_frame_encode(
        type, link->rx_seq, payload, len, link->reply_wire, sizeof(link->reply_wire));
    if(n == 0) return false;
    link->reply_len = n;
    link->reply_pending = false;
    link->cb.write(link->cb.ctx, link->reply_wire, n);
    return true;
}

bool proto_link_ack(ProtoLink* link) {
    return proto_link_reply(link, PROTO_T_ACK, NULL, 0);
}

static void handle_frame(ProtoLink* link, const ProtoFrame* f) {
    if(PROTO_IS_REPLY(f->type)) {
        if(link->outstanding && f->seq == link->out_seq) {
            link->outstanding = false;
            uint8_t req_type = link->out_type;
            if(link->cb.on_reply) link->cb.on_reply(link->cb.ctx, f, req_type);
        } else {
            link->stats.stale_replies++;
        }
        return;
    }

    /* A restarted peer starts over with HELLO; never treat it as a duplicate. */
    if(f->type == PROTO_T_HELLO) link->have_rx_seq = false;

    if(link->have_rx_seq && f->seq == link->rx_seq) {
        link->stats.duplicates++;
        /* Re-send the cached reply; if the owner has not answered yet, stay quiet. */
        if(!link->reply_pending && link->reply_len)
            link->cb.write(link->cb.ctx, link->reply_wire, link->reply_len);
        return;
    }
    link->have_rx_seq = true;
    link->rx_seq = f->seq;
    link->reply_pending = true;
    link->reply_len = 0;
    if(link->cb.on_frame) link->cb.on_frame(link->cb.ctx, f);
}

void proto_link_rx(ProtoLink* link, const uint8_t* data, size_t len, uint32_t now_ms) {
    (void)now_ms;
    for(size_t i = 0; i < len; i++) {
        ProtoFrame f;
        if(proto_decoder_push(&link->dec, data[i], &f) == ProtoDecFrame) handle_frame(link, &f);
    }
}

void proto_link_poll(ProtoLink* link, uint32_t now_ms) {
    if(!link->outstanding) return;
    if((uint32_t)(now_ms - link->sent_at) < link->timeout_ms) return;
    if(link->retries >= link->max_retries) {
        link->outstanding = false;
        link->stats.timeouts++;
        if(link->cb.on_timeout) link->cb.on_timeout(link->cb.ctx, link->out_type, link->out_seq);
        return;
    }
    link->retries++;
    link->sent_at = now_ms;
    link->stats.retransmits++;
    link->cb.write(link->cb.ctx, link->out_wire, link->out_len);
}
