/**
 * rogue_ap_worker.c — AP scan parsing and evil-twin detection logic.
 *
 * Parsing
 * ~~~~~~~
 * Marauder `scanap` output format (one AP per line):
 *   <idx> <SSID> <RSSI> <Channel> <BSSID> <Encryption>
 * e.g.: 0 MyNetwork -45 6 AA:BB:CC:DD:EE:FF WPA2
 *
 * Also handles `list -a` bracket format:
 *   [idx] SSID (rssi) ch:X [ENC] BSSID
 *
 * Lines that do not match this format are silently skipped.  Marauder also
 * emits [wifi/] prefix lines and other noise — the parser handles these
 * defensively by validating each field before accepting the record.
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
    bool scanning;
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
        int8_t delta = (int8_t)(max_rssi - min_rssi);
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
 *
 * Expected line format:  <rssi> <bssid> <channel> <ssid>
 * Marauder also prefixes some lines with "[wifi/]" — skip those.
 * ========================================================================= */
/* Try to parse a Marauder scanap line in streaming format:
 *   <idx> <SSID> <RSSI> <Channel> <BSSID> <Encryption>
 *   e.g.: 0 MyNetwork -45 6 AA:BB:CC:DD:EE:FF WPA2
 *
 * Or the 'list -a' bracket format:
 *   [idx] SSID (rssi) ch:X [ENC] BSSID
 *   e.g.: [0] MyNetwork (-45) ch:6 [WPA2] AA:BB:CC:DD:EE:FF
 *
 * Extracts ssid, bssid, rssi, channel into the provided buffers.
 * Returns true on success.
 */
static bool rogue_parse_marauder_ap(
    const char* line,
    char* ssid,
    char* bssid,
    int* rssi_out,
    int* channel_out) {
    /* Try bracket format first: [idx] SSID (rssi) ch:X [ENC] BSSID */
    if(line[0] == '[') {
        const char* idx_end = strchr(line, ']');
        if(!idx_end) return false;
        const char* p = idx_end + 1;
        while(*p == ' ')
            p++;
        if(!*p) return false;

        /* SSID: up to '(' */
        const char* paren = strchr(p, '(');
        if(!paren) return false;
        size_t ssid_len = (size_t)(paren - p);
        while(ssid_len > 0 && p[ssid_len - 1] == ' ')
            ssid_len--;
        if(ssid_len >= ROGUE_SSID_LEN) ssid_len = ROGUE_SSID_LEN - 1;
        memcpy(ssid, p, ssid_len);
        ssid[ssid_len] = '\0';

        /* RSSI inside parens */
        *rssi_out = atoi(paren + 1);
        const char* close = strchr(paren, ')');
        if(!close) return false;
        p = close + 1;
        while(*p == ' ')
            p++;

        /* Channel: "ch:X" */
        if(strncmp(p, "ch:", 3) == 0 || strncmp(p, "Ch:", 3) == 0) {
            *channel_out = atoi(p + 3);
            p += 3;
            while(*p >= '0' && *p <= '9')
                p++;
            while(*p == ' ')
                p++;
        }

        /* Skip encryption in brackets */
        if(*p == '[') {
            const char* enc_end = strchr(p, ']');
            if(enc_end) {
                p = enc_end + 1;
                while(*p == ' ')
                    p++;
            }
        }

        /* Remaining is BSSID */
        if(*p) {
            strncpy(bssid, p, ROGUE_BSSID_LEN - 1);
            bssid[ROGUE_BSSID_LEN - 1] = '\0';
            /* Trim trailing whitespace */
            size_t blen = strlen(bssid);
            while(blen > 0 &&
                  (bssid[blen - 1] == ' ' || bssid[blen - 1] == '\r' || bssid[blen - 1] == '\n')) {
                bssid[--blen] = '\0';
            }
        }
        return is_valid_bssid(bssid) && ssid[0] != '\0';
    }

    /* Streaming format: <idx> <SSID> <RSSI> <Channel> <BSSID> <Encryption>
     * Parse from the right side where tokens have fixed format. */
    if(line[0] < '0' || line[0] > '9') return false;

    /* Skip index */
    const char* p = line;
    while(*p >= '0' && *p <= '9')
        p++;
    while(*p == ' ')
        p++;
    if(!*p) return false;

    /* Find BSSID and encryption from the right end */
    const char* end = line + strlen(line);

    /* Trim trailing whitespace */
    while(end > p && (end[-1] == ' ' || end[-1] == '\r' || end[-1] == '\n'))
        end--;

    /* Last token = encryption */
    const char* enc_start = end;
    while(enc_start > p && enc_start[-1] != ' ')
        enc_start--;

    /* Second-to-last = BSSID */
    const char* bssid_end = enc_start;
    while(bssid_end > p && bssid_end[-1] == ' ')
        bssid_end--;
    const char* bssid_start = bssid_end;
    while(bssid_start > p && bssid_start[-1] != ' ')
        bssid_start--;

    size_t blen = (size_t)(bssid_end - bssid_start);
    if(blen >= ROGUE_BSSID_LEN) blen = ROGUE_BSSID_LEN - 1;
    memcpy(bssid, bssid_start, blen);
    bssid[blen] = '\0';
    if(!is_valid_bssid(bssid)) return false;

    /* Third-from-last = channel */
    const char* ch_end = bssid_start;
    while(ch_end > p && ch_end[-1] == ' ')
        ch_end--;
    const char* ch_start = ch_end;
    while(ch_start > p && ch_start[-1] != ' ')
        ch_start--;
    *channel_out = atoi(ch_start);

    /* Fourth-from-last = RSSI */
    const char* rssi_end = ch_start;
    while(rssi_end > p && rssi_end[-1] == ' ')
        rssi_end--;
    const char* rssi_start = rssi_end;
    while(rssi_start > p && rssi_start[-1] != ' ')
        rssi_start--;
    *rssi_out = atoi(rssi_start);

    /* Everything between p and rssi_start is the SSID */
    const char* ssid_end = rssi_start;
    while(ssid_end > p && ssid_end[-1] == ' ')
        ssid_end--;
    size_t slen = (size_t)(ssid_end - p);
    if(slen >= ROGUE_SSID_LEN) slen = ROGUE_SSID_LEN - 1;
    if(slen == 0) return false;
    memcpy(ssid, p, slen);
    ssid[slen] = '\0';

    return true;
}

static void rogue_uart_line_cb(const char* line, void* ctx) {
    RogueApWorker* worker = (RogueApWorker*)ctx;
    RogueApResults* r = worker->results;

    /* Skip Marauder status lines */
    if(line[0] == '\0') return;
    if(strncmp(line, "Scanning", 8) == 0 || strncmp(line, "Stopping", 8) == 0 ||
       strncmp(line, ">", 1) == 0 || strstr(line, "Started") || strstr(line, "Done") ||
       strstr(line, "[APs]") || strstr(line, "Scan complete"))
        return;

    int rssi_raw = 0;
    char bssid[ROGUE_BSSID_LEN];
    int channel = 0;
    char ssid[ROGUE_SSID_LEN];

    if(!rogue_parse_marauder_ap(line, ssid, bssid, &rssi_raw, &channel)) return;

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
