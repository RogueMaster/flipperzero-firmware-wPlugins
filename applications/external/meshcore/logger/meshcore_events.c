#include "meshcore_events.h"

#include <stdio.h>

const char* meshcore_event_kind_name(MeshCoreEventKind kind) {
    switch(kind) {
    case MeshCoreEventAdvert:
        return "advert";
    case MeshCoreEventMessage:
        return "msg";
    case MeshCoreEventMark:
        return "mark";
    default:
        return "unknown";
    }
}

size_t meshcore_event_format(
    MeshCoreEventKind kind,
    const char* ts,
    const char* info,
    const char* raw,
    const char* lat,
    const char* lon,
    char* out,
    size_t cap) {
    int written = snprintf(
        out,
        cap,
        "%s,%s,%s,%s,%s,,%s",
        ts,
        meshcore_event_kind_name(kind),
        info ? info : "",
        lat,
        lon,
        raw ? raw : "");

    return (written < 0) ? 0 : (size_t)written;
}

void meshcore_event_key_prefix(const uint8_t* pubkey, size_t n, char* out, size_t cap) {
    static const char digits[] = "0123456789abcdef";
    size_t written = 0;

    for(size_t i = 0; i < n && written + 2 < cap; i++) {
        out[written++] = digits[(pubkey[i] >> 4) & 0x0F];
        out[written++] = digits[pubkey[i] & 0x0F];
    }
    if(cap > 0) out[written] = '\0';
}
