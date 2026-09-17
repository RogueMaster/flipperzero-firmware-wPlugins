#include "sr_model.h"

#include <string.h>

/*
 * ★ Pure logic; must not include furi (ADR-003).
 * The rules and rationale live in sr_model.h; this file holds only the implementation.
 */

static int hex_nibble(char c) {
    if(c >= '0' && c <= '9') {
        return c - '0';
    }
    if(c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if(c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

bool sr_mac_parse(const char* s, uint8_t out[6]) {
    uint8_t tmp[6];
    unsigned i;

    if(s == NULL || out == NULL) {
        return false;
    }

    for(i = 0; i < 17u; i++) {
        if(s[i] == '\0') {
            return false;
        }
    }
    if(s[17] != '\0') {
        return false;
    }

    for(i = 0; i < 6u; i++) {
        size_t p = (size_t)i * 3u;
        int hi;
        int lo;

        if(i < 5u && s[p + 2u] != ':') {
            return false;
        }
        hi = hex_nibble(s[p]);
        lo = hex_nibble(s[p + 1u]);
        if(hi < 0 || lo < 0) {
            return false;
        }
        tmp[i] = (uint8_t)(((unsigned)hi << 4) | (unsigned)lo);
    }

    memcpy(out, tmp, 6);
    return true;
}

static int8_t clamp_rssi(int v) {
    if(v < -128) {
        return (int8_t)-128;
    }
    if(v > 127) {
        return (int8_t)127;
    }
    return (int8_t)v;
}

static uint8_t clamp_channel(int v) {
    if(v < 0) {
        return 0;
    }
    if(v > 255) {
        return 255;
    }
    return (uint8_t)v;
}

static void recent_push(SrModel* m, const SrApBrief* b) {
    m->recent[m->recent_head] = *b;
    m->recent_head++;
    if(m->recent_head == (size_t)SR_RECENT_CAP) {
        m->recent_head = 0;
    }
    if(m->recent_count < (size_t)SR_RECENT_CAP) {
        m->recent_count++;
    }
}

void sr_model_init(SrModel* m, SrBloom* bloom, SrRawLog* rawlog) {
    if(m == NULL) {
        return;
    }
    memset(m, 0, sizeof(*m));
    m->bloom = bloom;
    m->rawlog = rawlog;
    m->session = SrSessionIdle;
}

void sr_model_reset_session(SrModel* m, bool also_reset_bloom) {
    if(m == NULL) {
        return;
    }

    m->ap_wifi = 0;
    m->ap_ble = 0;
    m->gps_blocks = 0;
    m->unknown_lines = 0;
    m->malformed_lines = 0;
    m->with_gps_fix = 0;
    m->unique_est = 0;
    m->illegal_trans = 0;
    m->started_tick_ms = 0;

    memset(&m->gps_csv, 0, sizeof(m->gps_csv));
    m->gps_csv_rev = 0;

    memset(m->recent, 0, sizeof(m->recent));
    m->recent_head = 0;
    m->recent_count = 0;

    m->last_unknown[0] = '\0';
    m->last_unknown_len = 0;

    /* session / gps / last_tick / the bloom pointer / the rawlog pointer / firmware /
     * firmware_rev / session_rev / gps_stop_rev are all preserved.
     * firmware is device identity, not session data (ADR-016 decision 5).
     * session_rev is a cumulative transition count and reset must not clear it (ADR-017 decision 3).
     * gps_stop_rev follows the same convention and reset must not clear it (ADR-020 / ADR-017
     * decision 3).
     * gps_csv / gps_csv_rev are session data and are cleared (D12). gps / gps_stop_rev are not.
     * bloom / rawlog contents are cleared only on explicit request; reset does not touch the ring. */
    if(also_reset_bloom && m->bloom != NULL) {
        sr_bloom_init(m->bloom);
    }
}

static bool apply_started(SrModel* m, uint32_t tick_ms) {
    if(m->session == SrSessionIdle) {
        m->session = SrSessionRunning;
        m->started_tick_ms = tick_ms;
        m->session_rev++;
        return true;
    }
    if(m->session == SrSessionStopped) {
        sr_model_reset_session(m, false);
        m->session = SrSessionRunning;
        m->started_tick_ms = tick_ms;
        m->session_rev++;
        return true;
    }
    m->illegal_trans++;
    return false;
}

static bool apply_stopped(SrModel* m, SrStopReason reason) {
    if(m->session == SrSessionRunning) {
        m->session = SrSessionStopped;
        m->session_rev++;
        return true;
    }
    /*
     * ADR-020 decision 4: a GPS / NMEA stop reply while idle is not an "illegal session
     * transition"; it is the normal close-out of an on-demand sample. Counting it in
     * illegal_trans would pollute the diagnostic bit on the T4.4 Session tab.
     * SrStopWifiTranRecv while idle still counts as illegal -- that one really is.
     */
    if(reason == SrStopGpsUpdates || reason == SrStopEndNmea) {
        m->gps_stop_rev++;
        return false;
    }
    m->illegal_trans++;
    return false;
}

static bool apply_ap(SrModel* m, const SrApRecord* rec, bool ble) {
    SrApBrief b;
    uint8_t mac[6];

    if(rec == NULL || !sr_mac_parse(rec->bssid, mac)) {
        m->malformed_lines++;
        return true;
    }

    memset(&b, 0, sizeof(b));
    memcpy(b.mac, mac, 6);
    sr_strlcpy(b.ssid, sizeof(b.ssid), rec->ssid);
    b.rssi = clamp_rssi(rec->rssi);
    b.channel = clamp_channel(rec->channel);
    b.flags = 0;
    if(ble) {
        b.flags = (uint8_t)(b.flags | SR_AP_FLAG_BLE);
        m->ap_ble++;
    } else {
        m->ap_wifi++;
    }
    if(rec->datetime[0] != '\0') {
        b.flags = (uint8_t)(b.flags | SR_AP_FLAG_GPS);
        m->with_gps_fix++;
    }

    /*
     * D12 / F6-F8: every successful wardrive row carries live GPS. Copy even without a
     * fix so the GPS tab can show Fix: No instead of "no data". gps_blocks is not
     * touched (V-061 probe). rec fields are NUL-terminated C strings (copy_cap), not
     * ADR-010 borrowed views, so sr_strlcpy is safe here.
     */
    m->gps_csv.fix = rec->datetime[0] != '\0';
    sr_strlcpy(m->gps_csv.lat, sizeof(m->gps_csv.lat), rec->lat);
    sr_strlcpy(m->gps_csv.lon, sizeof(m->gps_csv.lon), rec->lon);
    sr_strlcpy(m->gps_csv.alt, sizeof(m->gps_csv.alt), rec->alt);
    sr_strlcpy(m->gps_csv.acc, sizeof(m->gps_csv.acc), rec->acc);
    sr_strlcpy(m->gps_csv.datetime, sizeof(m->gps_csv.datetime), rec->datetime);
    m->gps_csv_rev++;

    if(m->bloom != NULL && sr_bloom_add(m->bloom, rec->bssid)) {
        m->unique_est++;
    }

    recent_push(m, &b);
    return true;
}

static bool apply_unknown(SrModel* m, const SrRawView* v) {
    size_t n;

    if(v == NULL || v->text == NULL) {
        m->malformed_lines++;
        return true;
    }

    /*
     * Copy strictly by len and never read text[len].
     * SrRawView may be a **window view without a NUL** (that is precisely what the len field
     * means), so sr_strlcpy would strlen past the end of the buffer -- proven with ASan on
     * 2026-08-16, where that path produces a heap-buffer-overflow (the strlen at
     * sr_types.h:230). See the ADR-010 addendum.
     */
    n = v->len;
    if(n > (size_t)SR_RAW_LINE_MAX) {
        n = (size_t)SR_RAW_LINE_MAX;
    }
    if(n > 0) {
        memcpy(m->last_unknown, v->text, n);
    }
    m->last_unknown[n] = '\0';
    m->last_unknown_len = n;
    m->unknown_lines++;
    /* Pass v->len rather than n: the ring truncates to 80 itself and sets cut. n is the 511 cap
     * of last_unknown, not the rawlog contract. */
    if(m->rawlog != NULL) {
        sr_rawlog_push(m->rawlog, v->text, v->len);
    }
    return true;
}

bool sr_model_apply(SrModel* m, const SrEvent* ev, uint32_t tick_ms) {
    if(m == NULL || ev == NULL) {
        return false;
    }

    m->last_tick_ms = tick_ms;

    switch(ev->kind) {
    case SrEventNone:
        return false;
    case SrEventScanStarted:
        return apply_started(m, tick_ms);
    case SrEventScanStopped:
        return apply_stopped(m, ev->u.stop);
    case SrEventApFound:
        return apply_ap(m, &ev->u.ap, false);
    case SrEventBleFound:
        return apply_ap(m, &ev->u.ble, true);
    case SrEventGps:
        m->gps = ev->u.gps;
        m->gps_blocks++;
        return true;
    case SrEventFirmware:
        m->firmware = ev->u.firmware;
        m->firmware_rev++;
        return true;
    case SrEventUnknown:
        return apply_unknown(m, &ev->u.unknown);
    default:
        return false;
    }
}

const SrApBrief* sr_model_recent(const SrModel* m, size_t idx) {
    size_t pos;

    if(m == NULL || idx >= m->recent_count) {
        return NULL;
    }
    /* newest lives at head-1; subtracting one less would point recent(0) at the next write slot. */
    pos = (m->recent_head + (size_t)SR_RECENT_CAP - 1u - idx) % (size_t)SR_RECENT_CAP;
    return &m->recent[pos];
}

size_t sr_model_recent_count(const SrModel* m) {
    return m == NULL ? 0 : m->recent_count;
}
