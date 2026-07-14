/**
 * rogue_ap_worker.c — AP scan parsing and evil-twin detection logic.
 *
 * Parsing
 * ~~~~~~~
 * Marauder `scanap` streaming output format (one AP per line):
 *   <RSSI> Ch: <channel> <BSSID> ESSID: <SSID> <beacon_byte1> <beacon_byte2>
 * e.g.: -67 Ch: 6 AA:BB:CC:DD:EE:FF ESSID: MyNetwork 80 00
 *
 * Also handles `list -a` bracket format:
 *   [<idx>][CH:<channel>] <SSID> <RSSI>
 * e.g.: [0][CH:6] MyNetwork -67
 * Note: list-a lines carry no BSSID — they are skipped because evil-twin
 * detection requires distinct MAC addresses to be meaningful.
 *
 * Lines that do not match either format are silently skipped.
 *
 * Detection
 * ~~~~~~~~~
 * After each parsed line the SSID table is scanned for duplicates:
 *   - Same SSID, 2+ distinct BSSIDs → SUSPECT
 *   - SUSPECT + one BSSID has RSSI >= (other + ROGUE_EVIL_TWIN_RSSI_DELTA) →
 * EVIL_TWIN
 *
 * All table access is mutex-protected so the GUI timer thread can safely
 * read RogueApResults at any time.
 */

#include "rogue_ap_worker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "RogueWorker"

/* =========================================================================
 * Internal struct
 * ========================================================================= */

struct RogueApWorker {
    RogueUart* uart;
    RogueApResults* results;
    volatile bool scanning;
};

/* =========================================================================
 * Parsing helpers
 * ========================================================================= */

/* Validate a BSSID string of the form XX:XX:XX:XX:XX:XX.
 * Returns true on valid format. */
static bool is_valid_bssid(const char* s) {
    /* Expected length: 17 characters */
    if(!s || strlen(s) != 17) return false;
    for(int i = 0; i < 17; i++) {
        if(i % 3 == 2) {
            if(s[i] != ':') return false;
        } else {
            char c = s[i];
            bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
            if(!hex) return false;
        }
    }
    return true;
}

/* =========================================================================
 * Detection logic — called with results->mutex already held.
 *
 * Scans the AP table for SSIDs with multiple distinct BSSIDs.
 * Updates results->overall_status, flagged_ssid, flagged_bssid_count.
 * ========================================================================= */
static void rogue_detect(RogueApResults* r) {
    RogueStatus worst = RogueStatusClean;
    char worst_ssid[ROGUE_SSID_LEN] = {0};
    uint32_t worst_count = 0;

    for(uint32_t i = 0; i < r->ap_count; i++) {
        const char* ssid = r->aps[i].ssid;

        /* Count distinct BSSIDs for this SSID and track min/max RSSI. */
        uint32_t bssid_count = 0;
        int8_t max_rssi = -127;
        int8_t min_rssi = 0; /* RSSI values are negative; 0 is "not set" */
        bool min_set = false;

        for(uint32_t j = 0; j < r->ap_count; j++) {
            if(strcmp(r->aps[j].ssid, ssid) != 0) continue;
            bssid_count++;

            if(r->aps[j].rssi > max_rssi) max_rssi = r->aps[j].rssi;
            if(!min_set || r->aps[j].rssi < min_rssi) {
                min_rssi = r->aps[j].rssi;
                min_set = true;
            }
        }

        if(bssid_count < 2) continue;

        /* This SSID is suspicious — classify it. */
        RogueStatus s = RogueStatusSuspect;
        int delta = (int)max_rssi - (int)min_rssi;
        if(delta >= ROGUE_EVIL_TWIN_RSSI_DELTA) {
            s = RogueStatusEvilTwin;
        }

        /* Track the worst-case SSID across all suspicious entries. */
        if(s > worst || (s == worst && bssid_count > worst_count)) {
            worst = s;
            strncpy(worst_ssid, ssid, ROGUE_SSID_LEN - 1);
            worst_ssid[ROGUE_SSID_LEN - 1] = '\0';
            worst_count = bssid_count;
        }
    }

    r->overall_status = worst;
    strncpy(r->flagged_ssid, worst_ssid, ROGUE_SSID_LEN - 1);
    r->flagged_ssid[ROGUE_SSID_LEN - 1] = '\0';
    r->flagged_bssid_count = worst_count;
}

/* =========================================================================
 * Stale AP pruning — called with results->mutex held.
 *
 * Removes entries not seen within ROGUE_STALE_MS.  Uses a compact-shift
 * approach to avoid holes.
 * ========================================================================= */
static void rogue_prune_stale(RogueApResults* r) {
    uint32_t now = furi_get_tick();
    uint32_t stale_ticks = furi_ms_to_ticks(ROGUE_STALE_MS);
    uint32_t write = 0;

    for(uint32_t read = 0; read < r->ap_count; read++) {
        if((now - r->aps[read].last_seen_tick) <= stale_ticks) {
            if(write != read) {
                r->aps[write] = r->aps[read];
            }
            write++;
        }
    }
    r->ap_count = write;
}

/* =========================================================================
 * UART RX callback — fired on the UART worker thread for each line.
 *
 * Parses one Marauder AP line and upserts into the results table.
 * Must not block; mutex acquisition uses a short timeout.
 * ========================================================================= */

/* Parse a Marauder scanap streaming line:
 *   -67 Ch: 6 AA:BB:CC:DD:EE:FF ESSID: MyNetwork 80 00
 *
 * Field order: RSSI, "Ch: " channel, BSSID (17 chars), "ESSID: " SSID,
 * then optional trailing beacon bytes (decimal integers) which are discarded.
 *
 * Also recognises the `list -a` bracket format:
 *   [0][CH:6] MyNetwork -67
 * These lines carry no BSSID — evil-twin detection requires a MAC, so they
 * are accepted for display/channel info but returned false (skipped) here.
 *
 * Returns true and fills ssid/bssid/rssi_out/channel_out on success.
 */
static bool rogue_parse_marauder_ap(
    const char* line,
    char* ssid,
    char* bssid,
    int* rssi_out,
    int* channel_out) {
    const char* p = line;

    /* ------------------------------------------------------------------
     * Branch: list -a bracket format — [idx][CH:n] SSID rssi
     * No BSSID present; skip so detection logic only sees scanap lines.
     * ------------------------------------------------------------------ */
    if(p[0] == '[') {
        /* Consume [idx] */
        const char* bracket_end = strchr(p, ']');
        if(!bracket_end) return false;
        p = bracket_end + 1;

        /* Consume [CH:n] */
        if(*p != '[') return false;
        p++; /* skip '[' */
        if(strncmp(p, "CH:", 3) != 0 && strncmp(p, "ch:", 3) != 0) return false;
        p += 3;
        *channel_out = atoi(p);
        while(*p >= '0' && *p <= '9')
            p++;
        if(*p != ']') return false;
        p++; /* skip ']' */

        /* Skip leading space */
        while(*p == ' ')
            p++;
        if(!*p) return false;

        /* SSID runs to the last space (RSSI is the last token) */
        const char* end = p + strlen(p);
        while(end > p && (end[-1] == '\r' || end[-1] == '\n' || end[-1] == ' '))
            end--;

        /* Walk back one token to find the RSSI */
        const char* rssi_tok = end;
        while(rssi_tok > p && rssi_tok[-1] != ' ')
            rssi_tok--;
        if(rssi_tok == p) return false; /* no RSSI token */

        *rssi_out = atoi(rssi_tok);

        /* SSID is everything before the trailing space+RSSI */
        const char* ssid_end = rssi_tok;
        while(ssid_end > p && ssid_end[-1] == ' ')
            ssid_end--;
        size_t slen = (size_t)(ssid_end - p);
        if(slen == 0 || slen >= ROGUE_SSID_LEN) return false;
        memcpy(ssid, p, slen);
        ssid[slen] = '\0';

        /* No BSSID available — skip this line for detection purposes. */
        (void)bssid;
        return false;
    }

    /* ------------------------------------------------------------------
     * Branch: scanap streaming format
     *   -67 Ch: 6 AA:BB:CC:DD:EE:FF ESSID: MyNetwork 80 00
     * Line starts with RSSI: optional '-' followed by digits.
     * ------------------------------------------------------------------ */
    if(p[0] != '-' && (p[0] < '0' || p[0] > '9')) return false;

    /* 1. Parse RSSI */
    *rssi_out = atoi(p);
    if(*p == '-') p++;
    while(*p >= '0' && *p <= '9')
        p++;
    while(*p == ' ')
        p++;

    /* 2. Parse "Ch: <n>" */
    if(strncmp(p, "Ch: ", 4) != 0 && strncmp(p, "ch: ", 4) != 0) return false;
    p += 4;
    *channel_out = atoi(p);
    while(*p >= '0' && *p <= '9')
        p++;
    while(*p == ' ')
        p++;

    /* 3. Parse BSSID — must be exactly 17 chars matching XX:XX:XX:XX:XX:XX */
    if(strlen(p) < 17) return false;
    memcpy(bssid, p, 17);
    bssid[17] = '\0';
    if(!is_valid_bssid(bssid)) return false;
    p += 17;
    while(*p == ' ')
        p++;

    /* 4. Parse "ESSID: <ssid>" — trim trailing beacon bytes */
    if(strncmp(p, "ESSID: ", 7) != 0) return false;
    p += 7;

    /* SSID ends at the first trailing token that looks like a decimal byte
     * value (1-3 digits at end of string, separated by spaces).  Walk the
     * end of the string backwards, stripping decimal-only tokens. */
    const char* ssid_start = p;
    const char* end = p + strlen(p);
    while(end > ssid_start && (end[-1] == '\r' || end[-1] == '\n' || end[-1] == ' '))
        end--;

    /* Strip trailing beacon bytes: tokens of 1-3 decimal digits */
    while(end > ssid_start) {
        const char* tok = end;
        while(tok > ssid_start && tok[-1] != ' ')
            tok--;
        /* Check if this token is all digits and at most 3 chars (0-255) */
        const char* t = tok;
        bool all_digits = (end - tok) >= 1 && (end - tok) <= 3;
        while(t < end && all_digits) {
            if(*t < '0' || *t > '9') all_digits = false;
            t++;
        }
        if(!all_digits) break;
        /* Strip the token and preceding space */
        end = tok;
        while(end > ssid_start && end[-1] == ' ')
            end--;
    }

    size_t slen = (size_t)(end - ssid_start);
    if(slen == 0 || slen >= ROGUE_SSID_LEN) return false;
    memcpy(ssid, ssid_start, slen);
    ssid[slen] = '\0';

    return true;
}

static void rogue_uart_line_cb(const char* line, void* ctx) {
    if(!ctx) return; /* Guard against race during callback teardown */
    RogueApWorker* worker = (RogueApWorker*)ctx;
    RogueApResults* r = worker->results;

    /* Skip Marauder status lines */
    if(line[0] == '\0') return;
    if(strncmp(line, "Scanning", 8) == 0 || strncmp(line, "Starting", 8) == 0 ||
       strncmp(line, "Stopping", 8) == 0 || strncmp(line, ">", 1) == 0 ||
       strstr(line, "Started") || strstr(line, "Done") || strstr(line, "[APs]") ||
       strstr(line, "Scan complete") || strstr(line, "Stop with"))
        return;

    /* Log every non-status line for debugging parser issues */
    FURI_LOG_I(TAG, "RX line: %.80s", line);

    int rssi_raw = 0;
    char bssid[ROGUE_BSSID_LEN];
    int channel = 0;
    char ssid[ROGUE_SSID_LEN];

    if(!rogue_parse_marauder_ap(line, ssid, bssid, &rssi_raw, &channel)) {
        FURI_LOG_W(TAG, "Parse failed for: %.80s", line);
        return;
    }

    FURI_LOG_I(TAG, "Parsed AP: ssid=%s bssid=%s rssi=%d ch=%d", ssid, bssid, rssi_raw, channel);

    /* Channel sanity (1-14 WiFi channels). */
    if(channel < 1 || channel > 14) return;

    /* RSSI sanity (typical range: -100 to -10 dBm). */
    if(rssi_raw < -110 || rssi_raw > 0) return;

    uint32_t now = furi_get_tick();

    /* Acquire mutex with short timeout — drop the line if we can't get it
   * rather than blocking the UART worker. */
    if(furi_mutex_acquire(r->mutex, furi_ms_to_ticks(10)) != FuriStatusOk) {
        return;
    }

    /* Prune stale entries periodically (every parse call has low overhead). */
    rogue_prune_stale(r);

    /* Apply user-configured RSSI filter. */
    if(rssi_raw < (int)r->min_rssi) {
        furi_mutex_release(r->mutex);
        return;
    }

    /* Upsert: update existing entry if (ssid, bssid) matches. */
    bool found = false;
    for(uint32_t i = 0; i < r->ap_count; i++) {
        if(strcmp(r->aps[i].bssid, bssid) == 0 && strcmp(r->aps[i].ssid, ssid) == 0) {
            r->aps[i].rssi = (int8_t)rssi_raw;
            r->aps[i].channel = (uint8_t)channel;
            r->aps[i].last_seen_tick = now;
            found = true;
            break;
        }
    }

    /* Insert new entry if table has room. */
    if(!found && r->ap_count < ROGUE_MAX_APS) {
        RogueApEntry* e = &r->aps[r->ap_count];
        strncpy(e->ssid, ssid, ROGUE_SSID_LEN - 1);
        e->ssid[ROGUE_SSID_LEN - 1] = '\0';
        strncpy(e->bssid, bssid, ROGUE_BSSID_LEN - 1);
        e->bssid[ROGUE_BSSID_LEN - 1] = '\0';
        e->rssi = (int8_t)rssi_raw;
        e->channel = (uint8_t)channel;
        e->last_seen_tick = now;
        r->ap_count++;
    }

    /* Re-evaluate threat level after every update. */
    rogue_detect(r);

    furi_mutex_release(r->mutex);
}

/* =========================================================================
 * Public API
 * ========================================================================= */

RogueApWorker* rogue_ap_worker_alloc(RogueUart* uart, RogueApResults* results) {
    furi_assert(uart);
    furi_assert(results);

    RogueApWorker* worker = malloc(sizeof(RogueApWorker));
    furi_assert(worker);

    worker->uart = uart;
    worker->results = results;
    worker->scanning = false;

    return worker;
}

void rogue_ap_worker_free(RogueApWorker* worker) {
    furi_assert(worker);

    if(worker->scanning) {
        rogue_ap_worker_stop(worker);
    }

    free(worker);
}

void rogue_ap_worker_start(RogueApWorker* worker) {
    furi_assert(worker);

    if(worker->scanning) return;

    rogue_uart_set_rx_callback(worker->uart, rogue_uart_line_cb, worker);
    rogue_uart_send(worker->uart, "scanap");
    worker->scanning = true;

    FURI_LOG_I(TAG, "AP scan started");
}

void rogue_ap_worker_stop(RogueApWorker* worker) {
    furi_assert(worker);

    if(!worker->scanning) return;

    rogue_uart_send(worker->uart, "stopscan");
    /* Clear the callback so no further lines are processed. */
    rogue_uart_set_rx_callback(worker->uart, NULL, NULL);
    worker->scanning = false;

    FURI_LOG_I(TAG, "AP scan stopped");
}

bool rogue_ap_worker_is_scanning(RogueApWorker* worker) {
    furi_assert(worker);
    return worker->scanning;
}
