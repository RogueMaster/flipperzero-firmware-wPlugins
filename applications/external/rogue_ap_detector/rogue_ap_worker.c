/**
 * rogue_ap_worker.c — AP scan parsing and evil-twin detection logic.
 *
 * Parsing
 * ~~~~~~~
 * Marauder `scanap` output format (one AP per line):
 *   <rssi> <bssid> <channel> <ssid>
 * e.g.: -65 AA:BB:CC:DD:EE:FF 6 MyNetwork
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
static void rogue_uart_line_cb(const char* line, void* ctx) {
    RogueApWorker* worker = (RogueApWorker*)ctx;
    RogueApResults* r = worker->results;

    /* Skip Marauder status/prefix lines that start with '['. */
    if(line[0] == '[') return;

    /* Parse: rssi bssid channel ssid
   * ssid may contain spaces, so read up to the first three tokens then
   * take everything remaining as the SSID. */
    int rssi_raw = 0;
    char bssid[ROGUE_BSSID_LEN];
    int channel = 0;
    char ssid[ROGUE_SSID_LEN];

    /* sscanf into a large temp to capture multi-word SSIDs. */
    char ssid_raw[ROGUE_SSID_LEN + 8]; /* slight oversize for safety */
    int parsed = sscanf(line, "%d %17s %d %32[^\n]", &rssi_raw, bssid, &channel, ssid_raw);

    if(parsed < 3) return; /* not enough fields */

    /* Validate BSSID — guards against misparse of status lines. */
    if(!is_valid_bssid(bssid)) return;

    /* Channel sanity (1-14 WiFi channels). */
    if(channel < 1 || channel > 14) return;

    /* RSSI sanity (typical range: -100 to -10 dBm). */
    if(rssi_raw < -110 || rssi_raw > 0) return;

    /* Copy SSID — use empty string if field was missing. */
    if(parsed >= 4) {
        strncpy(ssid, ssid_raw, ROGUE_SSID_LEN - 1);
        ssid[ROGUE_SSID_LEN - 1] = '\0';
    } else {
        ssid[0] = '\0';
    }

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
