#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "sr_types.h"

/*
 * ★ Pure-logic adapter. Must not include any furi header.
 *
 * This header defines only the codec shape and the command-buffer contract; it implements no codec.
 * The feed_line and command words for Marauder / GhostESP / Native must wait for a real T0.4
 * dump (or a corresponding VERIFY ✅) before being written; filling them in from memory is forbidden.
 *
 * build_start_cmd / build_stop_cmd:
 *   Write the command into buf and return the bytes written (excluding the trailing NUL).
 *   cap is the total capacity of buf and must fit the command bytes plus the trailing NUL.
 *   On insufficient cap (cap < n+1) it returns 0 and modifies no byte of buf.
 */

typedef struct SrSourceCodec {
    const char* name; /* "marauder" / "ghostesp" / "native" */
    size_t (*build_start_cmd)(const SrScanCfg* cfg, char* buf, size_t cap);
    size_t (*build_stop_cmd)(char* buf, size_t cap);
    bool (*probe_line)(const char* line, SrFirmwareInfo* out);
    SrParseResult (*feed_line)(SrParser* parser, const char* line, size_t len, SrEvent* out);
    /* Command to fetch one frame of GPS status (ADR-020). Codecs that do not support it leave
     * this NULL, and callers must check for NULL first. */
    size_t (*build_gps_cmd)(char* buf, size_t cap);
    /* Command to tag a GPS POI during wardrive (card D15-POI). Codecs that do not support it leave
     * this NULL, and callers must check for NULL first. */
    size_t (*build_poi_cmd)(char* buf, size_t cap);
} SrSourceCodec;

/* Shared implementation of the command-buffer contract, for future codecs and host_test. Not a codec itself. */
static inline size_t sr_cmd_write(char* buf, size_t cap, const char* src) {
    size_t n = 0;
    if(src == NULL) {
        return 0;
    }
    while(src[n] != '\0') {
        n++;
    }
    /*
     * The capacity check is written n > cap - 1U rather than cap < n + 1U: the latter wraps
     * n + 1U to 0 when n == SIZE_MAX, defeating the guard and passing SIZE_MAX to memcpy.
     * cap == 0 must be rejected first,
     * or cap - 1U wraps on its own. gcc's -Wstringop-overflow flagged this path on an inlined
     * instance with cap=0 (on the first Linux CI run).
     */
    if(buf == NULL || cap == 0U || n > cap - 1U) {
        return 0;
    }
    memcpy(buf, src, n);
    buf[n] = '\0';
    return n;
}
