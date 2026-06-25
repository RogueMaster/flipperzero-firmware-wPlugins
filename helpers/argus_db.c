#include "argus_db.h"

#include <string.h>
#include <stdlib.h>

#define DEAUTH_RING 64u

struct ArgusDb {
    FuriMutex* mutex;

    char guard[ARGUS_SSID_MAX];

    ArgusAp aps[ARGUS_MAX_APS];
    size_t ap_count;

    ArgusThreat log[ARGUS_MAX_LOG];
    size_t log_count; // entries stored (<= ARGUS_MAX_LOG)
    size_t log_head; // next write slot

    uint32_t deauth_total;
    uint32_t frame_total;
    uint32_t last_deauth_tick;
    uint32_t deauth_rate;
    bool storm_active;

    uint32_t recent[DEAUTH_RING]; // ticks of recent deauths (sliding window)
    size_t recent_head;
    size_t recent_count;
};

/* ---------- small helpers ---------- */

static bool mac_eq(const uint8_t a[6], const uint8_t b[6]) {
    return memcmp(a, b, 6) == 0;
}

static void ssid_copy(char* dst, const char* src) {
    if(!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, ARGUS_SSID_MAX - 1);
    dst[ARGUS_SSID_MAX - 1] = '\0';
}

static void recent_push(ArgusDb* db, uint32_t t) {
    db->recent[db->recent_head] = t;
    db->recent_head = (db->recent_head + 1) % DEAUTH_RING;
    if(db->recent_count < DEAUTH_RING) db->recent_count++;
}

static uint32_t recent_within(ArgusDb* db, uint32_t now, uint32_t window) {
    uint32_t c = 0;
    for(size_t i = 0; i < db->recent_count; i++) {
        if((uint32_t)(now - db->recent[i]) <= window) c++;
    }
    return c;
}

static void log_push(ArgusDb* db, const ArgusThreat* t) {
    db->log[db->log_head] = *t;
    db->log_head = (db->log_head + 1) % ARGUS_MAX_LOG;
    if(db->log_count < ARGUS_MAX_LOG) db->log_count++;
}

/* Re-evaluate which APs sharing the guarded SSID are clones. The strongest
 * signal is assumed to be the real router (home); every other BSSID carrying
 * the same SSID is an evil twin. Returns the live twin count. */
static size_t reclassify_twins(ArgusDb* db) {
    size_t twins = 0;
    if(db->guard[0] == '\0') {
        for(size_t i = 0; i < db->ap_count; i++) db->aps[i].clone = false;
        return 0;
    }

    int home = -1;
    int8_t best = -127;
    for(size_t i = 0; i < db->ap_count; i++) {
        if(strncmp(db->aps[i].ssid, db->guard, ARGUS_SSID_MAX) == 0) {
            if(home < 0 || db->aps[i].rssi > best) {
                best = db->aps[i].rssi;
                home = (int)i;
            }
        }
    }
    for(size_t i = 0; i < db->ap_count; i++) {
        bool match = strncmp(db->aps[i].ssid, db->guard, ARGUS_SSID_MAX) == 0;
        db->aps[i].clone = match && ((int)i != home);
        if(db->aps[i].clone) twins++;
    }
    return twins;
}

/* ---------- lifecycle ---------- */

ArgusDb* argus_db_alloc(void) {
    ArgusDb* db = malloc(sizeof(ArgusDb));
    memset(db, 0, sizeof(ArgusDb));
    db->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    return db;
}

void argus_db_free(ArgusDb* db) {
    furi_assert(db);
    furi_mutex_free(db->mutex);
    free(db);
}

void argus_db_reset(ArgusDb* db) {
    furi_assert(db);
    furi_mutex_acquire(db->mutex, FuriWaitForever);
    db->ap_count = 0;
    db->log_count = 0;
    db->log_head = 0;
    db->deauth_total = 0;
    db->frame_total = 0;
    db->last_deauth_tick = 0;
    db->deauth_rate = 0;
    db->storm_active = false;
    db->recent_head = 0;
    db->recent_count = 0;
    furi_mutex_release(db->mutex);
}

/* ---------- guarded SSID ---------- */

void argus_db_set_guard(ArgusDb* db, const char* ssid) {
    furi_assert(db);
    furi_mutex_acquire(db->mutex, FuriWaitForever);
    ssid_copy(db->guard, ssid);
    reclassify_twins(db);
    furi_mutex_release(db->mutex);
}

void argus_db_get_guard(ArgusDb* db, char* out, size_t out_len) {
    furi_assert(db);
    furi_mutex_acquire(db->mutex, FuriWaitForever);
    strncpy(out, db->guard, out_len - 1);
    out[out_len - 1] = '\0';
    furi_mutex_release(db->mutex);
}

bool argus_db_has_guard(ArgusDb* db) {
    furi_assert(db);
    furi_mutex_acquire(db->mutex, FuriWaitForever);
    bool has = db->guard[0] != '\0';
    furi_mutex_release(db->mutex);
    return has;
}

/* ---------- ingest ---------- */

void argus_db_note_frame(ArgusDb* db) {
    furi_assert(db);
    furi_mutex_acquire(db->mutex, FuriWaitForever);
    db->frame_total++;
    furi_mutex_release(db->mutex);
}

bool argus_db_on_deauth(
    ArgusDb* db,
    ArgusThreatKind kind,
    const uint8_t src[6],
    const uint8_t bssid[6],
    uint8_t channel,
    int8_t rssi,
    uint8_t reason,
    uint32_t storm_threshold) {
    furi_assert(db);
    furi_mutex_acquire(db->mutex, FuriWaitForever);

    uint32_t now = furi_get_tick();
    db->deauth_total++;
    db->frame_total++;
    db->last_deauth_tick = now;
    recent_push(db, now);
    db->deauth_rate = recent_within(db, now, ARGUS_STORM_WINDOW_MS);

    ArgusThreat t;
    memset(&t, 0, sizeof(t));
    t.kind = kind;
    memcpy(t.addr, src, 6);
    t.channel = channel;
    t.rssi = rssi;
    t.reason = reason;
    t.time_tick = now;
    /* tag the event with the guarded SSID if the attack hits our BSSID group */
    for(size_t i = 0; i < db->ap_count; i++) {
        if(mac_eq(db->aps[i].bssid, bssid)) {
            ssid_copy(t.ssid, db->aps[i].ssid);
            break;
        }
    }
    log_push(db, &t);

    bool now_storm = (storm_threshold > 0) && (db->deauth_rate >= storm_threshold);
    bool crossed = now_storm && !db->storm_active;
    db->storm_active = now_storm;

    furi_mutex_release(db->mutex);
    return crossed;
}

bool argus_db_on_ap(
    ArgusDb* db,
    const uint8_t bssid[6],
    uint8_t channel,
    int8_t rssi,
    ArgusEnc enc,
    const char* ssid) {
    furi_assert(db);
    furi_mutex_acquire(db->mutex, FuriWaitForever);

    uint32_t now = furi_get_tick();
    db->frame_total++;

    /* find existing by BSSID */
    int idx = -1;
    for(size_t i = 0; i < db->ap_count; i++) {
        if(mac_eq(db->aps[i].bssid, bssid)) {
            idx = (int)i;
            break;
        }
    }

    bool newly_inserted = false;
    bool was_clone = false;

    if(idx < 0) {
        if(db->ap_count < ARGUS_MAX_APS) {
            idx = (int)db->ap_count++;
        } else {
            /* table full: evict the least-recently-seen AP */
            int oldest = 0;
            uint32_t oldest_t = db->aps[0].last_seen;
            for(size_t i = 1; i < db->ap_count; i++) {
                if(db->aps[i].last_seen < oldest_t) {
                    oldest_t = db->aps[i].last_seen;
                    oldest = (int)i;
                }
            }
            idx = oldest;
        }
        memset(&db->aps[idx], 0, sizeof(ArgusAp));
        memcpy(db->aps[idx].bssid, bssid, 6);
        newly_inserted = true;
    } else {
        was_clone = db->aps[idx].clone;
    }

    ArgusAp* ap = &db->aps[idx];
    ap->channel = channel;
    ap->rssi = rssi;
    ap->enc = enc;
    ssid_copy(ap->ssid, ssid);
    if(ap->beacons < 0xFFFF) ap->beacons++;
    ap->last_seen = now;

    reclassify_twins(db);

    bool new_twin = ap->clone && (newly_inserted || !was_clone);
    if(new_twin) {
        ArgusThreat t;
        memset(&t, 0, sizeof(t));
        t.kind = ArgusThreatEvilTwin;
        memcpy(t.addr, ap->bssid, 6);
        ssid_copy(t.ssid, ap->ssid);
        t.channel = ap->channel;
        t.rssi = ap->rssi;
        t.time_tick = now;
        log_push(db, &t);
    }

    furi_mutex_release(db->mutex);
    return new_twin;
}

void argus_db_tick(ArgusDb* db, uint32_t window_ms) {
    furi_assert(db);
    furi_mutex_acquire(db->mutex, FuriWaitForever);
    uint32_t now = furi_get_tick();
    db->deauth_rate = recent_within(db, now, window_ms ? window_ms : ARGUS_STORM_WINDOW_MS);
    furi_mutex_release(db->mutex);
}

/* ---------- snapshots ---------- */

void argus_db_get_stats(ArgusDb* db, ArgusStats* out) {
    furi_assert(db);
    furi_mutex_acquire(db->mutex, FuriWaitForever);
    out->deauth_total = db->deauth_total;
    out->deauth_rate = db->deauth_rate;
    out->frame_total = db->frame_total;
    out->last_deauth_tick = db->last_deauth_tick;
    out->ap_count = db->ap_count;
    size_t twins = 0;
    for(size_t i = 0; i < db->ap_count; i++)
        if(db->aps[i].clone) twins++;
    out->twin_count = twins;
    furi_mutex_release(db->mutex);
}

size_t argus_db_copy_aps(ArgusDb* db, ArgusAp* out, size_t max) {
    furi_assert(db);
    furi_mutex_acquire(db->mutex, FuriWaitForever);
    size_t n = db->ap_count < max ? db->ap_count : max;
    for(size_t i = 0; i < n; i++) out[i] = db->aps[i];
    furi_mutex_release(db->mutex);
    return n;
}

size_t argus_db_copy_twins(ArgusDb* db, ArgusAp* out, size_t max) {
    furi_assert(db);
    furi_mutex_acquire(db->mutex, FuriWaitForever);
    size_t n = 0;
    for(size_t i = 0; i < db->ap_count && n < max; i++) {
        if(db->aps[i].clone) out[n++] = db->aps[i];
    }
    furi_mutex_release(db->mutex);
    return n;
}

size_t argus_db_copy_log(ArgusDb* db, ArgusThreat* out, size_t max) {
    furi_assert(db);
    furi_mutex_acquire(db->mutex, FuriWaitForever);
    size_t n = db->log_count < max ? db->log_count : max;
    /* newest first: walk back from the most recent write */
    for(size_t i = 0; i < n; i++) {
        size_t slot = (db->log_head + ARGUS_MAX_LOG - 1 - i) % ARGUS_MAX_LOG;
        out[i] = db->log[slot];
    }
    furi_mutex_release(db->mutex);
    return n;
}

const char* argus_enc_label(ArgusEnc enc) {
    switch(enc) {
    case ArgusEncOpen:
        return "Open";
    case ArgusEncWep:
        return "WEP";
    case ArgusEncWpa:
        return "WPA";
    case ArgusEncWpa2:
        return "WPA2";
    case ArgusEncWpa3:
        return "WPA3";
    default:
        return "?";
    }
}
