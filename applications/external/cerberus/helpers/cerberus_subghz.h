/**
 * Cerberus - Sub-GHz radio worker.
 *
 * Runs a background thread that drives the internal CC1101: it tunes the band(s)
 * being watched, samples RSSI, feeds the per-band detectors, publishes a
 * snapshot for the UI to draw, and posts a custom event to the ViewDispatcher
 * whenever a fresh threat is raised.
 */
#pragma once

#include <furi.h>
#include <gui/view_dispatcher.h>
#include "cerberus_detector.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CERBERUS_BAND_COUNT 3

/**
 * Alert events are posted to the ViewDispatcher by the worker thread and
 * intercepted at the app level (so an alert fires no matter which screen is
 * open). The threat and band are packed into the event id.
 */
#define CERBERUS_ALERT_EVENT_BASE 0x1000U
#define CERBERUS_ALERT_EVENT(threat, band) \
    (CERBERUS_ALERT_EVENT_BASE | ((uint32_t)(threat) << 4) | ((uint32_t)(band) & 0xF))
#define CERBERUS_ALERT_EVENT_THREAT(ev) ((CerberusThreat)((((ev) - CERBERUS_ALERT_EVENT_BASE) >> 4) & 0xF))
#define CERBERUS_ALERT_EVENT_BAND(ev)   ((uint8_t)(((ev) - CERBERUS_ALERT_EVENT_BASE) & 0xF))

/** One watched band. The three heads of Cerberus. */
typedef struct {
    uint32_t frequency; // Hz, handed to the radio HAL
    uint16_t mhz; // for display / logging (433, 868, 915)
    const char* label; // short label drawn in the UI ("433")
} CerberusBand;

extern const CerberusBand cerberus_bands[CERBERUS_BAND_COUNT];

/** Scan strategy. Hop watches all three; the pinned modes camp one band. */
typedef enum {
    CerberusScanModeHop = 0,
    CerberusScanModeBand0, // pin 433
    CerberusScanModeBand1, // pin 868
    CerberusScanModeBand2, // pin 915
} CerberusScanMode;

/** Snapshot of a single band for the UI. Plain integers, no floats/pointers. */
typedef struct {
    int16_t rssi; // last measured RSSI (dBm)
    int16_t floor; // learned noise floor (dBm)
    int16_t peak; // peak hold (dBm)
    int16_t threshold; // current "busy" line = floor + margin (dBm)
    uint16_t bursts; // bursts in the last second
    CerberusThreat threat; // currently displayed threat
} CerberusBandStatus;

/** Atomic snapshot copied out for the monitor view to render. */
typedef struct {
    CerberusBandStatus band[CERBERUS_BAND_COUNT];
    uint8_t active_band; // band being sampled right now
    bool hop; // true if hopping
    bool running; // worker active
    uint32_t uptime_s; // seconds since the watch started
} CerberusSnapshot;

/** Mirror of the user config the worker needs (kept tiny on purpose). */
typedef struct {
    CerberusScanMode scan_mode;
    uint8_t sensitivity; // 0 low / 1 medium / 2 high
    bool detect_jam;
    bool detect_flood;
    bool detect_replay;
} CerberusWorkerConfig;

typedef struct CerberusSubGhz CerberusSubGhz;

CerberusSubGhz* cerberus_subghz_alloc(ViewDispatcher* view_dispatcher);
void cerberus_subghz_free(CerberusSubGhz* worker);

/** Update the live config. Safe to call while running (applied next sample). */
void cerberus_subghz_set_config(CerberusSubGhz* worker, const CerberusWorkerConfig* config);

void cerberus_subghz_start(CerberusSubGhz* worker);
void cerberus_subghz_stop(CerberusSubGhz* worker);
bool cerberus_subghz_is_running(CerberusSubGhz* worker);

/** Drop the learned noise floor on every band and start learning again. */
void cerberus_subghz_reset_floor(CerberusSubGhz* worker);

/** Copy the latest snapshot for drawing. */
void cerberus_subghz_get_snapshot(CerberusSubGhz* worker, CerberusSnapshot* out);

#ifdef __cplusplus
}
#endif
