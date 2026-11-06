/* Sends lines as packets (USB CDC bulk, BLE notifications), as a pure state
 * machine.
 *
 * A line and its '\n' go out in packets of up to `packet_size` bytes; a
 * transfer that ends on a full packet gets a zero-length packet (a packet
 * function without such packets takes it as sent). The packet
 * callback reports whether the packet reached the host, did not go out (the
 * host is not reading), or was handed over but not confirmed; the rest of
 * the line is then not sent.
 *
 * A line cut short leaves its start on the host. The next line therefore
 * begins with a '\n', which ends the fragment there: the receiver drops it
 * as invalid JSON (a strict prefix of a JSON object never parses) or, when
 * only the '\n' was missing, takes the whole line late. Empty lines are
 * ignored (PROTOCOL.md, Framing). Not thread-safe.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "transport.h"

#define LINE_SENDER_PACKET_MAX 243 /* a USB CDC packet (64) or a BLE notification */

typedef enum {
    LinePacketSent, /* the host took it */
    LinePacketNotSent, /* it did not go out */
    LinePacketMaybeSent, /* it went out but the host did not take it yet */
} LinePacketResult;

typedef LinePacketResult (*LineSenderPacketFn)(const uint8_t* data, size_t len, void* ctx);

typedef struct {
    size_t packet_size;
    bool cut; /* the host may hold the start of an unfinished line */
    LineSenderPacketFn send;
    void* ctx;
    uint8_t packet[LINE_SENDER_PACKET_MAX];
} LineSender;

/* `packet_size` is 1..LINE_SENDER_PACKET_MAX. */
void line_sender_init(LineSender* ls, size_t packet_size, LineSenderPacketFn send, void* ctx);

/* For the next line (BLE chunks grow after the MTU exchange); out of range
 * sizes become LINE_SENDER_PACKET_MAX. */
void line_sender_set_packet_size(LineSender* ls, size_t packet_size);

/* Sends `line` (without its '\n') and the '\n'. TransportNotSent when the
 * host cannot get it, TransportMaybeSent when the host may hold all of it
 * but the '\n' (which the next line's resync supplies) or when the end of
 * it or the zero-length packet was not confirmed. */
TransportSendResult line_sender_send(LineSender* ls, const char* line, size_t len);
