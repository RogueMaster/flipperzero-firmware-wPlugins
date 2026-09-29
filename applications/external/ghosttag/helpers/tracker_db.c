#include "tracker_db.h"
#include <stdlib.h>
#include <string.h>

struct TrackerDb {
    FuriMutex* mutex;
    TrackerRecord records[TRACKER_DB_MAX];
    size_t count;
    size_t following_count;
};

bool tracker_record_is_stale(const TrackerRecord* rec, uint32_t now_tick) {
    return (uint32_t)(now_tick - rec->last_seen) > TRACKER_STALE_MS;
}

TrackerDb* tracker_db_alloc(void) {
    TrackerDb* db = malloc(sizeof(TrackerDb));
    memset(db, 0, sizeof(TrackerDb));
    db->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
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
    /* Wipe the records, not just the counter. Leaving stale bytes behind means
     * a later partially-filled record can inherit a previous session's name,
     * peak or timestamps. */
    memset(db->records, 0, sizeof(db->records));
    db->count = 0;
    db->following_count = 0;
    furi_mutex_release(db->mutex);
}

static int32_t tracker_db_find(TrackerDb* db, const uint8_t mac[6]) {
    for(size_t i = 0; i < db->count; i++) {
        if(memcmp(db->records[i].mac, mac, 6) == 0) return (int32_t)i;
    }
    return -1;
}

/* Evict the least interesting record: never a follower, oldest last_seen. */
static size_t tracker_db_evict_index(TrackerDb* db) {
    size_t victim = 0;
    uint32_t oldest = UINT32_MAX;
    bool found = false;
    for(size_t i = 0; i < db->count; i++) {
        if(db->records[i].following) continue;
        if(db->records[i].last_seen <= oldest) {
            oldest = db->records[i].last_seen;
            victim = i;
            found = true;
        }
    }
    if(!found) {
        oldest = UINT32_MAX;
        for(size_t i = 0; i < db->count; i++) {
            if(db->records[i].last_seen <= oldest) {
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
            TrackerRecord* victim = &db->records[idx];
            if(victim->following && db->following_count) db->following_count--;
            /* An un-taken alert dies with the record it belonged to, because
             * the queue IS the records - there is no separate slot left
             * pointing at a device that no longer exists. */
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

        /* A tracker that vanished and came back much later is a NEW encounter,
         * not a continuation of the old one. Without this, two sightings ten
         * minutes apart with nothing in between satisfy any dwell window up to
         * ten minutes - the single worst false positive this app can produce,
         * because it tells somebody they are being followed when a tag merely
         * passed them twice. */
        if(tracker_record_is_stale(r, now)) {
            if(r->following && db->following_count) db->following_count--;
            r->following = false;
            r->alerted = false;
            r->first_seen = now;
            r->count = 0;
        }

        r->last_seen = now;
        r->rssi = rssi;
        if(rssi > r->rssi_max) r->rssi_max = rssi;
        if(r->count < UINT16_MAX) r->count++;
        if(r->type == TrackerTypeUnknown && type != TrackerTypeUnknown) r->type = type;
        if((!r->name[0]) && name && name[0]) {
            strncpy(r->name, name, TRACKER_NAME_LEN - 1);
            r->name[TRACKER_NAME_LEN - 1] = '\0';
        }

        /* Promoted only by a known tracker type that has stayed in range for
         * the whole dwell window AND been heard from enough times across it. */
        if(!r->following && tracker_type_is_threat(r->type) &&
           r->count >= TRACKER_DB_MIN_DETECTIONS &&
           (uint32_t)(r->last_seen - r->first_seen) >= follow_threshold_ms) {
            r->following = true;
            db->following_count++;
            new_follower = true; /* r->alerted stays false: that IS the queue */
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

size_t tracker_db_present_count(TrackerDb* db) {
    furi_assert(db);
    uint32_t now = furi_get_tick();
    furi_mutex_acquire(db->mutex, FuriWaitForever);
    size_t c = 0;
    for(size_t i = 0; i < db->count; i++) {
        if(!tracker_record_is_stale(&db->records[i], now)) c++;
    }
    furi_mutex_release(db->mutex);
    return c;
}

size_t tracker_db_threat_count(TrackerDb* db) {
    furi_assert(db);
    uint32_t now = furi_get_tick();
    furi_mutex_acquire(db->mutex, FuriWaitForever);
    size_t c = 0;
    for(size_t i = 0; i < db->count; i++) {
        if(tracker_record_is_stale(&db->records[i], now)) continue;
        if(tracker_type_is_threat(db->records[i].type)) c++;
    }
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

/*
 * Ordering: followers, then present-before-gone, then strongest signal, then
 * address as a final tie-break.
 *
 * The address tie-break is what stops the list flickering. Two records on the
 * same RSSI have no defined order without it, so an insertion sort can swap
 * them on one refresh and swap them back on the next - and the highlight rides
 * along with whatever lands under it.
 */
static bool tracker_record_before(const TrackerRecord* a, const TrackerRecord* b, uint32_t now) {
    if(a->following != b->following) return a->following;
    bool sa = tracker_record_is_stale(a, now);
    bool sb = tracker_record_is_stale(b, now);
    if(sa != sb) return sb;
    if(a->rssi != b->rssi) return a->rssi > b->rssi;
    return memcmp(a->mac, b->mac, 6) < 0;
}

size_t tracker_db_snapshot(TrackerDb* db, TrackerRecord* out, size_t max) {
    furi_assert(db);
    if(max == 0) return 0;
    uint32_t now = furi_get_tick();

    furi_mutex_acquire(db->mutex, FuriWaitForever);
    size_t n = db->count < max ? db->count : max;
    memcpy(out, db->records, n * sizeof(TrackerRecord));
    furi_mutex_release(db->mutex);

    /* Sorted on the caller's copy, outside the lock - the radio worker must
     * never be made to wait on the UI. */
    for(size_t i = 1; i < n; i++) {
        TrackerRecord key = out[i];
        size_t j = i;
        while(j > 0 && tracker_record_before(&key, &out[j - 1], now)) {
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
    /* The pending queue is the records themselves: anything promoted but not
     * yet shown is exactly "following && !alerted". A single pending slot -
     * which is what this used to be - loses an alert whenever two trackers
     * trip before the UI thread drains its event queue, and a lost alert in an
     * anti-stalking tool is the worst bug this app can have. */
    for(size_t i = 0; i < db->count; i++) {
        if(db->records[i].following && !db->records[i].alerted) {
            *out = db->records[i];
            db->records[i].alerted = true;
            ok = true;
            break;
        }
    }
    furi_mutex_release(db->mutex);
    return ok;
}
