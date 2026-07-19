#pragma once

#include <furi.h>
#include "ble_signatures.h"

#define TRACKER_DB_MAX 48
/* A record must be seen at least this many times before it can be flagged as
 * "following" - filters out one-off passers-by. */
#define TRACKER_DB_MIN_DETECTIONS 4
#define TRACKER_NAME_LEN 20

typedef struct {
    uint8_t mac[6];
    TrackerType type;
    int8_t rssi; // most recent
    int8_t rssi_max; // strongest seen
    uint32_t first_seen; // furi tick (ms)
    uint32_t last_seen; // furi tick (ms)
    uint16_t count; // detections
    bool following; // flagged as travelling with you
    bool alerted; // user already notified
    char name[TRACKER_NAME_LEN];
} TrackerRecord;

typedef struct TrackerDb TrackerDb;

TrackerDb* tracker_db_alloc(void);
void tracker_db_free(TrackerDb* db);

/** Clear all detections (e.g. when starting a fresh hunt). */
void tracker_db_reset(TrackerDb* db);

/**
 * Insert or refresh a detection.
 * @return true if THIS update newly promoted the record to "following"
 *         (a fresh alert should fire). Thread-safe.
 */
bool tracker_db_update(
    TrackerDb* db,
    const uint8_t mac[6],
    TrackerType type,
    int8_t rssi,
    const char* name,
    uint32_t follow_threshold_ms);

size_t tracker_db_count(TrackerDb* db);
size_t tracker_db_following_count(TrackerDb* db);

/**
 * Copy a UI snapshot sorted by threat priority (followers first, then by
 * strongest RSSI). Thread-safe.
 * @return number of records written (<= max).
 */
size_t tracker_db_snapshot(TrackerDb* db, TrackerRecord* out, size_t max);

/**
 * Fetch the record that most recently became a follower, marking it as
 * acknowledged. Used by the alert flow. @return false if none pending.
 */
bool tracker_db_take_pending_alert(TrackerDb* db, TrackerRecord* out);
