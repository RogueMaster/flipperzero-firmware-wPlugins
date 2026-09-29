#pragma once

#include <furi.h>
#include "ble_signatures.h"

#define TRACKER_DB_MAX 48

/* A record must be seen at least this many times before it can be flagged as
 * following - filters out a one-off passer-by caught twice. */
#define TRACKER_DB_MIN_DETECTIONS 4

/* No sighting for this long and the tracker is treated as gone. The radar only
 * draws what is around you NOW; the detections list keeps the history and
 * marks it. Without this, something that walked past half an hour ago sat on
 * the dial for the rest of the session looking exactly like a live contact. */
#define TRACKER_STALE_MS 30000UL

#define TRACKER_NAME_LEN 20

typedef struct {
    uint8_t mac[6];
    TrackerType type;
    int8_t rssi; /* most recent */
    int8_t rssi_max; /* strongest ever seen - how close it has actually been */
    uint32_t first_seen; /* furi tick (ms) */
    uint32_t last_seen; /* furi tick (ms) */
    uint16_t count; /* sightings */
    bool following; /* has travelled with you past the dwell window */
    bool alerted; /* the user has been shown this one */
    char name[TRACKER_NAME_LEN];
} TrackerRecord;

typedef struct TrackerDb TrackerDb;

TrackerDb* tracker_db_alloc(void);
void tracker_db_free(TrackerDb* db);

/** Clear all detections (a fresh hunt). */
void tracker_db_reset(TrackerDb* db);

/**
 * Insert or refresh a detection.
 * @return true if THIS update newly promoted the record to "following".
 * Thread-safe; called from a radio worker thread.
 */
bool tracker_db_update(
    TrackerDb* db,
    const uint8_t mac[6],
    TrackerType type,
    int8_t rssi,
    const char* name,
    uint32_t follow_threshold_ms);

/** Every record this session, gone or not. */
size_t tracker_db_count(TrackerDb* db);

/** Records seen within TRACKER_STALE_MS. */
size_t tracker_db_present_count(TrackerDb* db);

/** Present records whose type could realistically be used to stalk someone. */
size_t tracker_db_threat_count(TrackerDb* db);

size_t tracker_db_following_count(TrackerDb* db);

/**
 * Copy a UI snapshot sorted by threat priority (followers, then present
 * trackers, then strongest signal). Thread-safe.
 * @return number of records written (<= max).
 */
size_t tracker_db_snapshot(TrackerDb* db, TrackerRecord* out, size_t max);

/**
 * Fetch the record that most recently became a follower, clearing the pending
 * flag. @return false if none pending.
 *
 * The pending alert is held as an ADDRESS, not an index: eviction can move or
 * overwrite the slot a stored index pointed at, which would have made the
 * alert describe a completely different device.
 */
bool tracker_db_take_pending_alert(TrackerDb* db, TrackerRecord* out);

/** True if this record has not been heard from recently. */
bool tracker_record_is_stale(const TrackerRecord* rec, uint32_t now_tick);
