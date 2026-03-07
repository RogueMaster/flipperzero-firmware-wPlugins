#pragma once

#include "rogue_uart.h"
#include <furi.h>

/* =========================================================================
 * Detection constants
 * ========================================================================= */

#define ROGUE_MAX_APS   128
#define ROGUE_SSID_LEN  33
#define ROGUE_BSSID_LEN 18

/* AP entry stale-out: remove APs not seen for 30 seconds. */
#define ROGUE_STALE_MS 30000

/* RSSI delta that upgrades SUSPECT → EVIL_TWIN.
 * A rogue AP broadcasting at notably higher power is a strong indicator. */
#define ROGUE_EVIL_TWIN_RSSI_DELTA 20

/* =========================================================================
 * Data types
 * ========================================================================= */

typedef struct {
    char ssid[ROGUE_SSID_LEN];
    char bssid[ROGUE_BSSID_LEN];
    int8_t rssi;
    uint8_t channel;
    uint32_t last_seen_tick; /* furi_get_tick() at time of last observation */
} RogueApEntry;

typedef enum {
    RogueStatusClean,
    RogueStatusSuspect, /* Same SSID appearing from 2+ distinct BSSIDs */
    RogueStatusEvilTwin, /* Suspect + significant RSSI anomaly (>20 dBm) */
} RogueStatus;

typedef struct {
    RogueApEntry aps[ROGUE_MAX_APS];
    uint32_t ap_count;
    FuriMutex* mutex;
    RogueStatus overall_status;
    /* Details of the most suspicious SSID: */
    char flagged_ssid[ROGUE_SSID_LEN];
    uint32_t flagged_bssid_count;
} RogueApResults;

/* =========================================================================
 * Worker API
 * ========================================================================= */

typedef struct RogueApWorker RogueApWorker;

/* Allocate the worker. `results` must outlive the worker. */
RogueApWorker* rogue_ap_worker_alloc(RogueUart* uart, RogueApResults* results);

/* Free the worker. Does not free `results`. */
void rogue_ap_worker_free(RogueApWorker* worker);

/* Start continuous AP scanning (sends `scanap\n` to Marauder). */
void rogue_ap_worker_start(RogueApWorker* worker);

/* Stop scanning (sends `stopscan\n` and clears the UART callback). */
void rogue_ap_worker_stop(RogueApWorker* worker);

/* Returns true if scanning is currently active. */
bool rogue_ap_worker_is_scanning(RogueApWorker* worker);
