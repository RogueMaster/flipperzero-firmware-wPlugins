/**
 * Persisted per-device counters for the Desktop (NUS) mode.
 *
 * These feed the {"ack":"status","data":{"stats":{...}}} response the
 * Claude desktop app polls every ~2s, and drive the celebrate / heart
 * transient states in nus_state.c.
 *
 * File layout at /ext/apps_data/claude_buddy/stats.bin:
 *   byte 0:     version (1)
 *   bytes 1..:  packed NusStats struct (little-endian, matches native ARM)
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t approvals;       /* total permission approvals */
    uint32_t denies;          /* total permission denials */
    uint32_t approvals_fast;  /* subset approved within 5 s of prompt appearing */
    uint32_t tokens_seen;     /* max tokens value observed in any heartbeat */
    uint32_t level;           /* floor(tokens_seen / 50000) — celebrate on increment */
    uint32_t session_starts;  /* times total transitioned 0 → >0 */
} NusStats;

/* Load stats from disk (zeros on first run or read error). */
void nus_stats_load(NusStats* out);

/* Write current stats to disk.  Best-effort — logs on failure but does
 * not propagate it to the caller. */
void nus_stats_save(const NusStats* in);

#ifdef __cplusplus
}
#endif
