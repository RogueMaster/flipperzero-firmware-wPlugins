#include "sessionstats.h"

#include <stdio.h>

bool session_stats_silent(const SessionStats* s) {
    furi_assert(s);
    return s->rx_bytes == 0 && s->errors == 0;
}

uint32_t session_stats_bps(const SessionStats* s) {
    furi_assert(s);
    if(s->duration_ms == 0) return 0;
    /* Round to nearest: (bytes * 1000 + dur/2) / dur. The +dur/2 is the round,
     * and the 64-bit product keeps a fast link from overflowing at speed. */
    return (uint32_t)(((uint64_t)s->rx_bytes * 1000u + s->duration_ms / 2u) / s->duration_ms);
}

uint32_t session_stats_error_permille(const SessionStats* s) {
    furi_assert(s);
    const uint32_t total = s->rx_bytes + s->errors;
    if(total == 0) return 0;
    return (uint32_t)(((uint64_t)s->errors * 1000u) / total);
}

void session_stats_format_duration(const SessionStats* s, char* out, size_t out_len) {
    furi_assert(s);
    furi_assert(out);

    const uint32_t total_s = s->duration_ms / 1000u;
    const uint32_t mins = total_s / 60u;
    const uint32_t secs = total_s % 60u;

    /* Cast to unsigned long so %lu is right on both the ARM target (uint32_t is
     * unsigned long there) and the host test build (where it is unsigned int). */
    if(mins > 0) {
        snprintf(out, out_len, "%lum %lus", (unsigned long)mins, (unsigned long)secs);
    } else {
        snprintf(out, out_len, "%lus", (unsigned long)secs);
    }
}

void session_stats_format_bytes(uint32_t bytes, char* out, size_t out_len) {
    furi_assert(out);

    if(bytes < 1024u) {
        snprintf(out, out_len, "%lu B", (unsigned long)bytes);
    } else {
        /* One decimal of KB, without floating point: tenths = bytes*10/1024. */
        const uint32_t tenths = (uint32_t)(((uint64_t)bytes * 10u) / 1024u);
        snprintf(
            out,
            out_len,
            "%lu.%lu KB",
            (unsigned long)(tenths / 10u),
            (unsigned long)(tenths % 10u));
    }
}

const char* session_stats_verdict(const SessionStats* s) {
    furi_assert(s);
    if(session_stats_silent(s)) return "no traffic";

    const uint32_t permille = session_stats_error_permille(s);
    if(permille == 0) return "clean link";
    if(permille < 10u) return "minor errors"; // <1%
    if(permille < 100u) return "noisy"; // <10%
    return "wrong framing?"; // errors rival the data - the guess is off
}
