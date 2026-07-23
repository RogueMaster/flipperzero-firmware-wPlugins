#include "tracker_db.h"
#include <stdlib.h>
#include <string.h>

struct TrackerDb {
    FuriMutex* mutex;
    TrackerRecord records[TRACKER_DB_MAX];
    size_t count;
    size_t following_count;
    int32_t pending_alert; // index of a newly-flagged follower, or -1
};

TrackerDb* tracker_db_alloc(void) {
    TrackerDb* db = malloc(sizeof(TrackerDb));
    db->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    db->count = 0;
    db->following_count = 0;
    db->pending_alert = -1;
    return db;
}

void tracker_db_free(TrackerDb* db) {
    furi_assert(db);
    furi_mutex_free(db->mutex);
    free(db);
}

void tracker_db_reset(TrackerDb* db) {
    furi_assert(db);
    furi_mutex_acquire(db->mutex, FuriWaitForever);
    db->count = 0;
    db->following_count = 0;
    db->pending_alert = -1;
    furi_mutex_release(db->mutex);
}

static int32_t tracker_db_find(TrackerDb* db, const uint8_t mac[6]) {
    for(size_t i = 0; i < db->count; i++) {
        if(memcmp(db->records[i].mac, mac, 6) == 0) return (int32_t)i;
    }
    return -1;
}

// Evict the least-interesting record (never a follower, oldest last_seen).
static size_t tracker_db_evict_index(TrackerDb* db) {
    size_t victim = 0;
    uint32_t oldest = UINT32_MAX;
    bool found = false;
    for(size_t i = 0; i < db->count; i++) {
        if(db->records[i].following) continue;
        if(db->records[i].last_seen < oldest) {
            oldest = db->records[i].last_seen;
            victim = i;
            found = true;
        }
    }
    // Everything is a follower (unlikely) - drop the oldest regardless.
    if(!found) {
        oldest = UINT32_MAX;
        for(size_t i = 0; i < db->count; i++) {
            if(db->records[i].last_seen < oldest) {
                oldest = db->records[i].last_seen;
                victim = i;
            }
        }
    }
    return victim;
}

bool tracker_db_update(
    TrackerDb* db,
    const uint8_t mac[6],
    TrackerType type,
    int8_t rssi,
    const char* name,
    uint32_t follow_threshold_ms) {
    furi_assert(db);
    bool new_follower = false;
    uint32_t now = furi_get_tick();

    furi_mutex_acquire(db->mutex, FuriWaitForever);

    int32_t idx = tracker_db_find(db, mac);
    if(idx < 0) {
        if(db->count >= TRACKER_DB_MAX) {
            idx = (int32_t)tracker_db_evict_index(db);
            if(db->records[idx].following && db->following_count) db->following_count--;
        } else {
            idx = (int32_t)db->count++;
        }
        TrackerRecord* r = &db->records[idx];
        memset(r, 0, sizeof(TrackerRecord));
        memcpy(r->mac, mac, 6);
        r->type = type;
        r->rssi = rssi;
        r->rssi_max = rssi;
        r->first_seen = now;
        r->last_seen = now;
        r->count = 1;
        if(name && name[0]) {
            strncpy(r->name, name, TRACKER_NAME_LEN - 1);
            r->name[TRACKER_NAME_LEN - 1] = '\0';
        }
    } else {
        TrackerRecord* r = &db->records[idx];
        r->last_seen = now;
        r->rssi = rssi;
        if(rssi > r->rssi_max) r->rssi_max = rssi;
        if(r->count < UINT16_MAX) r->count++;
        // Trust a more specific classification if we learn one later.
        if(r->type == TrackerTypeUnknown && type != TrackerTypeUnknown) r->type = type;
        if((!r->name[0]) && name && name[0]) {
            strncpy(r->name, name, TRACKER_NAME_LEN - 1);
            r->name[TRACKER_NAME_LEN - 1] = '\0';
        }

        // "Following you" heuristic: a known tracker type that has stayed with
        // you for the threshold window across enough detections.
        if(!r->following && tracker_type_is_threat(r->type) &&
           r->count >= TRACKER_DB_MIN_DETECTIONS &&
           (r->last_seen - r->first_seen) >= follow_threshold_ms) {
            r->following = true;
            db->following_count++;
            db->pending_alert = idx;
            new_follower = true;
        }
    }

    furi_mutex_release(db->mutex);
    return new_follower;
}

size_t tracker_db_count(TrackerDb* db) {
    furi_assert(db);
    furi_mutex_acquire(db->mutex, FuriWaitForever);
    size_t c = db->count;
    furi_mutex_release(db->mutex);
    return c;
}

size_t tracker_db_following_count(TrackerDb* db) {
    furi_assert(db);
    furi_mutex_acquire(db->mutex, FuriWaitForever);
    size_t c = db->following_count;
    furi_mutex_release(db->mutex);
    return c;
}

// Ordering: followers first, then strongest RSSI.
static bool tracker_record_before(const TrackerRecord* a, const TrackerRecord* b) {
    if(a->following != b->following) return a->following;
    return a->rssi > b->rssi;
}

size_t tracker_db_snapshot(TrackerDb* db, TrackerRecord* out, size_t max) {
    furi_assert(db);
    furi_mutex_acquire(db->mutex, FuriWaitForever);
    size_t n = db->count < max ? db->count : max;
    memcpy(out, db->records, n * sizeof(TrackerRecord));
    furi_mutex_release(db->mutex);

    // Insertion sort (n is small; firmware API has no qsort).
    for(size_t i = 1; i < n; i++) {
        TrackerRecord key = out[i];
        size_t j = i;
        while(j > 0 && tracker_record_before(&key, &out[j - 1])) {
            out[j] = out[j - 1];
            j--;
        }
        out[j] = key;
    }
    return n;
}

bool tracker_db_take_pending_alert(TrackerDb* db, TrackerRecord* out) {
    furi_assert(db);
    bool ok = false;
    furi_mutex_acquire(db->mutex, FuriWaitForever);
    if(db->pending_alert >= 0 && db->pending_alert < (int32_t)db->count) {
        *out = db->records[db->pending_alert];
        db->records[db->pending_alert].alerted = true;
        db->pending_alert = -1;
        ok = true;
    }
    furi_mutex_release(db->mutex);
    return ok;
}
