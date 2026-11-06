#include "line_sender.h"

void line_sender_init(LineSender* ls, size_t packet_size, LineSenderPacketFn send, void* ctx) {
    line_sender_set_packet_size(ls, packet_size);
    ls->cut = false;
    ls->send = send;
    ls->ctx = ctx;
}

TransportSendResult line_sender_send(LineSender* ls, const char* line, size_t len) {
    /* the stream: an optional resync '\n', the line, its '\n' */
    size_t skip = ls->cut ? 0 : 1;
    size_t end = len + 2;
    size_t pos = skip;
    size_t last = 0;

    while(pos < end) {
        size_t n = end - pos < ls->packet_size ? end - pos : ls->packet_size;
        for(size_t i = 0; i < n; i++) {
            size_t at = pos + i;
            ls->packet[i] = at == 0 || at == end - 1 ? '\n' : (uint8_t)line[at - 1];
        }
        LinePacketResult sent = ls->send(ls->packet, n, ls->ctx);
        if(sent != LinePacketSent) {
            /* nothing new on the host */
            if(sent == LinePacketNotSent && pos == skip) return TransportNotSent;
            ls->cut = true;
            size_t held = sent == LinePacketNotSent ? pos : pos + n; /* may be on the host */
            return held >= end - 1 ? TransportMaybeSent : TransportNotSent;
        }
        ls->cut = false;
        pos += n;
        last = n;
    }
    /* a transfer that ends on a full packet needs a zero-length packet */
    if(last == ls->packet_size && ls->send(NULL, 0, ls->ctx) != LinePacketSent) {
        return TransportMaybeSent;
    }
    return TransportSent;
}

void line_sender_set_packet_size(LineSender* ls, size_t packet_size) {
    ls->packet_size = packet_size > 0 && packet_size <= LINE_SENDER_PACKET_MAX ?
                          packet_size :
                          LINE_SENDER_PACKET_MAX;
}
