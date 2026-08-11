/*
 * ping.csv rows, and the bookkeeping behind them.
 *
 * A ping is not a protocol feature: it is a text message to a contact, and the
 * round trip is the time until the node pushes SEND_CONFIRMED carrying the ack
 * tag that SENT promised. meshlog.py measures it the same way, which is why
 * the numbers from a phone and from a Flipper can sit in one table.
 *
 * The state here is deliberately single-slot. One outstanding ping at a time
 * keeps the measurement honest: two in flight share the air, and each would be
 * timing the other's transmission as well as its own.
 *
 * Pure bookkeeping, no furi and no I/O — testable on a laptop.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Four is what fits the field kit: a base and up to three repeaters. */
#define MESHCORE_PING_MAX_TARGETS 4u
#define MESHCORE_PING_NAME_LEN 24u

typedef struct {
    char name[MESHCORE_PING_NAME_LEN];
    uint8_t pubkey[32];
    bool known; /* seen in the contact list; false until its advert arrives */
    uint32_t seq;
} MeshCorePingTarget;

typedef struct {
    MeshCorePingTarget targets[MESHCORE_PING_MAX_TARGETS];
    size_t count;
    size_t cursor; /* round-robin position */

    bool in_flight;
    size_t flight_index;
    uint32_t flight_seq;
    uint32_t expected_ack;
    uint32_t sent_ms;
} MeshCorePing;

void meshcore_ping_init(MeshCorePing* ping);

/** Read the ack tag out of a SEND_CONFIRMED push, from the raw payload.
 *
 *  Ours rather than meshcore_c's on purpose. That library requires eight bytes
 *  — an ack code and a round trip — and drops the frame otherwise, while the
 *  reference Python client accepts four and reads only the code:
 *
 *      if len(data) >= 5:              # the 0x82 byte plus four
 *          ack_data["code"] = dbuf.read(4).hex()
 *
 *  A node that sends the short form would therefore have every ack silently
 *  discarded, and every ping recorded as a miss. So four is the minimum here
 *  and anything after it is ignored.
 *
 *  `payload` includes the 0x82 code byte. False if it is not one, or too short. */
bool meshcore_ping_parse_ack(const uint8_t* payload, size_t len, uint32_t* ack_code);

/* SEND_CONFIRMED. Not named in meshcore_c's push enum in a form this file can
 * use without dragging the whole header in. */
#define MESHCORE_PING_ACK_CODE 0x82u

/** Add a target by name. Ignored if the table is full or the name is already
 *  there. Returns false if it did not get added. */
bool meshcore_ping_add(MeshCorePing* ping, const char* name);

/** Attach a public key to a named target, which is what makes it pingable.
 *  Called as contacts arrive. False if the name is not a target. */
bool meshcore_ping_resolve(MeshCorePing* ping, const char* name, const uint8_t pubkey[32]);

/** Next target to ping, advancing the cursor. NULL if there are none. */
MeshCorePingTarget* meshcore_ping_next(MeshCorePing* ping);

/** Record that a ping went out and is now awaiting its ack. */
void meshcore_ping_started(
    MeshCorePing* ping,
    size_t index,
    uint32_t seq,
    uint32_t expected_ack,
    uint32_t now_ms);

/** Match an incoming SEND_CONFIRMED. True when it belongs to the outstanding
 *  ping, in which case `rtt_ms` is filled and the slot is freed. */
bool meshcore_ping_confirm(MeshCorePing* ping, uint32_t ack_code, uint32_t now_ms, uint32_t* rtt_ms);

/** Give up on the outstanding ping. True if there was one. */
bool meshcore_ping_timeout(MeshCorePing* ping);

/** ts,target,seq,ok,rtt_ms,lat,lon,acc — node tags appended by the caller.
 *  A miss writes an empty rtt_ms rather than zero: zero is a round trip that
 *  happened impossibly fast, not one that never happened. */
size_t meshcore_ping_format(
    const char* ts,
    const char* target,
    uint32_t seq,
    bool ok,
    uint32_t rtt_ms,
    const char* lat,
    const char* lon,
    char* out,
    size_t cap);

#define MESHCORE_PING_HEADER "ts,target,seq,ok,rtt_ms,lat,lon,acc,node,role,hw"
