#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "sr_types.h"
#include "sr_bloom.h"
#include "sr_rawlog.h"

/*
 * ★ Session state machine + statistics + ring buffer. Must not include any furi header (ADR-003).
 *
 * Condenses the SrEvent stream into state the UI can draw directly. Pure logic.
 *
 * Hard constraints (ADR-009 / ADR-010):
 *   - Ring buffer elements are SrApBrief; storing SrApRecord is forbidden (232 B x 64 blows the budget).
 *   - Must not retain SrEvent; it only passes through as an sr_model_apply argument.
 *   - SrEvent.u.unknown.text is a borrowed view; retaining it requires memcpy of len bytes plus your own NUL.
 *     sr_strlcpy / strlen are forbidden (only [text, text+len) is guaranteed readable; see the ADR-010 readable-range contract).
 *   - Touches no clock; when a timestamp is needed the caller passes tick_ms in.
 *   - SrBloom is owned by the caller; only a pointer is kept here. Do not embed the 4 KB bit array.
 *   - SrRawLog is owned by the caller; only a pointer is kept here. Do not embed the ring (ADR-018).
 *
 * The SrApBrief layout is settled (task card T2.4; the main session measured 34 B with host clang on 2026-08-16):
 *   ssid[25] + mac[6] + rssi + channel + flags, all members 1-byte aligned, no padding.
 *   flags bit0 = radio (0=WIFI / 1=BLE), bit1 = this entry has a GPS fix. Do not invent a third bit.
 */

enum { SR_RECENT_CAP = 64 };

enum {
    SR_AP_FLAG_BLE = 1u << 0,
    SR_AP_FLAG_GPS = 1u << 1
};

typedef enum {
    SrSessionIdle = 0,
    SrSessionRunning,
    SrSessionStopped
} SrSessionState;

typedef struct {
    char ssid[25];
    uint8_t mac[6];
    int8_t rssi;
    uint8_t channel;
    uint8_t flags;
} SrApBrief;

/* Threshold 40: measured at 34 B, leaving room for one new field; reverting mac to char[18] (46 B) must fail at compile time. */
_Static_assert(sizeof(SrApBrief) <= 40, "SrApBrief over 40 B (T2.4 / ADR-009)");

typedef struct {
    SrSessionState session;
    SrBloom* bloom; /* Borrowed, not owned */
    SrRawLog* rawlog; /* Borrowed, not owned */

    uint32_t ap_wifi;
    uint32_t ap_ble;
    uint32_t gps_blocks;
    uint32_t unknown_lines;
    uint32_t malformed_lines;
    uint32_t with_gps_fix;
    uint32_t unique_est;
    uint32_t illegal_trans;

    /* Tick supplied by the caller; the model never reads a clock itself. */
    uint32_t last_tick_ms;
    uint32_t started_tick_ms;

    SrGpsSnapshot gps; /* The most recent one, stored directly (there is only ever one) */
    SrGpsCsvView gps_csv; /* D12: live GPS copied from each wardrive CSV row; not a gpsdata block */
    uint32_t gps_csv_rev; /* Incremented on every successful AP/BLE row; 0 = never seen */

    SrFirmwareInfo firmware;
    uint32_t firmware_rev; /* Incremented on each SrEventFirmware; 0 = never seen */
    uint32_t session_rev; /* Incremented whenever the session actually transitions; pairs with the ADR-017 start/stop confirmation criteria */
    uint32_t gps_stop_rev; /* Incremented on a GPS/NMEA stop reply while idle; the close-out confirmation signal for sampling (ADR-020) */

    char last_unknown[SR_RAW_LINE_MAX + 1];
    size_t last_unknown_len;

    SrApBrief recent[SR_RECENT_CAP];
    size_t recent_head;  /* Next write position */
    size_t recent_count;
} SrModel;

/* The 4 KB bloom is not in this struct. Exceeding this means somebody embedded the bit array. */
_Static_assert(sizeof(SrModel) <= 4096, "SrModel must not embed SrBloom bits");

void sr_model_init(SrModel* m, SrBloom* bloom, SrRawLog* rawlog);
void sr_model_reset_session(SrModel* m, bool also_reset_bloom);

/*
 * Consume one event. tick_ms is supplied by the caller.
 * Returns true when something UI-visible changed (state / statistics / ring buffer / GPS / last_unknown).
 * An illegal ScanStarted, and a non-GPS/NMEA ScanStopped while idle, only increment
 * illegal_trans, leave session alone, and return false. While idle, SrStopGpsUpdates /
 * SrStopEndNmea increment gps_stop_rev, leave session alone, and return false (ADR-020 sampling close-out).
 */
bool sr_model_apply(SrModel* m, const SrEvent* ev, uint32_t tick_ms);

/* idx 0 = newest. Returns NULL when idx >= count or m is NULL. */
const SrApBrief* sr_model_recent(const SrModel* m, size_t idx);
size_t sr_model_recent_count(const SrModel* m);

/*
 * Accepts only XX:XX:XX:XX:XX:XX (exactly 17 chars + NUL, case insensitive).
 * Returns false on a format mismatch without writing out. Do not use sscanf.
 * Returns false when s or out is NULL.
 */
bool sr_mac_parse(const char* s, uint8_t out[6]);
