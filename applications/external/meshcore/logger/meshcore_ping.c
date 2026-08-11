#include "meshcore_ping.h"

#include <stdio.h>
#include <string.h>

void meshcore_ping_init(MeshCorePing* ping) {
    memset(ping, 0, sizeof(*ping));
}

bool meshcore_ping_parse_ack(const uint8_t* payload, size_t len, uint32_t* ack_code) {
    if(payload == NULL || len < 5) return false;
    if(payload[0] != MESHCORE_PING_ACK_CODE) return false;

    if(ack_code != NULL) {
        /* Little endian, matching the tag SENT handed out. */
        *ack_code = (uint32_t)payload[1] | ((uint32_t)payload[2] << 8) |
                    ((uint32_t)payload[3] << 16) | ((uint32_t)payload[4] << 24);
    }
    return true;
}

static MeshCorePingTarget* meshcore_ping_find(MeshCorePing* ping, const char* name) {
    for(size_t i = 0; i < ping->count; i++) {
        if(strncmp(ping->targets[i].name, name, MESHCORE_PING_NAME_LEN) == 0) {
            return &ping->targets[i];
        }
    }
    return NULL;
}

bool meshcore_ping_add(MeshCorePing* ping, const char* name) {
    if(name == NULL || name[0] == '\0') return false;
    if(ping->count >= MESHCORE_PING_MAX_TARGETS) return false;
    if(meshcore_ping_find(ping, name) != NULL) return false;

    MeshCorePingTarget* target = &ping->targets[ping->count];
    memset(target, 0, sizeof(*target));
    snprintf(target->name, sizeof(target->name), "%.*s", (int)(MESHCORE_PING_NAME_LEN - 1), name);
    ping->count++;
    return true;
}

bool meshcore_ping_resolve(MeshCorePing* ping, const char* name, const uint8_t pubkey[32]) {
    MeshCorePingTarget* target = meshcore_ping_find(ping, name);
    if(target == NULL) return false;

    memcpy(target->pubkey, pubkey, 32);
    target->known = true;
    return true;
}

MeshCorePingTarget* meshcore_ping_next(MeshCorePing* ping) {
    if(ping->count == 0) return NULL;

    MeshCorePingTarget* target = &ping->targets[ping->cursor % ping->count];
    ping->cursor = (ping->cursor + 1) % ping->count;
    return target;
}

void meshcore_ping_started(
    MeshCorePing* ping,
    size_t index,
    uint32_t seq,
    uint32_t expected_ack,
    uint32_t now_ms) {
    ping->in_flight = true;
    ping->flight_index = index;
    ping->flight_seq = seq;
    ping->expected_ack = expected_ack;
    ping->sent_ms = now_ms;
}

bool meshcore_ping_confirm(
    MeshCorePing* ping,
    uint32_t ack_code,
    uint32_t now_ms,
    uint32_t* rtt_ms) {
    if(!ping->in_flight) return false;
    if(ack_code != ping->expected_ack) return false;

    /* Unsigned subtraction, so a tick counter that wrapped between send and
     * ack still yields the elapsed time rather than four billion milliseconds. */
    if(rtt_ms != NULL) *rtt_ms = now_ms - ping->sent_ms;
    ping->in_flight = false;
    return true;
}

bool meshcore_ping_timeout(MeshCorePing* ping) {
    if(!ping->in_flight) return false;
    ping->in_flight = false;
    return true;
}

size_t meshcore_ping_format(
    const char* ts,
    const char* target,
    uint32_t seq,
    bool ok,
    uint32_t rtt_ms,
    const char* lat,
    const char* lon,
    char* out,
    size_t cap) {
    char rtt[12] = "";
    if(ok) snprintf(rtt, sizeof(rtt), "%lu", (unsigned long)rtt_ms);

    int written = snprintf(
        out,
        cap,
        "%s,%s,%lu,%d,%s,%s,%s,",
        ts,
        target,
        (unsigned long)seq,
        ok ? 1 : 0,
        rtt,
        lat,
        lon);

    return (written < 0) ? 0 : (size_t)written;
}
