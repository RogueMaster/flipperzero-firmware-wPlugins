#pragma once

#include <furi.h>
#include "baud_table.h"

/* A snapshot of one console session, taken when the link closes. Pure data plus
 * a few pure formatters, so the arithmetic can be tested on the host without a
 * Flipper in the loop. */
typedef struct {
    uint32_t baud;
    HermesFraming framing;
    uint32_t rx_bytes;
    uint32_t errors;
    uint32_t trigger_hits;
    uint32_t duration_ms;
    bool tx_used; // did the user actually send anything
    bool logged; // was a file written
    char log_name[48]; // filename, empty when not logged
} SessionStats;

/** Whole session had zero traffic - drives a different, honest summary. */
bool session_stats_silent(const SessionStats* s);

/** Average throughput in bytes per second over the whole session.
 *
 * Integer, rounded to nearest, guarded against a zero duration. This is where
 * an off-by-one in the ms->s scaling would hide, so it has its own test.
 */
uint32_t session_stats_bps(const SessionStats* s);

/** Error rate in tenths of a percent (0..1000), of bytes+errors. */
uint32_t session_stats_error_permille(const SessionStats* s);

/** "1m 23s" / "45s" into `out`. */
void session_stats_format_duration(const SessionStats* s, char* out, size_t out_len);

/** "1.2 KB" / "834 B" into `out`, a human byte count. */
void session_stats_format_bytes(uint32_t bytes, char* out, size_t out_len);

/** One-word health read of the link, from the error rate. */
const char* session_stats_verdict(const SessionStats* s);
