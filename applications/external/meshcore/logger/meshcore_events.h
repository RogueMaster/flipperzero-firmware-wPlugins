/*
 * events.csv rows: adverts heard, messages received, and the operator's own
 * point marks.
 *
 * Columns are meshlog.py's. The `raw` column held a JSON dump there; here it
 * holds `k=v;k=v`, for the same reason as in telemetry.csv — no JSON encoder
 * on the FAP, and no quoting in the CSV writer.
 *
 * Pure formatting, no furi and no I/O.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    MeshCoreEventAdvert,
    MeshCoreEventMessage,
    /* Pressed on the Flipper to say "I am standing here now". The one row type
     * that comes from the operator rather than from the radio. */
    MeshCoreEventMark,
} MeshCoreEventKind;

const char* meshcore_event_kind_name(MeshCoreEventKind kind);

/** ts,type,info,lat,lon,acc,raw — node tags appended by the caller.
 *
 *  `info` is the short human-facing field: a contact name for an advert, the
 *  message text for a message, the mark number for a mark. `raw` carries what
 *  did not fit, and may be empty. Both are sanitised by the caller. */
size_t meshcore_event_format(
    MeshCoreEventKind kind,
    const char* ts,
    const char* info,
    const char* raw,
    const char* lat,
    const char* lon,
    char* out,
    size_t cap);

/** Hex of the first `n` bytes of a public key, the way a contact is identified
 *  in a log when its name is missing or ambiguous. Writes at most `n*2+1`. */
void meshcore_event_key_prefix(const uint8_t* pubkey, size_t n, char* out, size_t cap);

#define MESHCORE_EVENTS_HEADER "ts,type,info,lat,lon,acc,raw,node,role,hw"
