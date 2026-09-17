#include "sr_test.h"

#include "sr_model.h"
#include "sr_bloom.h"
#include "sr_types.h"
#include "sr_line.h"
#include "sr_parse_marauder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

_Static_assert(sizeof(SrApBrief) <= 40, "SrApBrief over 40 B budget");
_Static_assert(SR_RECENT_CAP == 64, "ring cap is Plan 3.5 64 entries");
_Static_assert(SR_AP_FLAG_BLE == 1u, "bit0 = BLE");
_Static_assert(SR_AP_FLAG_GPS == 2u, "bit1 = GPS fix");

/* SrModel is ≈ 3 KB, kept off the test stack. The bloom filter's 4 KB is likewise static. */
static SrModel g_model;
static SrBloom g_bloom;
static SrBloom g_bloom_b;

/* -------------------------------------------------------------------------- */
/* A standalone oracle                                                        */
/*                                                                            */
/* Differences from the implementation (to avoid a tautological test):        */
/*   - MAC: a strlen + 256-entry nibble table, not a byte-by-byte scan over   */
/*     17 if-else ranges                                                      */
/*   - State machine: a 3×8 next-state table, not switch(kind) followed by    */
/*     if(session)                                                            */
/*   - Ring buffer: append at the tail + memmove left, not head modulo        */
/*   - RSSI: ternary saturation, not two early-return ifs                     */
/* -------------------------------------------------------------------------- */

static signed char g_nib[256];
static int g_nib_ready;

static void nib_init(void) {
    unsigned i;
    if(g_nib_ready) {
        return;
    }
    for(i = 0; i < 256u; i++) {
        g_nib[i] = (signed char)-1;
    }
    for(i = 0; i < 10u; i++) {
        g_nib[(unsigned char)('0' + i)] = (signed char)i;
    }
    for(i = 0; i < 6u; i++) {
        g_nib[(unsigned char)('A' + i)] = (signed char)(10 + i);
        g_nib[(unsigned char)('a' + i)] = (signed char)(10 + i);
    }
    g_nib_ready = 1;
}

static bool oracle_mac(const char* s, uint8_t out[6]) {
    size_t n;
    unsigned i;
    uint8_t tmp[6];

    nib_init();
    if(s == NULL || out == NULL) {
        return false;
    }
    n = 0;
    while(s[n] != '\0') {
        n++;
    }
    if(n != 17u) {
        return false;
    }
    for(i = 0; i < 6u; i++) {
        size_t p = (size_t)i * 3u;
        signed char hi = g_nib[(unsigned char)s[p]];
        signed char lo = g_nib[(unsigned char)s[p + 1u]];

        if(i != 5u && s[p + 2u] != ':') {
            return false;
        }
        if(hi < 0 || lo < 0) {
            return false;
        }
        tmp[i] = (uint8_t)(((unsigned)hi << 4) | (unsigned)lo);
    }
    memcpy(out, tmp, 6);
    return true;
}

static int8_t oracle_rssi(int v) {
    const int lo = -128;
    const int hi = 127;
    v = v < lo ? lo : v;
    v = v > hi ? hi : v;
    return (int8_t)v;
}

static uint8_t oracle_ch(int v) {
    v = v < 0 ? 0 : v;
    v = v > 255 ? 255 : v;
    return (uint8_t)v;
}

/*
 * Row = current state, column = SrEventKind (None..Unknown, contiguous 0..7). -1 = illegal, state
 * stays unchanged.
 */
static const int8_t kOracleNext[3][8] = {
    /* Idle     */ {0, 0, 0, 0, 1, -1, 0, 0},
    /* Running  */ {1, 1, 1, 1, -1, 2, 1, 1},
    /* Stopped  */ {2, 2, 2, 2, 1, -1, 2, 2},
};

typedef struct {
    SrSessionState session;
    uint32_t ap_wifi;
    uint32_t ap_ble;
    uint32_t gps_blocks;
    uint32_t unknown_lines;
    uint32_t malformed_lines;
    uint32_t with_gps_fix;
    uint32_t unique_est;
    uint32_t illegal_trans;
    uint32_t last_tick_ms;
    uint32_t started_tick_ms;
    SrGpsSnapshot gps;
    SrGpsCsvView gps_csv;
    uint32_t gps_csv_rev;
    SrFirmwareInfo firmware;
    uint32_t firmware_rev;
    char last_unknown[SR_RAW_LINE_MAX + 1];
    size_t last_unknown_len;
    SrApBrief rec[SR_RECENT_CAP]; /* the most recent entry is rec[n-1] */
    size_t n;
    SrBloom* bloom;
} OracleModel;

static OracleModel g_oracle;

static void oracle_reset_stats(OracleModel* o) {
    o->ap_wifi = 0;
    o->ap_ble = 0;
    o->gps_blocks = 0;
    o->unknown_lines = 0;
    o->malformed_lines = 0;
    o->with_gps_fix = 0;
    o->unique_est = 0;
    o->illegal_trans = 0;
    o->started_tick_ms = 0;
    memset(&o->gps_csv, 0, sizeof(o->gps_csv));
    o->gps_csv_rev = 0;
    o->last_unknown[0] = '\0';
    o->last_unknown_len = 0;
    o->n = 0;
    memset(o->rec, 0, sizeof(o->rec));
}

static void oracle_init(OracleModel* o, SrBloom* bloom) {
    memset(o, 0, sizeof(*o));
    o->session = SrSessionIdle;
    o->bloom = bloom;
}

static void oracle_push(OracleModel* o, const SrApBrief* b) {
    if(o->n < (size_t)SR_RECENT_CAP) {
        o->rec[o->n] = *b;
        o->n++;
        return;
    }
    memmove(o->rec, o->rec + 1, ((size_t)SR_RECENT_CAP - 1u) * sizeof(o->rec[0]));
    o->rec[SR_RECENT_CAP - 1] = *b;
}

static const SrApBrief* oracle_recent(const OracleModel* o, size_t idx) {
    if(o == NULL || idx >= o->n) {
        return NULL;
    }
    return &o->rec[o->n - 1u - idx];
}

static bool oracle_apply(OracleModel* o, const SrEvent* ev, uint32_t tick_ms) {
    unsigned st;
    unsigned k;
    int8_t next;
    const SrApRecord* rec;
    bool ble;

    if(o == NULL || ev == NULL) {
        return false;
    }
    o->last_tick_ms = tick_ms;

    k = (unsigned)ev->kind;
    if(k > (unsigned)SrEventUnknown) {
        return false;
    }
    st = (unsigned)o->session;
    next = kOracleNext[st][k];

    if(next < 0) {
        o->illegal_trans++;
        return false;
    }

    if(ev->kind == SrEventScanStarted) {
        if(o->session == SrSessionStopped) {
            oracle_reset_stats(o);
        }
        o->session = SrSessionRunning;
        o->started_tick_ms = tick_ms;
        return true;
    }
    if(ev->kind == SrEventScanStopped) {
        o->session = SrSessionStopped;
        return true;
    }
    if(ev->kind == SrEventNone) {
        return false;
    }
    if(ev->kind == SrEventFirmware) {
        o->firmware = ev->u.firmware;
        o->firmware_rev++;
        return true;
    }
    if(ev->kind == SrEventGps) {
        o->gps = ev->u.gps;
        o->gps_blocks++;
        return true;
    }
    if(ev->kind == SrEventUnknown) {
        size_t n;
        if(ev->u.unknown.text == NULL) {
            o->malformed_lines++;
            return true;
        }
        n = sr_strlcpy(o->last_unknown, sizeof(o->last_unknown), ev->u.unknown.text);
        if(n > (size_t)SR_RAW_LINE_MAX) {
            n = (size_t)SR_RAW_LINE_MAX;
        }
        if(ev->u.unknown.len < n) {
            o->last_unknown[ev->u.unknown.len] = '\0';
            n = ev->u.unknown.len;
        }
        o->last_unknown_len = n;
        o->unknown_lines++;
        return true;
    }

    ble = (ev->kind == SrEventBleFound);
    rec = ble ? &ev->u.ble : &ev->u.ap;
    {
        SrApBrief b;
        uint8_t mac[6];

        if(!oracle_mac(rec->bssid, mac)) {
            o->malformed_lines++;
            return true;
        }
        memset(&b, 0, sizeof(b));
        memcpy(b.mac, mac, 6);
        sr_strlcpy(b.ssid, sizeof(b.ssid), rec->ssid);
        b.rssi = oracle_rssi(rec->rssi);
        b.channel = oracle_ch(rec->channel);
        b.flags = 0;
        if(ble) {
            b.flags = (uint8_t)(b.flags | SR_AP_FLAG_BLE);
            o->ap_ble++;
        } else {
            o->ap_wifi++;
        }
        {
            /* Independent of apply_ap: walk the datetime payload instead of testing [0]. */
            unsigned dt_n = 0;
            while(rec->datetime[dt_n] != '\0' && dt_n < (unsigned)SR_DATETIME_MAX + 1u) {
                dt_n++;
            }
            if(dt_n > 0u) {
                b.flags = (uint8_t)(b.flags | SR_AP_FLAG_GPS);
                o->with_gps_fix = o->with_gps_fix + 1u;
            }
            o->gps_csv.fix = (dt_n > 0u);
            memcpy(o->gps_csv.lat, rec->lat, sizeof(o->gps_csv.lat));
            memcpy(o->gps_csv.lon, rec->lon, sizeof(o->gps_csv.lon));
            memcpy(o->gps_csv.alt, rec->alt, sizeof(o->gps_csv.alt));
            memcpy(o->gps_csv.acc, rec->acc, sizeof(o->gps_csv.acc));
            memcpy(o->gps_csv.datetime, rec->datetime, sizeof(o->gps_csv.datetime));
            o->gps_csv_rev = o->gps_csv_rev + 1u;
        }
        if(o->bloom != NULL && sr_bloom_add(o->bloom, rec->bssid)) {
            o->unique_est++;
        }
        oracle_push(o, &b);
        return true;
    }
}

static int briefs_eq(const SrApBrief* a, const SrApBrief* b) {
    if(a == NULL || b == NULL) {
        return a == b;
    }
    return memcmp(a, b, sizeof(*a)) == 0;
}

static void check_model_matches_oracle(const SrModel* m, const OracleModel* o) {
    size_t i;

    CHECK(m->session == o->session);
    CHECK(m->ap_wifi == o->ap_wifi);
    CHECK(m->ap_ble == o->ap_ble);
    CHECK(m->gps_blocks == o->gps_blocks);
    CHECK(m->unknown_lines == o->unknown_lines);
    CHECK(m->malformed_lines == o->malformed_lines);
    CHECK(m->with_gps_fix == o->with_gps_fix);
    CHECK(m->unique_est == o->unique_est);
    CHECK(m->illegal_trans == o->illegal_trans);
    CHECK(m->last_tick_ms == o->last_tick_ms);
    CHECK(m->started_tick_ms == o->started_tick_ms);
    CHECK(m->gps.fix == o->gps.fix);
    CHECK(m->gps_csv_rev == o->gps_csv_rev);
    CHECK(m->gps_csv.fix == o->gps_csv.fix);
    CHECK(strcmp(m->gps_csv.lat, o->gps_csv.lat) == 0);
    CHECK(strcmp(m->gps_csv.lon, o->gps_csv.lon) == 0);
    CHECK(strcmp(m->gps_csv.alt, o->gps_csv.alt) == 0);
    CHECK(strcmp(m->gps_csv.acc, o->gps_csv.acc) == 0);
    CHECK(strcmp(m->gps_csv.datetime, o->gps_csv.datetime) == 0);
    CHECK(m->firmware_rev == o->firmware_rev);
    CHECK(m->firmware.kind == o->firmware.kind);
    CHECK(strcmp(m->firmware.firmware, o->firmware.firmware) == 0);
    CHECK(strcmp(m->last_unknown, o->last_unknown) == 0);
    CHECK(m->last_unknown_len == o->last_unknown_len);
    CHECK(sr_model_recent_count(m) == o->n);
    for(i = 0; i < o->n; i++) {
        CHECK(briefs_eq(sr_model_recent(m, i), oracle_recent(o, i)));
    }
    CHECK(sr_model_recent(m, o->n) == NULL);
}

/* -------------------------------------------------------------------------- */
/* Event construction                                                         */
/* -------------------------------------------------------------------------- */

static void ev_clear(SrEvent* ev) {
    memset(ev, 0, sizeof(*ev));
}

static void ev_ap(SrEvent* ev, SrEventKind kind, const char* mac, const char* ssid, int ch, int rssi) {
    SrApRecord* r;

    ev_clear(ev);
    ev->kind = kind;
    r = (kind == SrEventBleFound) ? &ev->u.ble : &ev->u.ap;
    sr_strlcpy(r->bssid, sizeof(r->bssid), mac);
    sr_strlcpy(r->ssid, sizeof(r->ssid), ssid);
    r->channel = ch;
    r->rssi = rssi;
    r->radio = (kind == SrEventBleFound) ? SrRadioBle : SrRadioWifi;
}

static void ev_kind(SrEvent* ev, SrEventKind kind) {
    ev_clear(ev);
    ev->kind = kind;
}

static void model_fresh(SrSessionState st) {
    sr_bloom_init(&g_bloom);
    sr_model_init(&g_model, &g_bloom, NULL);
    if(st == SrSessionIdle) {
        return;
    }
    {
        SrEvent ev;
        ev_kind(&ev, SrEventScanStarted);
        CHECK(sr_model_apply(&g_model, &ev, 1) == true);
        CHECK(g_model.session == SrSessionRunning);
    }
    if(st == SrSessionRunning) {
        return;
    }
    {
        SrEvent ev;
        ev_kind(&ev, SrEventScanStopped);
        CHECK(sr_model_apply(&g_model, &ev, 2) == true);
        CHECK(g_model.session == SrSessionStopped);
    }
}

/* -------------------------------------------------------------------------- */
/* Targeted test cases                                                        */
/* -------------------------------------------------------------------------- */

static void test_sizeof_and_init(void) {
    fprintf(stderr, "sizeof(SrApBrief)=%zu\n", sizeof(SrApBrief));
    fprintf(stderr, "sizeof(SrModel)=%zu\n", sizeof(SrModel));
    fprintf(stderr, "sizeof(SrGpsCsvView)=%zu\n", sizeof(SrGpsCsvView));
    fprintf(stderr, "sizeof(SrParser)=%zu\n", sizeof(SrParser));
    fprintf(stderr, "sizeof(SrFirmwareInfo)=%zu\n", sizeof(SrFirmwareInfo));
    CHECK(sizeof(SrApBrief) == 34);
    CHECK(sizeof(SrApBrief) <= 40);
    CHECK(sizeof(SrModel) < sizeof(SrBloom));
    CHECK(sizeof(SrModel) <= 4096);
    CHECK(sizeof(g_model.recent) == (size_t)SR_RECENT_CAP * sizeof(SrApBrief));
    CHECK((size_t)SR_RECENT_CAP * sizeof(SrApBrief) == 2176u);

    sr_bloom_init(&g_bloom);
    sr_model_init(&g_model, &g_bloom, NULL);
    CHECK(g_model.session == SrSessionIdle);
    CHECK(g_model.bloom == &g_bloom);
    CHECK(g_model.ap_wifi == 0);
    CHECK(g_model.ap_ble == 0);
    CHECK(g_model.gps_blocks == 0);
    CHECK(g_model.unknown_lines == 0);
    CHECK(g_model.malformed_lines == 0);
    CHECK(g_model.with_gps_fix == 0);
    CHECK(g_model.unique_est == 0);
    CHECK(g_model.illegal_trans == 0);
    CHECK(sr_model_recent_count(&g_model) == 0);
    CHECK(sr_model_recent(&g_model, 0) == NULL);
    CHECK(g_model.gps.fix == false);
    CHECK(g_model.gps_csv_rev == 0);
    CHECK(g_model.gps_csv.fix == false);
    CHECK(g_model.gps_csv.lat[0] == '\0');
    CHECK(g_model.last_unknown[0] == '\0');
    CHECK(g_model.firmware_rev == 0);
}

static void test_firmware_apply_and_reset_keeps_identity(void) {
    SrEvent ev;
    SrFirmwareInfo snap;

    sr_bloom_init(&g_bloom);
    sr_model_init(&g_model, &g_bloom, NULL);
    CHECK(g_model.firmware_rev == 0);

    ev_clear(&ev);
    ev.kind = SrEventFirmware;
    ev.u.firmware.kind = SrSourceMarauder;
    sr_strlcpy(ev.u.firmware.firmware, sizeof(ev.u.firmware.firmware), "Marauder");
    sr_strlcpy(ev.u.firmware.version, sizeof(ev.u.firmware.version), "v1.14.1");
    sr_strlcpy(ev.u.firmware.hardware, sizeof(ev.u.firmware.hardware), "ESP32-C5 DevKit");
    sr_strlcpy(ev.u.firmware.esp_idf, sizeof(ev.u.firmware.esp_idf), "v5.5.1-710-g8410210c9a");

    CHECK(sr_model_apply(&g_model, &ev, 50) == true);
    CHECK(g_model.firmware_rev == 1);
    CHECK(g_model.firmware.kind == SrSourceMarauder);
    CHECK(strcmp(g_model.firmware.firmware, "Marauder") == 0);
    CHECK(strcmp(g_model.firmware.version, "v1.14.1") == 0);
    CHECK(strcmp(g_model.firmware.hardware, "ESP32-C5 DevKit") == 0);
    CHECK(strcmp(g_model.firmware.esp_idf, "v5.5.1-710-g8410210c9a") == 0);

    CHECK(sr_model_apply(&g_model, &ev, 51) == true);
    CHECK(g_model.firmware_rev == 2);

    snap = g_model.firmware;
    sr_model_reset_session(&g_model, false);
    CHECK(g_model.firmware_rev == 2);
    CHECK(memcmp(&g_model.firmware, &snap, sizeof(snap)) == 0);
}

static void test_null_safety(void) {
    uint8_t mac[6];
    SrEvent ev;

    memset(mac, 0xFF, sizeof(mac));
    sr_model_init(NULL, NULL, NULL);
    sr_model_reset_session(NULL, true);
    CHECK(sr_model_apply(NULL, NULL, 0) == false);
    ev_kind(&ev, SrEventScanStarted);
    CHECK(sr_model_apply(NULL, &ev, 0) == false);

    sr_bloom_init(&g_bloom);
    sr_model_init(&g_model, &g_bloom, NULL);
    CHECK(sr_model_apply(&g_model, NULL, 0) == false);
    CHECK(g_model.session == SrSessionIdle);

    CHECK(sr_model_recent(NULL, 0) == NULL);
    CHECK(sr_model_recent_count(NULL) == 0);

    CHECK(sr_mac_parse(NULL, mac) == false);
    CHECK(mac[0] == 0xFF);
    CHECK(sr_mac_parse("AA:BB:CC:DD:EE:FF", NULL) == false);
}

static void test_mac_parse(void) {
    uint8_t mac[6];
    uint8_t expect[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    memset(mac, 0x11, sizeof(mac));
    CHECK(sr_mac_parse("AA:BB:CC:DD:EE:FF", mac) == true);
    CHECK(memcmp(mac, expect, 6) == 0);

    memset(mac, 0x11, sizeof(mac));
    CHECK(sr_mac_parse("aa:bb:cc:dd:ee:ff", mac) == true);
    CHECK(memcmp(mac, expect, 6) == 0);

    memset(mac, 0x11, sizeof(mac));
    CHECK(sr_mac_parse("Aa:Bb:Cc:Dd:Ee:Ff", mac) == true);
    CHECK(memcmp(mac, expect, 6) == 0);

    /* agrees with the standalone oracle */
    {
        uint8_t a[6];
        uint8_t b[6];
        CHECK(oracle_mac("0a:1B:2c:3D:4e:5F", a) == true);
        CHECK(sr_mac_parse("0a:1B:2c:3D:4e:5F", b) == true);
        CHECK(memcmp(a, b, 6) == 0);
    }

    memset(mac, 0x22, sizeof(mac));
    CHECK(sr_mac_parse("AA:BB:CC:DD:EE:F", mac) == false);
    CHECK(mac[0] == 0x22 && mac[5] == 0x22);

    memset(mac, 0x22, sizeof(mac));
    CHECK(sr_mac_parse("AA:BB:CC:DD:EE:FF:", mac) == false);
    CHECK(mac[0] == 0x22);

    memset(mac, 0x22, sizeof(mac));
    CHECK(sr_mac_parse("AA:BB:CC:DD:EE:FFF", mac) == false);
    CHECK(mac[0] == 0x22);

    memset(mac, 0x22, sizeof(mac));
    CHECK(sr_mac_parse("AA:BB:CC:DD:EE:GG", mac) == false);
    CHECK(mac[0] == 0x22);

    memset(mac, 0x22, sizeof(mac));
    CHECK(sr_mac_parse("AA:BB:CC:DD:EE:0G", mac) == false);
    CHECK(mac[0] == 0x22);

    memset(mac, 0x22, sizeof(mac));
    CHECK(sr_mac_parse("AA-BB-CC-DD-EE-FF", mac) == false);
    CHECK(mac[0] == 0x22);

    memset(mac, 0x22, sizeof(mac));
    CHECK(sr_mac_parse("AA:BB:CC:DD:EE.FF", mac) == false);
    CHECK(mac[0] == 0x22);

    memset(mac, 0x22, sizeof(mac));
    CHECK(sr_mac_parse("AABBCCDDEEFF", mac) == false);
    CHECK(mac[0] == 0x22);

    memset(mac, 0x22, sizeof(mac));
    CHECK(sr_mac_parse("", mac) == false);
    CHECK(mac[0] == 0x22);

    memset(mac, 0x22, sizeof(mac));
    CHECK(sr_mac_parse("AA:BB:CC:DD:EE:FF ", mac) == false);
    CHECK(mac[0] == 0x22);

    CHECK(sr_mac_parse(NULL, mac) == false);
    CHECK(sr_mac_parse("AA:BB:CC:DD:EE:FF", NULL) == false);
}

static const char* kKindName[8] = {
    "None",
    "ApFound",
    "BleFound",
    "Gps",
    "ScanStarted",
    "ScanStopped",
    "Firmware",
    "Unknown",
};

static const char* kSessName[3] = {"Idle", "Running", "Stopped"};

static void fill_kind_payload(SrEvent* ev, SrEventKind kind) {
    ev_clear(ev);
    ev->kind = kind;
    switch(kind) {
    case SrEventApFound:
        ev_ap(ev, SrEventApFound, "02:00:00:00:00:01", "WifiOne", 6, -70);
        break;
    case SrEventBleFound:
        ev_ap(ev, SrEventBleFound, "06:00:00:00:00:02", "", 0, -80);
        break;
    case SrEventGps:
        ev->u.gps.fix = true;
        sr_strlcpy(ev->u.gps.sats, sizeof(ev->u.gps.sats), "8");
        sr_strlcpy(ev->u.gps.lat, sizeof(ev->u.gps.lat), "31.23");
        break;
    case SrEventUnknown:
        /*
         * points at a literal; this case only tests migration, borrow regressions are tested
         * elsewhere.
         */
        ev->u.unknown.text = "raw-line";
        ev->u.unknown.len = 8;
        break;
    case SrEventScanStopped:
        ev->u.stop = SrStopWifiTranRecv;
        break;
    case SrEventFirmware:
        ev->u.firmware.kind = SrSourceMarauder;
        sr_strlcpy(ev->u.firmware.firmware, sizeof(ev->u.firmware.firmware), "Marauder");
        break;
    default:
        break;
    }
}

static void test_transitions_24(void) {
    unsigned si;
    unsigned ki;
    unsigned n = 0;

    /*
     * 3 states × 8 kinds. The expected next state comes from a table different from the
     * implementation's.
     * Illegal edges: Idle+Stop / Running+Start / Stopped+Stop.
     */
    for(si = 0; si < 3u; si++) {
        for(ki = 0; ki < 8u; ki++) {
            SrEvent ev;
            SrSessionState from = (SrSessionState)si;
            SrEventKind kind = (SrEventKind)ki;
            int8_t expect = kOracleNext[si][ki];
            uint32_t ill0;
            bool changed;

            model_fresh(from);
            ill0 = g_model.illegal_trans;
            fill_kind_payload(&ev, kind);
            changed = sr_model_apply(&g_model, &ev, 100u + n);
            n++;

            if(expect < 0) {
                CHECK(g_model.session == from);
                CHECK(g_model.illegal_trans == ill0 + 1u);
                CHECK(changed == false);
            } else {
                CHECK(g_model.session == (SrSessionState)expect);
                CHECK(g_model.illegal_trans == ill0);
                if(kind == SrEventNone) {
                    CHECK(changed == false);
                } else {
                    CHECK(changed == true);
                }
            }

            if(kind == SrEventApFound && expect >= 0) {
                CHECK(g_model.ap_wifi == 1);
                CHECK(sr_model_recent_count(&g_model) == 1);
                CHECK(sr_model_recent(&g_model, 0) != NULL);
                CHECK(strcmp(sr_model_recent(&g_model, 0)->ssid, "WifiOne") == 0);
                CHECK((sr_model_recent(&g_model, 0)->flags & SR_AP_FLAG_BLE) == 0);
            }
            if(kind == SrEventBleFound && expect >= 0) {
                CHECK(g_model.ap_ble == 1);
                CHECK(sr_model_recent(&g_model, 0) != NULL);
                CHECK((sr_model_recent(&g_model, 0)->flags & SR_AP_FLAG_BLE) != 0);
                CHECK(sr_model_recent(&g_model, 0)->ssid[0] == '\0');
            }
            if(kind == SrEventGps && expect >= 0) {
                CHECK(g_model.gps_blocks == 1);
                CHECK(g_model.gps.fix == true);
                CHECK(strcmp(g_model.gps.sats, "8") == 0);
            }
            if(kind == SrEventUnknown && expect >= 0) {
                CHECK(g_model.unknown_lines == 1);
                CHECK(strcmp(g_model.last_unknown, "raw-line") == 0);
            }
            if(kind == SrEventScanStarted && from == SrSessionIdle) {
                CHECK(g_model.started_tick_ms == 100u + (n - 1u));
            }
        }
    }
    CHECK(n == 24u);
    fprintf(stderr, "transition table: %u combinations\n", n);
    (void)kKindName;
    (void)kSessName;
}

static void test_stopped_restart_clears_session_not_bloom(void) {
    SrEvent ev;
    uint8_t mac[6];

    model_fresh(SrSessionRunning);
    ev_ap(&ev, SrEventApFound, "AA:BB:CC:DD:EE:FF", "KeepMe", 1, -50);
    CHECK(sr_model_apply(&g_model, &ev, 10) == true);
    CHECK(g_model.ap_wifi == 1);
    CHECK(g_model.unique_est == 1);
    CHECK(sr_bloom_unique(&g_bloom) == 1);

    ev_kind(&ev, SrEventScanStopped);
    CHECK(sr_model_apply(&g_model, &ev, 11) == true);

    ev_kind(&ev, SrEventScanStarted);
    CHECK(sr_model_apply(&g_model, &ev, 12) == true);
    CHECK(g_model.session == SrSessionRunning);
    CHECK(g_model.ap_wifi == 0);
    CHECK(g_model.unique_est == 0);
    CHECK(sr_model_recent_count(&g_model) == 0);
    CHECK(sr_model_recent(&g_model, 0) == NULL);
    CHECK(g_model.last_unknown[0] == '\0');
    /* the bloom filter is the caller's decision; apply must never implicitly clear it */
    CHECK(sr_bloom_unique(&g_bloom) == 1);
    CHECK(sr_bloom_maybe_contains(&g_bloom, "AA:BB:CC:DD:EE:FF") == true);

    ev_ap(&ev, SrEventApFound, "AA:BB:CC:DD:EE:FF", "KeepMe", 1, -50);
    CHECK(sr_model_apply(&g_model, &ev, 13) == true);
    CHECK(g_model.ap_wifi == 1);
    CHECK(g_model.unique_est == 0); /* the bloom filter has already seen it */
    CHECK(sr_mac_parse("AA:BB:CC:DD:EE:FF", mac) == true);
    CHECK(memcmp(sr_model_recent(&g_model, 0)->mac, mac, 6) == 0);

    sr_model_reset_session(&g_model, true);
    CHECK(g_model.ap_wifi == 0);
    CHECK(sr_model_recent_count(&g_model) == 0);
    CHECK(sr_bloom_unique(&g_bloom) == 0);
    CHECK(g_model.session == SrSessionRunning); /* reset does not change the state */
}

static void test_ring_wrap(void) {
    unsigned i;
    char mac[18];
    char ssid[12];
    SrEvent ev;
    const SrApBrief* b0;
    const SrApBrief* b63;

    model_fresh(SrSessionRunning);

    for(i = 1; i <= 200u; i++) {
        /*
         * the 1-based index is written into the MAC's last two bytes and into the SSID, so
         * assertions about "entry #137" are easy to make.
         */
        snprintf(mac, sizeof(mac), "02:00:00:00:%02X:%02X", (i >> 8) & 0xFF, i & 0xFF);
        snprintf(ssid, sizeof(ssid), "N%u", i);
        ev_ap(&ev, SrEventApFound, mac, ssid, (int)(i % 14u) + 1, -40 - (int)(i % 50u));
        CHECK(sr_model_apply(&g_model, &ev, 1000u + i) == true);
        if(i < 64u) {
            CHECK(sr_model_recent_count(&g_model) == (size_t)i);
        } else {
            CHECK(sr_model_recent_count(&g_model) == 64u);
        }
    }

    CHECK(sr_model_recent_count(&g_model) == 64u);
    b0 = sr_model_recent(&g_model, 0);
    b63 = sr_model_recent(&g_model, 63);
    CHECK(b0 != NULL);
    CHECK(b63 != NULL);
    CHECK(strcmp(b0->ssid, "N200") == 0);
    CHECK(strcmp(b63->ssid, "N137") == 0);
    CHECK(sr_model_recent(&g_model, 64) == NULL);

    {
        uint8_t m200[6];
        uint8_t m137[6];
        CHECK(sr_mac_parse("02:00:00:00:00:C8", m200) == true); /* 200 = 0x00C8 */
        CHECK(sr_mac_parse("02:00:00:00:00:89", m137) == true); /* 137 = 0x0089 */
        CHECK(memcmp(b0->mac, m200, 6) == 0);
        CHECK(memcmp(b63->mac, m137, 6) == 0);
    }
}

static void test_borrowed_view_not_stored(void) {
    char src[64];
    SrEvent ev;
    size_t n;

    model_fresh(SrSessionRunning);
    memset(src, 0, sizeof(src));
    n = sr_strlcpy(src, sizeof(src), "ADR-010-borrow-check");
    ev_clear(&ev);
    ev.kind = SrEventUnknown;
    ev.u.unknown.text = src;
    ev.u.unknown.len = n;

    CHECK(sr_model_apply(&g_model, &ev, 50) == true);
    CHECK(strcmp(g_model.last_unknown, "ADR-010-borrow-check") == 0);
    CHECK(g_model.last_unknown_len == n);
    CHECK(g_model.unknown_lines == 1);

    /* once the source buffer is overwritten, the model must still hold the original copy. */
    memset(src, 0xAA, sizeof(src));
    CHECK(strcmp(g_model.last_unknown, "ADR-010-borrow-check") == 0);
    CHECK((unsigned char)g_model.last_unknown[0] != 0xAAu);
    CHECK(g_model.last_unknown_len == n);
    CHECK(g_model.last_unknown[n] == '\0');
}

/*
 * A window view (has a len, no NUL) must never be read out of bounds -- added by the lead session
 * on 2026-08-16 during the T2.4 review.
 *
 * SrRawView carries a len field, and by its semantics it is allowed to be a window into the
 * middle of a line rather than a C string. The first version of apply_unknown went through
 * sr_strlcpy, which calls strlen(text) first and reads past the end of the buffer; a plain make
 * run stays green regardless (the garbage bytes it reads do not affect any assertion), only ASan
 * catches it. Here we use malloc for an exact, tight allocation so an out-of-bounds read is
 * guaranteed to land in the red zone. **This test is only meaningful under make asan.**
 */
static void test_window_view_no_overread(void) {
    enum { N = 8 };
    char* buf = (char*)malloc(N); /* exactly N bytes, no room for a NUL */
    SrEvent ev;

    CHECK(buf != NULL);
    if(buf == NULL) {
        return;
    }
    memcpy(buf, "ABCDEFGH", (size_t)N);

    model_fresh(SrSessionRunning);
    ev_clear(&ev);
    ev.kind = SrEventUnknown;
    ev.u.unknown.text = buf;
    ev.u.unknown.len = (size_t)N;

    CHECK(sr_model_apply(&g_model, &ev, 60) == true);
    CHECK(g_model.last_unknown_len == (size_t)N);
    CHECK(memcmp(g_model.last_unknown, "ABCDEFGH", (size_t)N) == 0);
    CHECK(g_model.last_unknown[N] == '\0');
    CHECK(g_model.unknown_lines == 1);

    /*
     * when len is shorter than the actual content, only the windowed portion is taken -- the
     * bytes past it must never leak out.
     */
    model_fresh(SrSessionRunning);
    ev.u.unknown.len = 3u;
    CHECK(sr_model_apply(&g_model, &ev, 61) == true);
    CHECK(g_model.last_unknown_len == 3u);
    CHECK(strcmp(g_model.last_unknown, "ABC") == 0);

    free(buf);
}

static void test_stats_gps_malformed(void) {
    SrEvent ev;
    const SrApBrief* b;

    model_fresh(SrSessionIdle);

    ev_clear(&ev);
    ev.kind = SrEventGps;
    ev.u.gps.fix = true;
    sr_strlcpy(ev.u.gps.lat, sizeof(ev.u.gps.lat), "1.0");
    CHECK(sr_model_apply(&g_model, &ev, 3) == true);
    CHECK(g_model.gps.fix == true);
    CHECK(g_model.gps_blocks == 1);

    ev_kind(&ev, SrEventScanStarted);
    CHECK(sr_model_apply(&g_model, &ev, 4) == true);

    ev_ap(&ev, SrEventApFound, "AA:BB:CC:DD:EE:FF", "HasFix", 11, -30);
    sr_strlcpy(ev.u.ap.datetime, sizeof(ev.u.ap.datetime), "2026-01-01 00:00:00");
    sr_strlcpy(ev.u.ap.lat, sizeof(ev.u.ap.lat), "31.2300000");
    CHECK(sr_model_apply(&g_model, &ev, 5) == true);
    b = sr_model_recent(&g_model, 0);
    CHECK(b != NULL);
    CHECK((b->flags & SR_AP_FLAG_GPS) != 0);
    CHECK((b->flags & SR_AP_FLAG_BLE) == 0);
    CHECK(g_model.with_gps_fix == 1);
    CHECK(g_model.unique_est == 1);

    ev_ap(&ev, SrEventApFound, "not-a-mac", "Bad", 1, -20);
    CHECK(sr_model_apply(&g_model, &ev, 6) == true);
    CHECK(g_model.malformed_lines == 1);
    CHECK(g_model.ap_wifi == 1); /* a bad MAC never enters the ring and is not counted as an AP */
    CHECK(sr_model_recent_count(&g_model) == 1);

    ev_ap(&ev, SrEventBleFound, "11:22:33:44:55:66", "", 0, -90);
    sr_strlcpy(ev.u.ble.datetime, sizeof(ev.u.ble.datetime), "2026-01-01 00:00:00");
    sr_strlcpy(ev.u.ble.lat, sizeof(ev.u.ble.lat), "31.2300000");
    CHECK(sr_model_apply(&g_model, &ev, 7) == true);
    CHECK(g_model.ap_ble == 1);
    b = sr_model_recent(&g_model, 0);
    CHECK(b != NULL);
    CHECK((b->flags & SR_AP_FLAG_BLE) != 0);
    CHECK((b->flags & SR_AP_FLAG_GPS) != 0);
    CHECK(b->ssid[0] == '\0');
    CHECK(b->channel == 0);
    CHECK(g_model.with_gps_fix == 2);

    ev_clear(&ev);
    ev.kind = SrEventUnknown;
    ev.u.unknown.text = NULL;
    ev.u.unknown.len = 0;
    CHECK(sr_model_apply(&g_model, &ev, 8) == true);
    CHECK(g_model.malformed_lines == 2);
    CHECK(g_model.unknown_lines == 0);

    /* out-of-range RSSI / channel get clamped, but the AP still counts as legal */
    ev_ap(&ev, SrEventApFound, "02:00:00:00:00:03", "Clamp", 300, -200);
    CHECK(sr_model_apply(&g_model, &ev, 9) == true);
    b = sr_model_recent(&g_model, 0);
    CHECK(b != NULL);
    CHECK(b->rssi == (int8_t)-128);
    CHECK(b->channel == 255);

    /* an overlong SSID is narrowed down to 24 characters + NUL */
    {
        char long_ssid[40];
        memset(long_ssid, 'Z', 39);
        long_ssid[39] = '\0';
        ev_ap(&ev, SrEventApFound, "02:00:00:00:00:04", long_ssid, 1, -10);
        CHECK(sr_model_apply(&g_model, &ev, 10) == true);
        b = sr_model_recent(&g_model, 0);
        CHECK(b != NULL);
        CHECK(strlen(b->ssid) == 24);
        CHECK(b->ssid[24] == '\0');
    }

    /* must not crash even without a bloom filter */
    sr_model_init(&g_model, NULL, NULL);
    ev_kind(&ev, SrEventScanStarted);
    CHECK(sr_model_apply(&g_model, &ev, 11) == true);
    ev_ap(&ev, SrEventApFound, "02:00:00:00:00:05", "NoBloom", 2, -15);
    CHECK(sr_model_apply(&g_model, &ev, 12) == true);
    CHECK(g_model.unique_est == 0);
    CHECK(g_model.ap_wifi == 1);
}

static void test_oracle_agreement_scripted(void) {
    SrEvent ev;
    char src[32];

    sr_bloom_init(&g_bloom);
    sr_bloom_init(&g_bloom_b);
    sr_model_init(&g_model, &g_bloom, NULL);
    oracle_init(&g_oracle, &g_bloom_b);

    ev_kind(&ev, SrEventScanStarted);
    CHECK(sr_model_apply(&g_model, &ev, 1) == oracle_apply(&g_oracle, &ev, 1));
    check_model_matches_oracle(&g_model, &g_oracle);

    ev_clear(&ev);
    ev.kind = SrEventGps;
    ev.u.gps.fix = false;
    CHECK(sr_model_apply(&g_model, &ev, 2) == oracle_apply(&g_oracle, &ev, 2));
    check_model_matches_oracle(&g_model, &g_oracle);

    ev_clear(&ev);
    ev.kind = SrEventGps;
    ev.u.gps.fix = true;
    CHECK(sr_model_apply(&g_model, &ev, 3) == oracle_apply(&g_oracle, &ev, 3));

    ev_ap(&ev, SrEventApFound, "aa:bb:cc:dd:ee:ff", "Case", 6, -60);
    CHECK(sr_model_apply(&g_model, &ev, 4) == oracle_apply(&g_oracle, &ev, 4));
    ev_ap(&ev, SrEventApFound, "AA:BB:CC:DD:EE:FF", "Case", 6, -60);
    CHECK(sr_model_apply(&g_model, &ev, 5) == oracle_apply(&g_oracle, &ev, 5));
    CHECK(g_model.unique_est == 1);

    ev_ap(&ev, SrEventBleFound, "11:22:33:44:55:66", "", 0, -88);
    CHECK(sr_model_apply(&g_model, &ev, 6) == oracle_apply(&g_oracle, &ev, 6));

    sr_strlcpy(src, sizeof(src), "hello-raw");
    ev_clear(&ev);
    ev.kind = SrEventUnknown;
    ev.u.unknown.text = src;
    ev.u.unknown.len = 9;
    CHECK(sr_model_apply(&g_model, &ev, 7) == oracle_apply(&g_oracle, &ev, 7));
    memset(src, 0xAA, sizeof(src));
    CHECK(strcmp(g_model.last_unknown, "hello-raw") == 0);
    CHECK(strcmp(g_oracle.last_unknown, "hello-raw") == 0);

    ev_kind(&ev, SrEventScanStarted); /* illegal while running */
    CHECK(sr_model_apply(&g_model, &ev, 8) == oracle_apply(&g_oracle, &ev, 8));
    ev_kind(&ev, SrEventScanStopped);
    CHECK(sr_model_apply(&g_model, &ev, 9) == oracle_apply(&g_oracle, &ev, 9));
    ev_kind(&ev, SrEventScanStopped); /* illegal */
    CHECK(sr_model_apply(&g_model, &ev, 10) == oracle_apply(&g_oracle, &ev, 10));
    ev_kind(&ev, SrEventScanStarted);
    CHECK(sr_model_apply(&g_model, &ev, 11) == oracle_apply(&g_oracle, &ev, 11));

    check_model_matches_oracle(&g_model, &g_oracle);
}

/* -------------------------------------------------------------------------- */
/* Fuzz: the seed is hardcoded. Coverage counters must be able to distinguish */
/* complementary branches.                                                    */
/* -------------------------------------------------------------------------- */

#define SR_MODEL_FUZZ_ITERS 8000u
#define SR_MODEL_FUZZ_SEED 0x7D2404E1u

static uint32_t xs32(uint32_t* s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

static void mac_fmt(char* dst, uint8_t prefix, uint32_t id, int lower) {
    static const char H[] = "0123456789ABCDEF";
    static const char L[] = "0123456789abcdef";
    const char* h = lower ? L : H;
    uint8_t b[6];
    size_t i;
    size_t p = 0;

    b[0] = prefix;
    b[1] = (uint8_t)(id >> 24);
    b[2] = (uint8_t)(id >> 16);
    b[3] = (uint8_t)(id >> 8);
    b[4] = (uint8_t)id;
    b[5] = (uint8_t)(id * 0x9Eu + prefix);
    for(i = 0; i < 6; i++) {
        if(i != 0) {
            dst[p++] = ':';
        }
        dst[p++] = h[b[i] >> 4];
        dst[p++] = h[b[i] & 0x0Fu];
    }
    dst[p] = '\0';
}

static void test_fuzz(void) {
    uint32_t seed = SR_MODEL_FUZZ_SEED;
    uint32_t iter;
    uint32_t cov_idle_run = 0, cov_run_stop = 0, cov_stop_run = 0;
    uint32_t cov_ill_idle_stop = 0, cov_ill_run_start = 0, cov_ill_stop_stop = 0;
    uint32_t cov_ap_wifi = 0, cov_ap_ble = 0, cov_ap_bad = 0;
    uint32_t cov_gps_fix = 0, cov_gps_nofix = 0;
    uint32_t cov_unknown = 0, cov_unk_null = 0;
    uint32_t cov_fw = 0, cov_none = 0;
    uint32_t cov_ring_grow = 0, cov_ring_full = 0;
    uint32_t cov_apply_yes = 0, cov_apply_no = 0;
    uint32_t cov_mac_lower = 0, cov_mac_upper = 0;
    uint32_t cov_ssid_empty = 0, cov_ssid_long = 0;
    uint32_t cov_oracle = 0;
    int reported = 0;
    char unkbuf[48];

    sr_bloom_init(&g_bloom);
    sr_bloom_init(&g_bloom_b);
    sr_model_init(&g_model, &g_bloom, NULL);
    oracle_init(&g_oracle, &g_bloom_b);

    /*
     * Idle cannot be reached back from Running/Stopped. Hit two Idle edges before entering the
     * main loop.
     */
    {
        SrEvent ev;
        ev_kind(&ev, SrEventScanStopped);
        CHECK(sr_model_apply(&g_model, &ev, 0) == oracle_apply(&g_oracle, &ev, 0));
        CHECK(sr_model_apply(&g_model, &ev, 0) == oracle_apply(&g_oracle, &ev, 0));
        cov_ill_idle_stop += 2;
        ev_kind(&ev, SrEventScanStarted);
        CHECK(sr_model_apply(&g_model, &ev, 0) == oracle_apply(&g_oracle, &ev, 0));
        cov_idle_run += 1;
    }

    for(iter = 0; iter < SR_MODEL_FUZZ_ITERS; iter++) {
        SrEvent ev;
        SrSessionState before = g_model.session;
        uint32_t kind_roll;
        SrEventKind kind;
        bool impl;
        bool ref;
        bool mac_ok = false;

        ev_clear(&ev);
        /*
         * Every 250 iterations, insert one round of state edges, with data in between so the
         * ring buffer gets to wrap around fully.
         * 0 = illegal Start (if Running) / 1 = Stop / 2 = illegal Stop (if Stopped) / 3 = Start.
         */
        if((iter % 250u) < 4u) {
            uint32_t slot = iter % 250u;
            if(slot == 0u) {
                kind = SrEventScanStarted;
            } else if(slot == 1u) {
                kind = SrEventScanStopped;
            } else if(slot == 2u) {
                kind = SrEventScanStopped;
            } else {
                kind = SrEventScanStarted;
            }
        } else {
            kind_roll = xs32(&seed) % 10u;
            if(kind_roll <= 3u) {
                kind = SrEventApFound;
            } else if(kind_roll == 4u) {
                kind = SrEventBleFound;
            } else if(kind_roll == 5u) {
                kind = SrEventGps;
            } else if(kind_roll == 6u) {
                kind = SrEventUnknown;
            } else if(kind_roll == 7u) {
                kind = SrEventFirmware;
            } else {
                kind = SrEventNone;
            }
        }
        ev.kind = kind;

        if(kind == SrEventApFound || kind == SrEventBleFound) {
            char mac[18];
            char ssid[40];
            uint32_t id = xs32(&seed);
            int lower = (int)(xs32(&seed) & 1u);
            uint32_t bad = xs32(&seed) % 7u;
            SrApRecord* r = (kind == SrEventBleFound) ? &ev.u.ble : &ev.u.ap;
            uint8_t parsed[6];

            if(bad == 0) {
                sr_strlcpy(r->bssid, sizeof(r->bssid), "xx:not:mac");
            } else {
                mac_fmt(mac, (uint8_t)(0x02u + (id & 3u)), id, lower);
                sr_strlcpy(r->bssid, sizeof(r->bssid), mac);
                if(lower) {
                    cov_mac_lower++;
                } else {
                    cov_mac_upper++;
                }
            }
            mac_ok = oracle_mac(r->bssid, parsed);
            if((xs32(&seed) % 5u) == 0) {
                ssid[0] = '\0';
                cov_ssid_empty++;
            } else if((xs32(&seed) % 5u) == 0) {
                memset(ssid, 'Q', 36);
                ssid[36] = '\0';
                cov_ssid_long++;
            } else {
                snprintf(ssid, sizeof(ssid), "S%u", id & 0xFFFFu);
            }
            sr_strlcpy(r->ssid, sizeof(r->ssid), ssid);
            r->channel = (int)(xs32(&seed) % 20u);
            r->rssi = -30 - (int)(xs32(&seed) % 80u);
        } else if(kind == SrEventGps) {
            ev.u.gps.fix = (xs32(&seed) & 1u) != 0;
            if(ev.u.gps.fix) {
                cov_gps_fix++;
            } else {
                cov_gps_nofix++;
            }
        } else if(kind == SrEventUnknown) {
            if((xs32(&seed) % 11u) == 0) {
                ev.u.unknown.text = NULL;
                ev.u.unknown.len = 0;
                cov_unk_null++;
            } else {
                snprintf(unkbuf, sizeof(unkbuf), "U%u", iter);
                ev.u.unknown.text = unkbuf;
                ev.u.unknown.len = strlen(unkbuf);
                cov_unknown++;
            }
        }

        impl = sr_model_apply(&g_model, &ev, iter);
        ref = oracle_apply(&g_oracle, &ev, iter);
        if(impl != ref) {
            sr_test_failures++;
            if(reported < 5) {
                fprintf(
                    stderr,
                    "fuzz apply-return mismatch: iter=%u seed=0x%08X kind=%u\n",
                    iter,
                    SR_MODEL_FUZZ_SEED,
                    (unsigned)kind);
                reported++;
            }
        } else {
            cov_oracle++;
        }

        if(impl) {
            cov_apply_yes++;
        } else {
            cov_apply_no++;
        }

        if(kind == SrEventScanStarted && before == SrSessionIdle && g_model.session == SrSessionRunning) {
            cov_idle_run++;
        } else if(kind == SrEventScanStopped && before == SrSessionRunning && g_model.session == SrSessionStopped) {
            cov_run_stop++;
        } else if(kind == SrEventScanStarted && before == SrSessionStopped && g_model.session == SrSessionRunning) {
            cov_stop_run++;
        } else if(kind == SrEventScanStopped && before == SrSessionIdle) {
            cov_ill_idle_stop++;
        } else if(kind == SrEventScanStarted && before == SrSessionRunning) {
            cov_ill_run_start++;
        } else if(kind == SrEventScanStopped && before == SrSessionStopped) {
            cov_ill_stop_stop++;
        }

        if(kind == SrEventApFound || kind == SrEventBleFound) {
            if(!mac_ok) {
                cov_ap_bad++;
            } else if(kind == SrEventApFound) {
                cov_ap_wifi++;
            } else {
                cov_ap_ble++;
            }
        }
        if(kind == SrEventFirmware) {
            cov_fw++;
        }
        if(kind == SrEventNone) {
            cov_none++;
        }
        if(sr_model_recent_count(&g_model) < (size_t)SR_RECENT_CAP) {
            cov_ring_grow++;
        } else {
            cov_ring_full++;
        }

        /*
         * compare last_unknown only after the source buffer has been overwritten, so fuzzing
         * cannot miss a "stored the pointer" regression.
         */
        if(kind == SrEventUnknown && ev.u.unknown.text == unkbuf) {
            char expect[48];
            sr_strlcpy(expect, sizeof(expect), unkbuf);
            memset(unkbuf, 0xAA, sizeof(unkbuf));
            if(strcmp(g_model.last_unknown, expect) != 0) {
                sr_test_failures++;
                if(reported < 5) {
                    fprintf(stderr, "fuzz borrowed-view leak: iter=%u\n", iter);
                    reported++;
                }
            }
        }

        if((iter % 400u) == 399u) {
            check_model_matches_oracle(&g_model, &g_oracle);
        }
    }

    check_model_matches_oracle(&g_model, &g_oracle);

    fprintf(
        stderr,
        "model fuzz coverage: idle_run=%u run_stop=%u stop_run=%u "
        "ill_idle_stop=%u ill_run_start=%u ill_stop_stop=%u "
        "ap_wifi=%u ap_ble=%u ap_bad=%u gps_fix=%u gps_nofix=%u "
        "unk=%u unk_null=%u fw=%u none=%u ring_grow=%u ring_full=%u "
        "yes=%u no=%u mac_lo=%u mac_hi=%u ssid_empty=%u ssid_long=%u oracle=%u\n",
        cov_idle_run,
        cov_run_stop,
        cov_stop_run,
        cov_ill_idle_stop,
        cov_ill_run_start,
        cov_ill_stop_stop,
        cov_ap_wifi,
        cov_ap_ble,
        cov_ap_bad,
        cov_gps_fix,
        cov_gps_nofix,
        cov_unknown,
        cov_unk_null,
        cov_fw,
        cov_none,
        cov_ring_grow,
        cov_ring_full,
        cov_apply_yes,
        cov_apply_no,
        cov_mac_lower,
        cov_mac_upper,
        cov_ssid_empty,
        cov_ssid_long,
        cov_oracle);

    /*
     * Coverage floor. Seed 0x7D2404E1 is hardcoded => the numbers are deterministic.
     * Measured (8000 iter): idle_run=1 run_stop=32 stop_run=32
     * ill_idle_stop=2 ill_run_start=32 ill_stop_stop=32
     * ap_wifi=2787 ap_ble=647 ap_bad=523 gps_fix=388 gps_nofix=385
     * unk=700 unk_null=80 fw=820 none=1542 ring_grow=4665 ring_full=3335
     * yes=5574 no=2426 mac_lo=1739 mac_hi=1695 ssid_empty=786 ssid_long=626
     * oracle=8000.
     *
     * Idle can never be reached back from Running/Stopped; idle_run / ill_idle_stop only get
     * forced once/twice before entering the loop, so the threshold can only be >0 -- not the kind
     * of "always equal to the iteration count" that tests nothing.
     * gps_fix vs gps_nofix, unk vs unk_null, ring_grow vs ring_full, and yes vs no must each be
     * counted separately; folding them into one counter that is always 8000 is not allowed.
     */
    CHECK(cov_idle_run > 0u);
    CHECK(cov_run_stop > 10u);
    CHECK(cov_stop_run > 10u);
    CHECK(cov_ill_idle_stop > 0u);
    CHECK(cov_ill_run_start > 10u);
    CHECK(cov_ill_stop_stop > 10u);
    CHECK(cov_ap_wifi > 900u);
    CHECK(cov_ap_ble > 200u);
    CHECK(cov_ap_bad > 170u);
    CHECK(cov_gps_fix > 120u);
    CHECK(cov_gps_nofix > 120u);
    CHECK(cov_unknown > 230u);
    CHECK(cov_unk_null > 25u);
    CHECK(cov_fw > 270u);
    CHECK(cov_none > 500u);
    CHECK(cov_ring_grow > 1500u);
    CHECK(cov_ring_full > 1100u);
    CHECK(cov_apply_yes > 1800u);
    CHECK(cov_apply_no > 800u);
    CHECK(cov_mac_lower > 570u);
    CHECK(cov_mac_upper > 560u);
    CHECK(cov_ssid_empty > 260u);
    CHECK(cov_ssid_long > 200u);
    CHECK(cov_oracle == SR_MODEL_FUZZ_ITERS);
}

static void test_session_rev(void) {
    SrEvent ev;
    uint32_t illegal;

    sr_bloom_init(&g_bloom);
    sr_model_init(&g_model, &g_bloom, NULL);
    CHECK(g_model.session_rev == 0);
    CHECK(g_model.session == SrSessionIdle);

    ev_kind(&ev, SrEventScanStarted);
    CHECK(sr_model_apply(&g_model, &ev, 10) == true);
    CHECK(g_model.session_rev == 1);
    CHECK(g_model.session == SrSessionRunning);

    ev_kind(&ev, SrEventScanStopped);
    CHECK(sr_model_apply(&g_model, &ev, 20) == true);
    CHECK(g_model.session_rev == 2);
    CHECK(g_model.session == SrSessionStopped);

    /* Stopped→Running goes through reset_session; rev must not be cleared. */
    ev_kind(&ev, SrEventScanStarted);
    CHECK(sr_model_apply(&g_model, &ev, 30) == true);
    CHECK(g_model.session_rev == 3);
    CHECK(g_model.session == SrSessionRunning);

    illegal = g_model.illegal_trans;
    ev_kind(&ev, SrEventScanStarted);
    CHECK(sr_model_apply(&g_model, &ev, 40) == false);
    CHECK(g_model.illegal_trans == illegal + 1u);
    CHECK(g_model.session_rev == 3);

    sr_model_init(&g_model, &g_bloom, NULL);
    CHECK(g_model.session == SrSessionIdle);
    CHECK(g_model.session_rev == 0);
    illegal = g_model.illegal_trans;
    ev_kind(&ev, SrEventScanStopped);
    CHECK(sr_model_apply(&g_model, &ev, 1) == false);
    CHECK(g_model.illegal_trans == illegal + 1u);
    CHECK(g_model.session_rev == 0);

    ev_kind(&ev, SrEventScanStarted);
    CHECK(sr_model_apply(&g_model, &ev, 2) == true);
    CHECK(g_model.session_rev == 1);
    sr_model_reset_session(&g_model, true);
    CHECK(g_model.session_rev == 1);

    fprintf(stderr, "sizeof(SrModel)=%zu\n", sizeof(SrModel));
    /* After T4.6 added SrRawLog*, 3176. D12 added SrGpsCsvView + gps_csv_rev → 3328. */
    CHECK(sizeof(SrModel) == 3328);
    CHECK(sizeof(SrModel) <= 4096);
}

static void ev_stop_reason(SrEvent* ev, SrStopReason reason) {
    ev_clear(ev);
    ev->kind = SrEventScanStopped;
    ev->u.stop = reason;
}

static void test_stop_reason(void) {
    unsigned run_stop = 0;
    unsigned ill_keep = 0;
    unsigned gps_ack = 0;
    unsigned total = 0;
    uint32_t illegal0;
    uint32_t grev0;
    uint32_t srev0;
    SrEvent ev;
    unsigned ri;
    unsigned si;
    static const SrStopReason k_all[3] = {
        SrStopWifiTranRecv,
        SrStopEndNmea,
        SrStopGpsUpdates,
    };
    static const SrStopReason k_gps[2] = {
        SrStopGpsUpdates,
        SrStopEndNmea,
    };
    static const SrSessionState k_idleish[2] = {
        SrSessionIdle,
        SrSessionStopped,
    };

    for(ri = 0; ri < 3u; ri++) {
        model_fresh(SrSessionRunning);
        illegal0 = g_model.illegal_trans;
        grev0 = g_model.gps_stop_rev;
        srev0 = g_model.session_rev;
        ev_stop_reason(&ev, k_all[ri]);
        CHECK(sr_model_apply(&g_model, &ev, 50) == true);
        CHECK(g_model.session == SrSessionStopped);
        CHECK(g_model.session_rev == srev0 + 1u);
        CHECK(g_model.gps_stop_rev == grev0);
        CHECK(g_model.illegal_trans == illegal0);
        if(g_model.session == SrSessionStopped && g_model.session_rev == srev0 + 1u) {
            run_stop++;
        } else if(g_model.gps_stop_rev == grev0 + 1u) {
            gps_ack++;
        } else if(g_model.illegal_trans == illegal0 + 1u) {
            ill_keep++;
        }
        total++;
    }

    for(si = 0; si < 2u; si++) {
        model_fresh(k_idleish[si]);
        illegal0 = g_model.illegal_trans;
        grev0 = g_model.gps_stop_rev;
        srev0 = g_model.session_rev;
        ev_stop_reason(&ev, SrStopWifiTranRecv);
        CHECK(sr_model_apply(&g_model, &ev, 60) == false);
        CHECK(g_model.session == k_idleish[si]);
        CHECK(g_model.session_rev == srev0);
        CHECK(g_model.gps_stop_rev == grev0);
        CHECK(g_model.illegal_trans == illegal0 + 1u);
        if(g_model.session == SrSessionStopped && g_model.session_rev == srev0 + 1u) {
            run_stop++;
        } else if(g_model.gps_stop_rev == grev0 + 1u) {
            gps_ack++;
        } else if(g_model.illegal_trans == illegal0 + 1u) {
            ill_keep++;
        }
        total++;
    }

    for(si = 0; si < 2u; si++) {
        for(ri = 0; ri < 2u; ri++) {
            model_fresh(k_idleish[si]);
            illegal0 = g_model.illegal_trans;
            grev0 = g_model.gps_stop_rev;
            srev0 = g_model.session_rev;
            ev_stop_reason(&ev, k_gps[ri]);
            CHECK(sr_model_apply(&g_model, &ev, 70) == false);
            CHECK(g_model.session == k_idleish[si]);
            CHECK(g_model.session_rev == srev0);
            CHECK(g_model.gps_stop_rev == grev0 + 1u);
            CHECK(g_model.illegal_trans == illegal0);
            if(g_model.session == SrSessionStopped && g_model.session_rev == srev0 + 1u) {
                run_stop++;
            } else if(g_model.gps_stop_rev == grev0 + 1u) {
                gps_ack++;
            } else if(g_model.illegal_trans == illegal0 + 1u) {
                ill_keep++;
            }
            total++;
        }
    }

    CHECK(g_model.gps_stop_rev > 0u);
    grev0 = g_model.gps_stop_rev;
    sr_model_reset_session(&g_model, false);
    CHECK(g_model.gps_stop_rev == grev0);

    printf(
        "stop_reason cover: run_stop=%u ill_keep=%u gps_ack=%u total=%u\n",
        run_stop,
        ill_keep,
        gps_ack,
        total);
    CHECK(run_stop == 3);
    CHECK(ill_keep == 2);
    CHECK(gps_ack == 4);
    CHECK(total == 9);
}

/* -------------------------------------------------------------------------- */
/* D12 Part B: CSV-row GPS (F6/F7/F8)                                         */
/* -------------------------------------------------------------------------- */

static void test_gps_csv_rows(void) {
    SrEvent ev;
    const SrApBrief* b;
    uint32_t grev;
    uint32_t fix0;
    uint32_t rev0;
    SrGpsSnapshot gps_snap;
    SrGpsCsvView csv_zero;

    memset(&csv_zero, 0, sizeof(csv_zero));

    /* gpsdata block must not drive FLAG_GPS / with_gps_fix (the D12 bug). */
    model_fresh(SrSessionIdle);
    ev_clear(&ev);
    ev.kind = SrEventGps;
    ev.u.gps.fix = true;
    sr_strlcpy(ev.u.gps.lat, sizeof(ev.u.gps.lat), "1.0");
    CHECK(sr_model_apply(&g_model, &ev, 1) == true);
    CHECK(g_model.gps.fix == true);
    CHECK(g_model.gps_blocks == 1);
    CHECK(g_model.gps_csv_rev == 0);

    ev_ap(&ev, SrEventApFound, "AA:BB:CC:DD:EE:00", "NoDt", 6, -40);
    sr_strlcpy(ev.u.ap.lat, sizeof(ev.u.ap.lat), "0.0000000");
    CHECK(sr_model_apply(&g_model, &ev, 2) == true);
    b = sr_model_recent(&g_model, 0);
    CHECK(b != NULL);
    CHECK((b->flags & SR_AP_FLAG_GPS) == 0);
    CHECK(g_model.with_gps_fix == 0);
    CHECK(g_model.gps_csv_rev == 1);
    CHECK(g_model.gps_csv.fix == false);
    CHECK(strcmp(g_model.gps_csv.lat, "0.0000000") == 0);
    CHECK(g_model.gps_blocks == 1);

    /* Fresh session for the row-level cases. gps_blocks must stay 0 throughout. */
    model_fresh(SrSessionIdle);
    CHECK(g_model.gps_blocks == 0);
    CHECK(g_model.gps_csv_rev == 0);

    /* WiFi with fix: datetime non-empty, lat/lon non-zero. */
    ev_ap(&ev, SrEventApFound, "AA:BB:CC:DD:EE:01", "FixYes", 6, -50);
    sr_strlcpy(ev.u.ap.datetime, sizeof(ev.u.ap.datetime), "2026-09-03 12:00:00");
    sr_strlcpy(ev.u.ap.lat, sizeof(ev.u.ap.lat), "31.2300000");
    sr_strlcpy(ev.u.ap.lon, sizeof(ev.u.ap.lon), "121.4700000");
    sr_strlcpy(ev.u.ap.alt, sizeof(ev.u.ap.alt), "12.00");
    sr_strlcpy(ev.u.ap.acc, sizeof(ev.u.ap.acc), "3.50");
    CHECK(sr_model_apply(&g_model, &ev, 10) == true);
    CHECK(g_model.with_gps_fix == 1);
    CHECK(g_model.gps_csv.fix == true);
    CHECK(g_model.gps_csv_rev == 1);
    CHECK(strcmp(g_model.gps_csv.lat, "31.2300000") == 0);
    CHECK(strcmp(g_model.gps_csv.lon, "121.4700000") == 0);
    CHECK(strcmp(g_model.gps_csv.datetime, "2026-09-03 12:00:00") == 0);
    b = sr_model_recent(&g_model, 0);
    CHECK(b != NULL);
    CHECK((b->flags & SR_AP_FLAG_GPS) != 0);
    CHECK((b->flags & SR_AP_FLAG_BLE) == 0);
    CHECK(g_model.gps_blocks == 0);

    /* WiFi without fix: empty datetime, lat/lon 0.0000000. rev still +1. */
    fix0 = g_model.with_gps_fix;
    rev0 = g_model.gps_csv_rev;
    ev_ap(&ev, SrEventApFound, "AA:BB:CC:DD:EE:02", "FixNo", 11, -60);
    ev.u.ap.datetime[0] = '\0';
    sr_strlcpy(ev.u.ap.lat, sizeof(ev.u.ap.lat), "0.0000000");
    sr_strlcpy(ev.u.ap.lon, sizeof(ev.u.ap.lon), "0.0000000");
    sr_strlcpy(ev.u.ap.alt, sizeof(ev.u.ap.alt), "0.00");
    sr_strlcpy(ev.u.ap.acc, sizeof(ev.u.ap.acc), "0.00");
    CHECK(sr_model_apply(&g_model, &ev, 11) == true);
    CHECK(g_model.with_gps_fix == fix0);
    CHECK(g_model.gps_csv.fix == false);
    CHECK(g_model.gps_csv_rev == rev0 + 1u);
    CHECK(strcmp(g_model.gps_csv.lat, "0.0000000") == 0);
    b = sr_model_recent(&g_model, 0);
    CHECK(b != NULL);
    CHECK((b->flags & SR_AP_FLAG_GPS) == 0);
    CHECK(g_model.gps_blocks == 0);

    /* BLE with fix. */
    ev_ap(&ev, SrEventBleFound, "11:22:33:44:55:01", "BleFix", 0, -70);
    sr_strlcpy(ev.u.ble.datetime, sizeof(ev.u.ble.datetime), "2026-09-03 12:00:01");
    sr_strlcpy(ev.u.ble.lat, sizeof(ev.u.ble.lat), "31.2310000");
    sr_strlcpy(ev.u.ble.lon, sizeof(ev.u.ble.lon), "121.4710000");
    CHECK(sr_model_apply(&g_model, &ev, 12) == true);
    CHECK(g_model.with_gps_fix == fix0 + 1u);
    CHECK(g_model.gps_csv.fix == true);
    CHECK(g_model.gps_csv_rev == rev0 + 2u);
    CHECK(strcmp(g_model.gps_csv.lat, "31.2310000") == 0);
    b = sr_model_recent(&g_model, 0);
    CHECK(b != NULL);
    CHECK((b->flags & SR_AP_FLAG_BLE) != 0);
    CHECK((b->flags & SR_AP_FLAG_GPS) != 0);
    CHECK(g_model.gps_blocks == 0);

    /* BLE without fix. rev still +1, flag not set. */
    ev_ap(&ev, SrEventBleFound, "11:22:33:44:55:02", "", 0, -80);
    sr_strlcpy(ev.u.ble.lat, sizeof(ev.u.ble.lat), "0.0000000");
    sr_strlcpy(ev.u.ble.lon, sizeof(ev.u.ble.lon), "0.0000000");
    CHECK(sr_model_apply(&g_model, &ev, 13) == true);
    CHECK(g_model.with_gps_fix == fix0 + 1u);
    CHECK(g_model.gps_csv.fix == false);
    CHECK(g_model.gps_csv_rev == rev0 + 3u);
    CHECK(strcmp(g_model.gps_csv.lat, "0.0000000") == 0);
    b = sr_model_recent(&g_model, 0);
    CHECK(b != NULL);
    CHECK((b->flags & SR_AP_FLAG_GPS) == 0);
    CHECK((b->flags & SR_AP_FLAG_BLE) != 0);
    CHECK(g_model.gps_blocks == 0);

    /* reset_session clears gps_csv / gps_csv_rev; gps and gps_stop_rev stay. */
    ev_clear(&ev);
    ev.kind = SrEventGps;
    ev.u.gps.fix = true;
    sr_strlcpy(ev.u.gps.lat, sizeof(ev.u.gps.lat), "9.9");
    CHECK(sr_model_apply(&g_model, &ev, 14) == true);
    gps_snap = g_model.gps;
    grev = g_model.gps_stop_rev;
    CHECK(g_model.gps_csv_rev != 0);
    sr_model_reset_session(&g_model, false);
    CHECK(g_model.gps_csv_rev == 0);
    CHECK(memcmp(&g_model.gps_csv, &csv_zero, sizeof(csv_zero)) == 0);
    CHECK(memcmp(&g_model.gps, &gps_snap, sizeof(gps_snap)) == 0);
    CHECK(g_model.gps_stop_rev == grev);
}

static char* gps_csv_load_fixture(const char* name, size_t expect, size_t* out_n) {
    char path_a[256];
    char path_b[256];
    FILE* f = NULL;
    long sz;
    char* buf;
    size_t n;

    snprintf(path_a, sizeof(path_a), "fixtures/%s", name);
    snprintf(path_b, sizeof(path_b), "tools/host_test/fixtures/%s", name);
    f = fopen(path_a, "rb");
    if(f == NULL) {
        f = fopen(path_b, "rb");
    }
    if(f == NULL) {
        fprintf(stderr, "fixture open failed, tried: %s ; %s\n", path_a, path_b);
        CHECK(0);
        *out_n = 0;
        return NULL;
    }
    if(fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        CHECK(0);
        *out_n = 0;
        return NULL;
    }
    sz = ftell(f);
    if(sz < 0) {
        fclose(f);
        CHECK(0);
        *out_n = 0;
        return NULL;
    }
    n = (size_t)sz;
    CHECK(n == expect);
    if(n != expect) {
        fclose(f);
        *out_n = 0;
        return NULL;
    }
    rewind(f);
    buf = (char*)malloc(n);
    if(buf == NULL) {
        fclose(f);
        CHECK(0);
        *out_n = 0;
        return NULL;
    }
    if(fread(buf, 1, n, f) != n) {
        fclose(f);
        free(buf);
        CHECK(0);
        *out_n = 0;
        return NULL;
    }
    fclose(f);
    *out_n = n;
    return buf;
}

static void test_gps_csv_fixture(void) {
    char* raw;
    size_t nbytes = 0;
    size_t off = 0;
    SrLine line;
    SrParser parser;
    SrEvent ev;
    uint32_t parse_ok = 0;
    uint32_t ap_ok = 0;
    uint32_t ble_ok = 0;
    uint32_t tick = 0;

    raw = gps_csv_load_fixture("wardrive_no_fix.bin", 6373, &nbytes);
    CHECK(raw != NULL);
    if(raw == NULL) {
        return;
    }

    sr_line_init(&line);
    memset(&parser, 0, sizeof(parser));
    sr_bloom_init(&g_bloom);
    sr_model_init(&g_model, &g_bloom, NULL);

    while(off < nbytes) {
        size_t used = sr_line_feed(&line, raw + off, nbytes - off);
        if(used == 0) {
            if(!sr_line_ready(&line)) {
                break;
            }
        } else {
            off += used;
        }
        if(!sr_line_ready(&line)) {
            continue;
        }
        {
            size_t len = 0;
            const char* text = sr_line_text(&line, &len);
            SrParseResult r;

            if(!sr_line_truncated(&line)) {
                memset(&ev, 0, sizeof(ev));
                r = sr_codec_marauder.feed_line(&parser, text, len, &ev);
                if(r == SrParseOk &&
                   (ev.kind == SrEventApFound || ev.kind == SrEventBleFound)) {
                    parse_ok++;
                    if(ev.kind == SrEventApFound) {
                        ap_ok++;
                    } else {
                        ble_ok++;
                    }
                    (void)sr_model_apply(&g_model, &ev, tick++);
                }
            }
            sr_line_consume(&line);
        }
    }

    fprintf(
        stderr,
        "gps_csv fixture: parse_ok=%u ap=%u ble=%u gps_csv_rev=%u with_gps_fix=%u gps_blocks=%u\n",
        parse_ok,
        ap_ok,
        ble_ok,
        g_model.gps_csv_rev,
        g_model.with_gps_fix,
        g_model.gps_blocks);

    CHECK(g_model.with_gps_fix == 0);
    CHECK(g_model.gps_blocks == 0);
    CHECK(g_model.gps_csv_rev == parse_ok);
    CHECK(ap_ok + ble_ok == parse_ok);
    /* Welded after the first print: gps_csv fixture: parse_ok=62 ap=28 ble=34 */
    CHECK(parse_ok == 62);
    CHECK(ap_ok == 28);
    CHECK(ble_ok == 34);

    free(raw);
}

int test_model_run(void) {
    sr_test_failures = 0;
    test_sizeof_and_init();
    test_session_rev();
    test_stop_reason();
    test_firmware_apply_and_reset_keeps_identity();
    test_null_safety();
    test_mac_parse();
    test_transitions_24();
    test_stopped_restart_clears_session_not_bloom();
    test_ring_wrap();
    test_borrowed_view_not_stored();
    test_window_view_no_overread();
    test_stats_gps_malformed();
    test_gps_csv_rows();
    test_gps_csv_fixture();
    test_oracle_agreement_scripted();
    test_fuzz();
    return sr_test_failures;
}
