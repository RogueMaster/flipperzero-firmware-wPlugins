#pragma once

#include <stddef.h>
#include "observation.h"
#include "scoring.h"

#define SESSION_MAX_ENTRIES 20

typedef struct {
    AccessObservation obs;
    AuditScore score;
} SessionEntry;

typedef struct {
    SessionEntry entries[SESSION_MAX_ENTRIES];
    size_t count;
    char name[13]; /* optional user-entered label, empty string = unnamed */
} ScanSession;

typedef struct {
    size_t high;
    size_t medium;
    size_t low;
    size_t secure;
    CardType most_common_type; /* CardTypeUnknown if count == 0 */
} SessionSummary;

void session_init(ScanSession* session);

/**
 * Append a completed scan to the session.
 * Returns false (and does nothing) if the session is full.
 */
bool session_append(ScanSession* session, const AccessObservation* obs, const AuditScore* score);

/** Compute summary statistics over all entries in the session. */
SessionSummary session_summarise(const ScanSession* session);
