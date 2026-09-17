#include "sr_test.h"

#include "sr_line.h"
#include "sr_parse_marauder.h"
#include "sr_source_codec.h"
#include "sr_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

_Static_assert(sizeof(SrParser) <= 512, "ADR-016: SrParser with fw_partial must stay <= 512");

/*
 * Independent oracle (A3): memcpy into a NUL-terminated buffer first, then
 * slice fields with strchr / strstr / sscanf / strtoul -- deliberately
 * different from the implementation's own "scan commas from the right +
 * hand-written parse_int" technique.
 */

typedef struct {
    uint32_t cursor;
    char bssid[SR_BSSID_MAX + 1];
    char ssid[SR_SSID_MAX + 1];
    char auth[SR_AUTH_MAX + 1];
    int channel;
    int rssi;
    SrRadioType radio;
    bool ok;
} OracleAp;

static int oracle_mac_ok(const char* s) {
    unsigned a, b, c, d, e, f;
    char extra;

    if(s == NULL || strlen(s) != 17U) {
        return 0;
    }
    if(s[2] != ':' || s[5] != ':' || s[8] != ':' || s[11] != ':' || s[14] != ':') {
        return 0;
    }
    extra = 0;
    if(sscanf(s, "%2x:%2x:%2x:%2x:%2x:%2x%c", &a, &b, &c, &d, &e, &f, &extra) != 6) {
        return 0;
    }
    return 1;
}

static bool oracle_int(const char* s, int* out) {
    char extra;
    size_t n;
    size_t i = 0;

    if(s == NULL || s[0] == '\0') {
        return false;
    }
    n = strlen(s);
    if(s[0] == '-') {
        i = 1;
        if(s[i] == '\0') {
            return false;
        }
    }
    for(; i < n; i++) {
        if(s[i] < '0' || s[i] > '9') {
            return false;
        }
    }
    extra = 0;
    if(sscanf(s, "%d%c", out, &extra) != 1) {
        return false;
    }
    return true;
}

static void oracle_copy_trunc(char* dst, size_t cap, const char* src) {
    size_t n = src ? strlen(src) : 0;
    if(cap == 0) {
        return;
    }
    if(n >= cap) {
        n = cap - 1U;
    }
    if(n > 0 && src != NULL) {
        memcpy(dst, src, n);
    }
    dst[n] = '\0';
}

static bool oracle_parse(const char* line, size_t len, OracleAp* o) {
    char* buf;
    char* commas[64];
    int ncom = 0;
    char* p;
    char* fields[11];
    int i;
    char* f0;
    char extra;

    memset(o, 0, sizeof(*o));
    if(line == NULL) {
        return false;
    }
    buf = (char*)malloc(len + 1U);
    if(buf == NULL) {
        return false;
    }
    memcpy(buf, line, len);
    buf[len] = '\0';

    p = buf;
    while(ncom < 64) {
        char* c = strchr(p, ',');
        if(c == NULL) {
            break;
        }
        commas[ncom++] = c;
        p = c + 1;
    }
    if(ncom < 10) {
        free(buf);
        return false;
    }

    /* Take the rightmost 10 commas. The extra commas stay in field[0] --
       same semantics as the implementation, just written differently. */
    {
        char** last = commas + (ncom - 10);
        fields[0] = buf;
        for(i = 0; i < 10; i++) {
            *last[i] = '\0';
            fields[i + 1] = last[i] + 1;
        }
    }

    if(strcmp(fields[10], "WIFI") == 0) {
        o->radio = SrRadioWifi;
    } else if(strcmp(fields[10], "BLE") == 0) {
        o->radio = SrRadioBle;
    } else {
        free(buf);
        return false;
    }

    if(fields[2][0] != '[' || fields[2][0] == '\0' ||
       fields[2][strlen(fields[2]) - 1U] != ']') {
        free(buf);
        return false;
    }

    f0 = fields[0];
    if(o->radio == SrRadioWifi) {
        char* sep = strstr(f0, " | ");
        unsigned long cur;
        if(sep == NULL) {
            free(buf);
            return false;
        }
        *sep = '\0';
        if(f0[0] == '\0' || strspn(f0, "0123456789") != strlen(f0)) {
            free(buf);
            return false;
        }
        extra = 0;
        if(sscanf(f0, "%lu%c", &cur, &extra) != 1) {
            free(buf);
            return false;
        }
        o->cursor = (uint32_t)cur;
        if(!oracle_mac_ok(sep + 3)) {
            free(buf);
            return false;
        }
        oracle_copy_trunc(o->bssid, sizeof(o->bssid), sep + 3);
        oracle_copy_trunc(o->ssid, sizeof(o->ssid), fields[1]);
    } else {
        size_t flen = strlen(f0);
        const char* mac;
        if(flen < 17U) {
            free(buf);
            return false;
        }
        mac = f0 + (flen - 17U);
        if(!oracle_mac_ok(mac)) {
            free(buf);
            return false;
        }
        o->cursor = 0;
        oracle_copy_trunc(o->bssid, sizeof(o->bssid), mac);
        if(flen == 34U && memcmp(f0, mac, 17) == 0) {
            o->ssid[0] = '\0';
        } else {
            char name[SR_RAW_LINE_MAX + 1];
            size_t nn = flen - 17U;
            memcpy(name, f0, nn);
            name[nn] = '\0';
            oracle_copy_trunc(o->ssid, sizeof(o->ssid), name);
        }
    }

    oracle_copy_trunc(o->auth, sizeof(o->auth), fields[2]);
    if(!oracle_int(fields[4], &o->channel) || !oracle_int(fields[5], &o->rssi)) {
        free(buf);
        return false;
    }
    o->ok = true;
    free(buf);
    return true;
}

/* -------------------------------------------------------------------------- */
/* Fixture loading                                                            */
/* -------------------------------------------------------------------------- */

static char* load_fixture(const char* name, size_t expect, size_t* out_n) {
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

typedef struct {
    uint32_t delivered;
    uint32_t ap;
    uint32_t ble;
    uint32_t started;
    uint32_t stopped;
    uint32_t unknown;
    uint32_t malformed;
    uint32_t needmore;
    uint32_t gps;
    uint32_t firmware;
    SrGpsSnapshot gps_snaps[4];
    SrStopReason last_stop;
    char leftover[SR_RAW_LINE_MAX + 1];
    size_t leftover_len;
    uint32_t lines_seen;
    uint32_t lines_malformed;
    bool in_session;
} RunStats;

static void feed_window(const char* text, size_t len, SrParser* p, SrEvent* ev, SrParseResult* r);

static void run_bytes(const char* raw, size_t n, RunStats* st) {
    SrLine line;
    SrParser parser;
    size_t off = 0;

    memset(st, 0, sizeof(*st));
    sr_line_init(&line);
    memset(&parser, 0, sizeof(parser));

    while(off < n) {
        size_t used = sr_line_feed(&line, raw + off, n - off);
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
            SrEvent ev;
            SrParseResult r;

            memset(&ev, 0, sizeof(ev));
            r = sr_codec_marauder.feed_line(&parser, text, len, &ev);
            st->delivered++;
            if(r == SrParseMalformed) {
                st->malformed++;
            } else if(r == SrParseUnknown) {
                st->unknown++;
            } else if(r == SrParseNeedMore) {
                st->needmore++;
            }
            if(ev.kind == SrEventApFound) {
                st->ap++;
            } else if(ev.kind == SrEventBleFound) {
                st->ble++;
            } else if(ev.kind == SrEventScanStarted) {
                st->started++;
            } else if(ev.kind == SrEventScanStopped) {
                st->stopped++;
                st->last_stop = ev.u.stop;
            } else if(ev.kind == SrEventGps) {
                if(st->gps < 4) {
                    st->gps_snaps[st->gps] = ev.u.gps;
                }
                st->gps++;
            } else if(ev.kind == SrEventFirmware) {
                st->firmware++;
            }
            sr_line_consume(&line);
        }
    }
    st->leftover_len = line.len;
    if(line.len > 0) {
        memcpy(st->leftover, line.buf, line.len);
    }
    st->leftover[line.len] = '\0';
    st->lines_seen = parser.lines_seen;
    st->lines_malformed = parser.lines_malformed;
    st->in_session = parser.in_session;
}

static void account_event(RunStats* st, SrParseResult r, const SrEvent* ev) {
    st->delivered++;
    if(r == SrParseMalformed) {
        st->malformed++;
    } else if(r == SrParseUnknown) {
        st->unknown++;
    } else if(r == SrParseNeedMore) {
        st->needmore++;
    }
    if(ev->kind == SrEventApFound) {
        st->ap++;
    } else if(ev->kind == SrEventBleFound) {
        st->ble++;
    } else if(ev->kind == SrEventScanStarted) {
        st->started++;
    } else if(ev->kind == SrEventScanStopped) {
        st->stopped++;
        st->last_stop = ev->u.stop;
    } else if(ev->kind == SrEventGps) {
        if(st->gps < 4) {
            st->gps_snaps[st->gps] = ev->u.gps;
        }
        st->gps++;
    } else if(ev->kind == SrEventFirmware) {
        st->firmware++;
    }
}

static void run_bytes_window(const char* raw, size_t n, RunStats* st) {
    SrLine line;
    SrParser parser;
    size_t off = 0;

    memset(st, 0, sizeof(*st));
    sr_line_init(&line);
    memset(&parser, 0, sizeof(parser));

    while(off < n) {
        size_t used = sr_line_feed(&line, raw + off, n - off);
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
            SrEvent ev;
            SrParseResult r;

            feed_window(text, len, &parser, &ev, &r);
            account_event(st, r, &ev);
            sr_line_consume(&line);
        }
    }
    st->leftover_len = line.len;
    if(line.len > 0) {
        memcpy(st->leftover, line.buf, line.len);
    }
    st->leftover[line.len] = '\0';
    st->lines_seen = parser.lines_seen;
    st->lines_malformed = parser.lines_malformed;
    st->in_session = parser.in_session;
}

static char* g_wardrive;
static size_t g_wardrive_n;
static char* g_startup;
static size_t g_startup_n;
static char* g_gpsdata;
static size_t g_gpsdata_n;
static char* g_stop;
static size_t g_stop_n;

static void load_all(void) {
    g_wardrive = load_fixture("wardrive_no_fix.bin", 6373, &g_wardrive_n);
    g_startup = load_fixture("startup_info.bin", 230, &g_startup_n);
    g_gpsdata = load_fixture("gpsdata.bin", 479, &g_gpsdata_n);
    g_stop = load_fixture("stop_sequence.bin", 104, &g_stop_n);
}

static void free_all(void) {
    free(g_wardrive);
    free(g_startup);
    free(g_gpsdata);
    free(g_stop);
    g_wardrive = g_startup = g_gpsdata = g_stop = NULL;
}

/* -------------------------------------------------------------------------- */
/* A2 event counts across the four fixtures                                   */
/* -------------------------------------------------------------------------- */

static void test_fixture_counts(void) {
    RunStats st;

    CHECK(g_wardrive != NULL && g_startup != NULL && g_gpsdata != NULL && g_stop != NULL);

    run_bytes(g_wardrive, g_wardrive_n, &st);
    CHECK(st.delivered == 81);
    CHECK(st.ap == 28);
    CHECK(st.ble == 34);
    CHECK(st.started == 1);
    CHECK(st.stopped == 0);
    CHECK(st.unknown == 18);
    CHECK(st.needmore == 0);
    CHECK(st.gps == 0);
    CHECK(st.malformed == 0);
    CHECK(st.lines_malformed == 0);
    CHECK(st.lines_seen == 81);
    CHECK(st.leftover_len == 0);

    run_bytes(g_stop, g_stop_n, &st);
    CHECK(st.delivered == 5);
    CHECK(st.ap == 0);
    CHECK(st.ble == 0);
    CHECK(st.started == 0);
    CHECK(st.stopped == 1);
    CHECK(st.last_stop == SrStopGpsUpdates);
    CHECK(st.unknown == 1);
    CHECK(st.needmore == 3);
    CHECK(st.gps == 0);
    CHECK(st.malformed == 0);
    CHECK(st.leftover_len == 2);
    CHECK(st.leftover[0] == '>' && st.leftover[1] == ' ');

    run_bytes(g_startup, g_startup_n, &st);
    CHECK(st.delivered == 10);
    CHECK(st.ap == 0);
    CHECK(st.ble == 0);
    CHECK(st.started == 0);
    CHECK(st.stopped == 0);
    CHECK(st.unknown == 6);
    CHECK(st.firmware == 4);
    CHECK(st.needmore == 0);
    CHECK(st.gps == 0);
    CHECK(st.malformed == 0);
    CHECK(st.leftover_len == 2);
    CHECK(st.leftover[0] == '>' && st.leftover[1] == ' ');

    run_bytes(g_gpsdata, g_gpsdata_n, &st);
    CHECK(st.delivered == 29);
    CHECK(st.ap == 0);
    CHECK(st.ble == 0);
    CHECK(st.started == 0);
    CHECK(st.stopped == 0);
    CHECK(st.unknown == 2);
    CHECK(st.needmore == 24);
    CHECK(st.gps == 3);
    CHECK(st.malformed == 0);
    CHECK(st.leftover_len == 0);
}

/* -------------------------------------------------------------------------- */
/* A3 field-level vs. oracle                                                  */
/* -------------------------------------------------------------------------- */

static void test_oracle_fields(void) {
    SrLine line;
    SrParser parser;
    size_t off = 0;
    uint32_t data_n = 0;

    CHECK(g_wardrive != NULL);
    sr_line_init(&line);
    memset(&parser, 0, sizeof(parser));

    while(off < g_wardrive_n) {
        size_t used = sr_line_feed(&line, g_wardrive + off, g_wardrive_n - off);
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
            int is_data = 0;

            if(len >= 5 && memcmp(text + len - 5, ",WIFI", 5) == 0) {
                is_data = 1;
            } else if(len >= 4 && memcmp(text + len - 4, ",BLE", 4) == 0) {
                is_data = 1;
            }
            if(is_data) {
                SrEvent ev;
                SrParseResult r;
                OracleAp oa;
                const SrApRecord* rec;

                memset(&ev, 0, sizeof(ev));
                r = sr_codec_marauder.feed_line(&parser, text, len, &ev);
                CHECK(oracle_parse(text, len, &oa));
                CHECK(r == SrParseOk);
                CHECK(oa.ok);
                CHECK(ev.kind == SrEventApFound || ev.kind == SrEventBleFound);
                rec = (ev.kind == SrEventApFound) ? &ev.u.ap : &ev.u.ble;
                CHECK(rec->cursor == oa.cursor);
                CHECK(strcmp(rec->bssid, oa.bssid) == 0);
                CHECK(strcmp(rec->ssid, oa.ssid) == 0);
                CHECK(strcmp(rec->auth, oa.auth) == 0);
                CHECK(rec->channel == oa.channel);
                CHECK(rec->rssi == oa.rssi);
                CHECK(rec->radio == oa.radio);
                data_n++;
            }
            sr_line_consume(&line);
        }
    }
    CHECK(data_n == 62);
}

/* -------------------------------------------------------------------------- */
/* A4 named assertions                                                        */
/* -------------------------------------------------------------------------- */

static const char kLineWifi1[] =
    "1 | 25:56:D6:8C:84:95,HOMENET1,[WPA2_PSK],,149,-63,0.0000000,0.0000000,0.00,0.00,WIFI";
static const char kLineEmptySsid[] =
    "18 | 7B:FF:D7:8C:41:12,,[WPA_WPA2_PSK],,48,-82,0.0000000,0.0000000,0.00,0.00,WIFI";
/* The card said cursor=20; the fixture really carries "19 | 7D:FF:...". Use the real  */
/* line and compare the ssid byte for byte.                                             */
static const char kLineNonAscii[] =
    "19 | 7D:FF:D7:8C:92:C3,1Rés-Süd_5G,[WPA_WPA2_PSK],,48,-86,0.0000000,0.0000000,0.00,0.00,WIFI";
static const char kSsidNonAscii[] = "1Rés-Süd_5G";
static const unsigned char kSsidNonAsciiBytes[] = {
    0x31, 0x52, 0xC3, 0xA9, 0x73, 0x2D, 0x53, 0xC3, 0xBC, 0x64, 0x5F, 0x35, 0x47
};
static const char kLineBleNamed[] =
    "BT-729SP_85E7-LE3d:73:a7:13:85:e7,,[BLE],,0,-85,0.0000000,0.0000000,0.00,0.00,BLE";
static const char kLineBleAnon[] =
    "4b:31:3e:f6:71:4e4b:31:3e:f6:71:4e,,[BLE],,0,-59,0.0000000,0.0000000,0.00,0.00,BLE";
static const char kLineBleWatch[] =
    "Fitness Band (X7QPA)22:f2:e4:f7:d2:6b,,[BLE],,0,-87,0.0000000,0.0000000,0.00,0.00,BLE";
static const char kLineBleComma[] =
    "foo,barAA:BB:CC:DD:EE:FF,,[BLE],,0,-50,0,0,0,0,BLE";

static SrParseResult feed_lit(SrParser* p, const char* s, SrEvent* ev) {
    memset(ev, 0, sizeof(*ev));
    return sr_codec_marauder.feed_line(p, s, strlen(s), ev);
}

static void test_named(void) {
    SrParser p;
    SrEvent ev;
    SrParseResult r;

    memset(&p, 0, sizeof(p));

    r = feed_lit(&p, kLineWifi1, &ev);
    CHECK(r == SrParseOk);
    CHECK(ev.kind == SrEventApFound);
    CHECK(ev.u.ap.cursor == 1);
    CHECK(strcmp(ev.u.ap.bssid, "25:56:D6:8C:84:95") == 0);
    CHECK(strcmp(ev.u.ap.ssid, "HOMENET1") == 0);
    CHECK(strcmp(ev.u.ap.auth, "[WPA2_PSK]") == 0);
    CHECK(ev.u.ap.channel == 149);
    CHECK(ev.u.ap.rssi == -63);
    CHECK(ev.u.ap.radio == SrRadioWifi);

    r = feed_lit(&p, kLineEmptySsid, &ev);
    CHECK(r == SrParseOk);
    CHECK(ev.kind == SrEventApFound);
    CHECK(ev.u.ap.ssid[0] == '\0');
    CHECK(ev.u.ap.cursor == 18);
    CHECK(strcmp(ev.u.ap.bssid, "7B:FF:D7:8C:41:12") == 0);

    r = feed_lit(&p, kLineNonAscii, &ev);
    CHECK(r == SrParseOk);
    CHECK(ev.kind == SrEventApFound);
    CHECK(strcmp(ev.u.ap.ssid, kSsidNonAscii) == 0);
    CHECK(strlen(ev.u.ap.ssid) == sizeof(kSsidNonAsciiBytes));
    CHECK(memcmp(ev.u.ap.ssid, kSsidNonAsciiBytes, sizeof(kSsidNonAsciiBytes)) == 0);

    r = feed_lit(&p, kLineBleNamed, &ev);
    CHECK(r == SrParseOk);
    CHECK(ev.kind == SrEventBleFound);
    CHECK(ev.u.ble.cursor == 0);
    CHECK(ev.u.ble.channel == 0);
    CHECK(ev.u.ble.radio == SrRadioBle);
    CHECK(strcmp(ev.u.ble.bssid, "3d:73:a7:13:85:e7") == 0);
    CHECK(strcmp(ev.u.ble.ssid, "BT-729SP_85E7-LE") == 0);

    r = feed_lit(&p, kLineBleAnon, &ev);
    CHECK(r == SrParseOk);
    CHECK(ev.kind == SrEventBleFound);
    CHECK(strcmp(ev.u.ble.bssid, "4b:31:3e:f6:71:4e") == 0);
    CHECK(ev.u.ble.ssid[0] == '\0');

    r = feed_lit(&p, kLineBleWatch, &ev);
    CHECK(r == SrParseOk);
    CHECK(ev.kind == SrEventBleFound);
    CHECK(strcmp(ev.u.ble.ssid, "Fitness Band (X7QPA)") == 0);
    CHECK(strcmp(ev.u.ble.bssid, "22:f2:e4:f7:d2:6b") == 0);

    /* Synthetic case for negative control ②: the BLE name contains a comma, so it only parses correctly when sliced from the right. */
    r = feed_lit(&p, kLineBleComma, &ev);
    CHECK(r == SrParseOk);
    CHECK(ev.kind == SrEventBleFound);
    CHECK(strcmp(ev.u.ble.bssid, "AA:BB:CC:DD:EE:FF") == 0);
    CHECK(strcmp(ev.u.ble.ssid, "foo,bar") == 0);
    CHECK(ev.u.ble.cursor == 0);
}

/* -------------------------------------------------------------------------- */
/* A5 exactly len bytes, no NUL padding                                       */
/* -------------------------------------------------------------------------- */

static void feed_window(const char* text, size_t len, SrParser* p, SrEvent* ev, SrParseResult* r) {
    char* w = (char*)malloc(len);

    CHECK(w != NULL);
    if(w == NULL) {
        return;
    }
    if(len > 0 && text != NULL) {
        memcpy(w, text, len);
    }
    memset(ev, 0, sizeof(*ev));
    *r = sr_codec_marauder.feed_line(p, w, len, ev);
    /* The Unknown event's text points into w; it must be read before w is freed. */
    if(ev->kind == SrEventUnknown) {
        CHECK(ev->u.unknown.len == len);
        CHECK(ev->u.unknown.text == w);
    }
    free(w);
}

static void test_no_nul_windows(void) {
    const char* blobs[4];
    size_t sizes[4];
    int bi;

    blobs[0] = g_wardrive;
    sizes[0] = g_wardrive_n;
    blobs[1] = g_startup;
    sizes[1] = g_startup_n;
    blobs[2] = g_gpsdata;
    sizes[2] = g_gpsdata_n;
    blobs[3] = g_stop;
    sizes[3] = g_stop_n;

    for(bi = 0; bi < 4; bi++) {
        SrLine line;
        SrParser parser;
        size_t off = 0;

        CHECK(blobs[bi] != NULL);
        sr_line_init(&line);
        memset(&parser, 0, sizeof(parser));
        while(off < sizes[bi]) {
            size_t used = sr_line_feed(&line, blobs[bi] + off, sizes[bi] - off);
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
                SrEvent ev;
                SrParseResult r;

                feed_window(text, len, &parser, &ev, &r);
                (void)r;
                sr_line_consume(&line);
            }
        }
    }

    /* D10: feeding in half a line must not crash. Malformed or Unknown are both fine. */
    {
        SrParser p;
        SrEvent ev;
        SrParseResult r;
        static const char half[] = "1 | 25:56:D6:8C:84:95,HOMENET1,[WPA2";
        static const char few[] = "1 | 25:56:D6:8C:84:95,HOMENET1,[WPA2_PSK],,149,-63,WIFI";

        memset(&p, 0, sizeof(p));
        feed_window(half, sizeof(half) - 1U, &p, &ev, &r);
        CHECK(r == SrParseUnknown || r == SrParseMalformed);
        CHECK(ev.kind == SrEventUnknown);
        feed_window(few, sizeof(few) - 1U, &p, &ev, &r);
        CHECK(r == SrParseUnknown || r == SrParseMalformed);
        CHECK(ev.kind == SrEventUnknown);
    }
}

/* -------------------------------------------------------------------------- */
/* A6 probe_line                                                              */
/* -------------------------------------------------------------------------- */

static void test_probe(void) {
    SrLine line;
    SrParser unused;
    SrFirmwareInfo info;
    SrFirmwareInfo snap;
    size_t off = 0;
    int hits = 0;
    int delivered = 0;

    (void)unused;
    CHECK(g_startup != NULL);
    memset(&info, 0, sizeof(info));
    sr_line_init(&line);

    while(off < g_startup_n) {
        size_t used = sr_line_feed(&line, g_startup + off, g_startup_n - off);
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
            bool hit;

            memcpy(&snap, &info, sizeof(snap));
            hit = sr_codec_marauder.probe_line(text, &info);
            if(hit) {
                hits++;
            } else {
                CHECK(memcmp(&info, &snap, sizeof(info)) == 0);
            }
            delivered++;
            sr_line_consume(&line);
        }
    }
    CHECK(delivered == 10);
    CHECK(hits == 4);
    CHECK(strcmp(info.firmware, "Marauder") == 0);
    CHECK(strcmp(info.version, "v1.14.1") == 0);
    CHECK(strcmp(info.hardware, "ESP32-C5 DevKit") == 0);
    CHECK(strcmp(info.esp_idf, "v5.5.1-710-g8410210c9a") == 0);
    CHECK(info.kind == SrSourceMarauder);

    /* out is not zeroed between calls: fill version first, then firmware -- version is still there. */
    {
        SrFirmwareInfo acc;
        memset(&acc, 0, sizeof(acc));
        CHECK(sr_codec_marauder.probe_line("Version: v0.0.1", &acc) == true);
        CHECK(strcmp(acc.version, "v0.0.1") == 0);
        CHECK(sr_codec_marauder.probe_line("Firmware: Marauder", &acc) == true);
        CHECK(strcmp(acc.version, "v0.0.1") == 0);
        CHECK(acc.kind == SrSourceMarauder);
        CHECK(sr_codec_marauder.probe_line("Firmware: Other", &acc) == true);
        CHECK(strcmp(acc.firmware, "Other") == 0);
        CHECK(acc.kind == SrSourceMarauder); /* kind is not cleared just because the firmware isn't Marauder */
    }

    CHECK(sr_codec_marauder.probe_line("WSL Bypass: enabled", &info) == false);
    CHECK(sr_codec_marauder.probe_line("SD Card: Connected", &info) == false);
    CHECK(sr_codec_marauder.probe_line(NULL, &info) == false);
    CHECK(sr_codec_marauder.probe_line("Firmware: Marauder", NULL) == false);
}

/* -------------------------------------------------------------------------- */
/* A7 command contract                                                        */
/* -------------------------------------------------------------------------- */

static void test_cmds(void) {
    char buf[32];
    SrScanCfg cfg;
    size_t n;
    size_t i;

    memset(&cfg, 0, sizeof(cfg));
    n = sr_codec_marauder.build_start_cmd(&cfg, buf, sizeof(buf));
    CHECK(n == 9);
    CHECK(memcmp(buf, "wardrive\n", 10) == 0);

    cfg.mirror_to_serial = true;
    n = sr_codec_marauder.build_start_cmd(&cfg, buf, sizeof(buf));
    CHECK(n == 17);
    CHECK(memcmp(buf, "wardrive -serial\n", 18) == 0);

    n = sr_codec_marauder.build_stop_cmd(buf, sizeof(buf));
    CHECK(n == 9);
    CHECK(memcmp(buf, "stopscan\n", 10) == 0);

    memset(buf, 0xAA, sizeof(buf));
    cfg.mirror_to_serial = false;
    n = sr_codec_marauder.build_start_cmd(&cfg, buf, 9); /* one NUL slot short */
    CHECK(n == 0);
    for(i = 0; i < sizeof(buf); i++) {
        CHECK((unsigned char)buf[i] == 0xAA);
    }

    memset(buf, 0xAA, sizeof(buf));
    cfg.mirror_to_serial = true;
    n = sr_codec_marauder.build_start_cmd(&cfg, buf, 17);
    CHECK(n == 0);
    for(i = 0; i < sizeof(buf); i++) {
        CHECK((unsigned char)buf[i] == 0xAA);
    }

    memset(buf, 0xAA, sizeof(buf));
    n = sr_codec_marauder.build_stop_cmd(buf, 9);
    CHECK(n == 0);
    for(i = 0; i < sizeof(buf); i++) {
        CHECK((unsigned char)buf[i] == 0xAA);
    }

    CHECK(strcmp(sr_codec_marauder.name, "marauder") == 0);
}

/* -------------------------------------------------------------------------- */
/* Targeted: session flags / malformed / truncation                           */
/* -------------------------------------------------------------------------- */

static void test_session_and_malformed(void) {
    SrParser p;
    SrEvent ev;
    SrParseResult r;

    memset(&p, 0, sizeof(p));
    r = feed_lit(&p, "StartingWardrive. Stop with stopscan", &ev);
    CHECK(r == SrParseOk);
    CHECK(ev.kind == SrEventScanStarted);
    CHECK(p.in_session == true);

    r = feed_lit(&p, "Stopping WiFi tran/recv", &ev);
    CHECK(r == SrParseOk && ev.kind == SrEventScanStopped && ev.u.stop == SrStopWifiTranRecv);
    CHECK(p.in_session == false);

    r = feed_lit(&p, "END OF NMEA STREAM", &ev);
    CHECK(r == SrParseOk && ev.u.stop == SrStopEndNmea);

    r = feed_lit(&p, "Stopping GPS data updates", &ev);
    CHECK(r == SrParseOk && ev.u.stop == SrStopGpsUpdates);

    {
        uint32_t mal0 = p.lines_malformed;
        r = feed_lit(&p, "AP config set error, Maurauder SSID might visible : err=0x3005", &ev);
        CHECK(r == SrParseUnknown);
        CHECK(ev.kind == SrEventUnknown);
        CHECK(p.lines_malformed == mal0);
    }

    r = feed_lit(&p, "", &ev);
    CHECK(r == SrParseUnknown);
    CHECK(ev.u.unknown.len == 0);

    /* Not enough commas */
    r = feed_lit(&p, "a,b,c,WIFI", &ev);
    CHECK(r == SrParseMalformed);
    CHECK(ev.kind == SrEventUnknown);

    /* AUTH missing brackets */
    r = feed_lit(&p, "1 | AA:BB:CC:DD:EE:FF,x,OPEN,,1,-1,0,0,0,0,WIFI", &ev);
    CHECK(r == SrParseMalformed);

    /* Empty SSID is valid */
    r = feed_lit(&p, kLineEmptySsid, &ev);
    CHECK(r == SrParseOk);
    CHECK(ev.u.ap.ssid[0] == '\0');

    /* Oversized BLE name gets truncated, not rejected */
    {
        char longn[96];
        size_t i;
        memset(longn, 'A', 40);
        memcpy(longn + 40, "AA:BB:CC:DD:EE:FF,,[BLE],,0,-1,0,0,0,0,BLE", 42);
        r = sr_codec_marauder.feed_line(&p, longn, 40 + 42, &ev);
        CHECK(r == SrParseOk);
        CHECK(ev.kind == SrEventBleFound);
        CHECK(strlen(ev.u.ble.ssid) == (size_t)SR_SSID_MAX);
        for(i = 0; i < (size_t)SR_SSID_MAX; i++) {
            CHECK(ev.u.ble.ssid[i] == 'A');
        }
    }

    /* Numeric overflow */
    r = feed_lit(&p, "1 | AA:BB:CC:DD:EE:FF,x,[WPA2],,99999999999,-1,0,0,0,0,WIFI", &ev);
    CHECK(r == SrParseMalformed);

    CHECK(sr_codec_marauder.feed_line(NULL, "x", 1, &ev) == SrParseUnknown);
    CHECK(sr_codec_marauder.feed_line(&p, "x", 1, NULL) == SrParseUnknown);
}

/* -------------------------------------------------------------------------- */
/* A3 / A7 gpsdata snapshot + sizeof                                          */
/* -------------------------------------------------------------------------- */

/* Per task card A3's table: the three no-fix blocks in gpsdata.bin are identical. Values copied from the card. */
static void check_nofix_snap(const SrGpsSnapshot* g) {
    CHECK(g->fix == false);
    CHECK(strcmp(g->text, "02 ANTSTATUS=OPEN") == 0);
    CHECK(strcmp(g->sats, "0") == 0);
    CHECK(strcmp(g->acc, "0.00") == 0);
    CHECK(strcmp(g->lat, "0.0000000") == 0);
    CHECK(strcmp(g->lon, "0.0000000") == 0);
    CHECK(strcmp(g->alt, "0.00") == 0);
    CHECK(g->datetime[0] == '\0');
}

static void test_gps_snapshots(void) {
    RunStats st;

    CHECK(g_gpsdata != NULL);
    run_bytes(g_gpsdata, g_gpsdata_n, &st);
    CHECK(st.gps == 3);
    check_nofix_snap(&st.gps_snaps[0]);
    check_nofix_snap(&st.gps_snaps[1]);
    check_nofix_snap(&st.gps_snaps[2]);
    CHECK(memcmp(&st.gps_snaps[0], &st.gps_snaps[1], sizeof(st.gps_snaps[0])) == 0);
    CHECK(memcmp(&st.gps_snaps[1], &st.gps_snaps[2], sizeof(st.gps_snaps[0])) == 0);
}

static void test_sizeof_parser(void) {
    fprintf(
        stderr,
        "sizeof(SrParser)=%zu sizeof(SrGpsSnapshot)=%zu sizeof(SrFirmwareInfo)=%zu\n",
        sizeof(SrParser),
        sizeof(SrGpsSnapshot),
        sizeof(SrFirmwareInfo));
    CHECK(sizeof(SrParser) <= 512);
    CHECK(sizeof(SrGpsSnapshot) == 221);
    CHECK(sizeof(SrFirmwareInfo) == 164);
}

/* T3.3 A3: startup_info.bin goes through feed_line (not probe_line) */
static void test_firmware_feed_startup(void) {
    SrLine line;
    SrParser parser;
    size_t off = 0;
    uint32_t delivered = 0;
    uint32_t fw_events = 0;
    uint32_t unknown = 0;
    SrEvent last_fw;

    CHECK(g_startup != NULL);
    sr_line_init(&line);
    memset(&parser, 0, sizeof(parser));
    memset(&last_fw, 0, sizeof(last_fw));

    while(off < g_startup_n) {
        size_t used = sr_line_feed(&line, g_startup + off, g_startup_n - off);
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
            SrEvent ev;
            SrParseResult r;

            memset(&ev, 0, sizeof(ev));
            r = sr_codec_marauder.feed_line(&parser, text, len, &ev);
            delivered++;
            if(ev.kind == SrEventFirmware) {
                CHECK(r == SrParseOk);
                fw_events++;
                last_fw = ev;
            } else if(r == SrParseUnknown) {
                unknown++;
            }
            sr_line_consume(&line);
        }
    }

    printf(
        "firmware feed cover: fw_events=%u unknown=%u delivered=%u\n",
        fw_events,
        unknown,
        delivered);
    CHECK(delivered == 10);
    CHECK(fw_events == 4);
    CHECK(unknown == 6);
    CHECK(strcmp(last_fw.u.firmware.firmware, "Marauder") == 0);
    CHECK(strcmp(last_fw.u.firmware.version, "v1.14.1") == 0);
    CHECK(strcmp(last_fw.u.firmware.hardware, "ESP32-C5 DevKit") == 0);
    CHECK(strcmp(last_fw.u.firmware.esp_idf, "v5.5.1-710-g8410210c9a") == 0);
    CHECK(last_fw.u.firmware.kind == SrSourceMarauder);
}

/* -------------------------------------------------------------------------- */
/* A4 no-NUL windows: gpsdata / stop_sequence counts match A2/A3              */
/* -------------------------------------------------------------------------- */

static void test_gps_window_counts(void) {
    RunStats st;

    CHECK(g_gpsdata != NULL && g_stop != NULL);

    run_bytes_window(g_gpsdata, g_gpsdata_n, &st);
    CHECK(st.delivered == 29);
    CHECK(st.unknown == 2);
    CHECK(st.needmore == 24);
    CHECK(st.gps == 3);
    CHECK(st.malformed == 0);
    CHECK(st.ap == 0);
    CHECK(st.stopped == 0);
    check_nofix_snap(&st.gps_snaps[0]);
    check_nofix_snap(&st.gps_snaps[1]);
    check_nofix_snap(&st.gps_snaps[2]);
    CHECK(memcmp(&st.gps_snaps[0], &st.gps_snaps[1], sizeof(st.gps_snaps[0])) == 0);
    CHECK(memcmp(&st.gps_snaps[1], &st.gps_snaps[2], sizeof(st.gps_snaps[0])) == 0);

    run_bytes_window(g_stop, g_stop_n, &st);
    CHECK(st.delivered == 5);
    CHECK(st.unknown == 1);
    CHECK(st.needmore == 3);
    CHECK(st.gps == 0);
    CHECK(st.stopped == 1);
    CHECK(st.last_stop == SrStopGpsUpdates);
    CHECK(st.malformed == 0);
    CHECK(st.leftover_len == 2);
    CHECK(st.leftover[0] == '>' && st.leftover[1] == ' ');
}

/* -------------------------------------------------------------------------- */
/* A5 stop_sequence named step-by-step assertions                             */
/* -------------------------------------------------------------------------- */

/* Card A5 / stop_sequence.bin real lines; \r has already been stripped by sr_line */
static const char kStopHdr[] = "==== GPS Data ====";
static const char kStopFix[] = "  Fix: No";
static const char kStopText[] = "      Text: 02 ANTSTATUS=OPEN";
static const char kStopSplit[] = " S#stopscan";
static const char kStopMsg[] = "Stopping GPS data updates";

static void test_gps_interrupt(void) {
    SrParser p;
    SrEvent ev;
    SrParseResult r;

    memset(&p, 0, sizeof(p));

    r = feed_lit(&p, kStopHdr, &ev);
    CHECK(r == SrParseNeedMore);
    CHECK(p.in_gps_block == true);
    CHECK(ev.kind == SrEventNone);
    CHECK(p.lines_malformed == 0);

    r = feed_lit(&p, kStopFix, &ev);
    CHECK(r == SrParseNeedMore);
    CHECK(p.in_gps_block == true);
    CHECK(ev.kind == SrEventNone);
    CHECK(p.lines_malformed == 0);

    r = feed_lit(&p, kStopText, &ev);
    CHECK(r == SrParseNeedMore);
    CHECK(p.in_gps_block == true);
    CHECK(ev.kind == SrEventNone);
    CHECK(p.lines_malformed == 0);

    r = feed_lit(&p, kStopSplit, &ev);
    CHECK(r == SrParseUnknown);
    CHECK(p.in_gps_block == false);
    CHECK(ev.kind == SrEventUnknown);
    CHECK(p.lines_malformed == 0);

    r = feed_lit(&p, kStopMsg, &ev);
    CHECK(r == SrParseOk);
    CHECK(ev.kind == SrEventScanStopped);
    CHECK(ev.u.stop == SrStopGpsUpdates);
    CHECK(p.in_gps_block == false);
    CHECK(p.lines_malformed == 0);
    CHECK(p.lines_seen == 5);
}

/* -------------------------------------------------------------------------- */
/* A6 synthetic cases                                                         */
/* (device-unverified shapes, see D7; not written to fixtures/)               */
/* -------------------------------------------------------------------------- */

static void test_gps_synthetic(void) {
    SrParser p;
    SrEvent ev;
    SrParseResult r;
    uint32_t mal0;

    /* A block header with or without "> " both open a block */
    memset(&p, 0, sizeof(p));
    r = feed_lit(&p, "> ==== GPS Data ====", &ev);
    CHECK(r == SrParseNeedMore);
    CHECK(p.in_gps_block == true);
    memset(&p, 0, sizeof(p));
    r = feed_lit(&p, "==== GPS Data ====", &ev);
    CHECK(r == SrParseNeedMore);
    CHECK(p.in_gps_block == true);

    /* Fix: lines indented by 0 / 2 / 6 spaces all match */
    memset(&p, 0, sizeof(p));
    (void)feed_lit(&p, "==== GPS Data ====", &ev);
    r = feed_lit(&p, "Fix: No", &ev);
    CHECK(r == SrParseNeedMore);
    CHECK(p.in_gps_block == true);
    r = feed_lit(&p, "  Fix: No", &ev);
    CHECK(r == SrParseNeedMore);
    r = feed_lit(&p, "      Fix: No", &ev);
    CHECK(r == SrParseNeedMore);

    /* Fix: Yes + nonzero lat/lon + non-empty D/T (synthetic, device-unverified, see D7) */
    memset(&p, 0, sizeof(p));
    CHECK(feed_lit(&p, "==== GPS Data ====", &ev) == SrParseNeedMore);
    CHECK(feed_lit(&p, "Fix: Yes", &ev) == SrParseNeedMore);
    CHECK(feed_lit(&p, "Text: 02 ANTSTATUS=OPEN", &ev) == SrParseNeedMore);
    CHECK(feed_lit(&p, "Sats: 8", &ev) == SrParseNeedMore);
    CHECK(feed_lit(&p, "Acc: 1.23", &ev) == SrParseNeedMore);
    CHECK(feed_lit(&p, "Lat: 31.2300000", &ev) == SrParseNeedMore);
    CHECK(feed_lit(&p, "Lon: 121.4700000", &ev) == SrParseNeedMore);
    CHECK(feed_lit(&p, "Alt: 12.50", &ev) == SrParseNeedMore);
    r = feed_lit(&p, "D/T: 2026-08-17 10:30:00", &ev);
    CHECK(r == SrParseOk);
    CHECK(ev.kind == SrEventGps);
    CHECK(ev.u.gps.fix == true);
    CHECK(strcmp(ev.u.gps.text, "02 ANTSTATUS=OPEN") == 0);
    CHECK(strcmp(ev.u.gps.sats, "8") == 0);
    CHECK(strcmp(ev.u.gps.acc, "1.23") == 0);
    CHECK(strcmp(ev.u.gps.lat, "31.2300000") == 0);
    CHECK(strcmp(ev.u.gps.lon, "121.4700000") == 0);
    CHECK(strcmp(ev.u.gps.alt, "12.50") == 0);
    CHECK(strcmp(ev.u.gps.datetime, "2026-08-17 10:30:00") == 0);
    CHECK(p.in_gps_block == false);

    /* A block nested inside a block: two headers back to back -- the earlier block is discarded and emits no event */
    memset(&p, 0, sizeof(p));
    CHECK(feed_lit(&p, "==== GPS Data ====", &ev) == SrParseNeedMore);
    CHECK(feed_lit(&p, "Fix: Yes", &ev) == SrParseNeedMore);
    r = feed_lit(&p, "==== GPS Data ====", &ev);
    CHECK(r == SrParseNeedMore);
    CHECK(ev.kind == SrEventNone);
    CHECK(p.in_gps_block == true);
    CHECK(p.gps_partial.fix == false);
    CHECK(feed_lit(&p, "Fix: No", &ev) == SrParseNeedMore);
    CHECK(feed_lit(&p, "Sats: 0", &ev) == SrParseNeedMore);
    CHECK(feed_lit(&p, "Acc: 0.00", &ev) == SrParseNeedMore);
    CHECK(feed_lit(&p, "Lat: 0.0000000", &ev) == SrParseNeedMore);
    CHECK(feed_lit(&p, "Lon: 0.0000000", &ev) == SrParseNeedMore);
    CHECK(feed_lit(&p, "Alt: 0.00", &ev) == SrParseNeedMore);
    r = feed_lit(&p, "D/T:", &ev);
    CHECK(r == SrParseOk);
    CHECK(ev.kind == SrEventGps);
    CHECK(ev.u.gps.fix == false);
    CHECK(ev.u.gps.text[0] == '\0');

    /* Missing Text: still lands correctly at D/T: */
    memset(&p, 0, sizeof(p));
    CHECK(feed_lit(&p, "==== GPS Data ====", &ev) == SrParseNeedMore);
    CHECK(feed_lit(&p, "Fix: No", &ev) == SrParseNeedMore);
    CHECK(feed_lit(&p, "Sats: 0", &ev) == SrParseNeedMore);
    CHECK(feed_lit(&p, "Acc: 0.00", &ev) == SrParseNeedMore);
    CHECK(feed_lit(&p, "Lat: 0.0000000", &ev) == SrParseNeedMore);
    CHECK(feed_lit(&p, "Lon: 0.0000000", &ev) == SrParseNeedMore);
    CHECK(feed_lit(&p, "Alt: 0.00", &ev) == SrParseNeedMore);
    r = feed_lit(&p, "D/T:", &ev);
    CHECK(r == SrParseOk);
    CHECK(ev.kind == SrEventGps);
    CHECK(ev.u.gps.text[0] == '\0');
    CHECK(strcmp(ev.u.gps.sats, "0") == 0);

    /* Fix: Maybe -> aborts the block; the line itself is Unknown, malformed does not increase */
    memset(&p, 0, sizeof(p));
    CHECK(feed_lit(&p, "==== GPS Data ====", &ev) == SrParseNeedMore);
    mal0 = p.lines_malformed;
    r = feed_lit(&p, "Fix: Maybe", &ev);
    CHECK(r == SrParseUnknown);
    CHECK(ev.kind == SrEventUnknown);
    CHECK(p.in_gps_block == false);
    CHECK(p.lines_malformed == mal0);

    /* Text: longer than SR_GPS_TEXT_MAX (63) gets truncated and NUL-terminated */
    {
        char long_text[96];
        size_t i;
        size_t n;

        memset(long_text, 0, sizeof(long_text));
        memcpy(long_text, "Text: ", 6);
        for(i = 0; i < 80; i++) {
            long_text[6 + i] = 'A';
        }
        n = 6 + 80;
        memset(&p, 0, sizeof(p));
        CHECK(feed_lit(&p, "==== GPS Data ====", &ev) == SrParseNeedMore);
        memset(&ev, 0, sizeof(ev));
        r = sr_codec_marauder.feed_line(&p, long_text, n, &ev);
        CHECK(r == SrParseNeedMore);
        CHECK(strlen(p.gps_partial.text) == (size_t)SR_GPS_TEXT_MAX);
        CHECK(p.gps_partial.text[SR_GPS_TEXT_MAX] == '\0');
        for(i = 0; i < (size_t)SR_GPS_TEXT_MAX; i++) {
            CHECK(p.gps_partial.text[i] == 'A');
        }
    }

    /* A real WIFI line inserted inside a block: the block is aborted and the line still parses as ApFound */
    memset(&p, 0, sizeof(p));
    CHECK(feed_lit(&p, "==== GPS Data ====", &ev) == SrParseNeedMore);
    CHECK(feed_lit(&p, "Fix: No", &ev) == SrParseNeedMore);
    r = feed_lit(&p, kLineWifi1, &ev);
    CHECK(r == SrParseOk);
    CHECK(ev.kind == SrEventApFound);
    CHECK(p.in_gps_block == false);
    CHECK(ev.u.ap.cursor == 1);
    CHECK(strcmp(ev.u.ap.ssid, "HOMENET1") == 0);
}

/* -------------------------------------------------------------------------- */
/* A8 fuzz                                                                    */
/* -------------------------------------------------------------------------- */

#define SR_FUZZ_ITERS 20000u
#define SR_FUZZ_SEED  0x7A23B0DEu
#define SR_FUZZ_GPS_ITERS 8000u
#define SR_FUZZ_GPS_SEED  0xC0FFEE01u

static uint32_t xs32(uint32_t* s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

enum { FUZZ_TEMPL_MAX = 80, FUZZ_TEMPL_LEN = 160 };

static char g_templ[FUZZ_TEMPL_MAX][FUZZ_TEMPL_LEN];
static size_t g_templ_len[FUZZ_TEMPL_MAX];
static int g_templ_n;

static void collect_templates(void) {
    SrLine line;
    size_t off = 0;

    g_templ_n = 0;
    if(g_wardrive == NULL) {
        return;
    }
    sr_line_init(&line);
    while(off < g_wardrive_n && g_templ_n < FUZZ_TEMPL_MAX) {
        size_t used = sr_line_feed(&line, g_wardrive + off, g_wardrive_n - off);
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
            if(len > 0 && len < (size_t)FUZZ_TEMPL_LEN) {
                memcpy(g_templ[g_templ_n], text, len);
                g_templ_len[g_templ_n] = len;
                g_templ_n++;
            }
            sr_line_consume(&line);
        }
    }
}

static void test_fuzz(void) {
    uint32_t seed = SR_FUZZ_SEED;
    uint32_t iter;
    uint32_t cov_ok = 0, cov_unknown = 0, cov_malformed = 0;
    uint32_t cov_ap = 0, cov_ble = 0, cov_started = 0, cov_stopped = 0;
    uint32_t cov_nonascii = 0, cov_comma_name = 0, cov_trunc = 0, cov_empty = 0;
    int reported = 0;

    collect_templates();
    CHECK(g_templ_n > 20);

    for(iter = 0; iter < SR_FUZZ_ITERS; iter++) {
        uint32_t case_seed = seed;
        uint32_t mode = iter % 10u;
        char stack[256];
        char* win;
        size_t n = 0;
        SrParser p;
        SrEvent ev;
        SrParseResult r;
        OracleAp oa;
        int i;

        memset(&p, 0, sizeof(p));
        memset(stack, 0, sizeof(stack));

        if(mode == 0 && g_templ_n > 0) {
            int idx = (int)(xs32(&seed) % (uint32_t)g_templ_n);
            n = g_templ_len[idx];
            memcpy(stack, g_templ[idx], n);
        } else if(mode == 1) {
            n = strlen(kLineWifi1);
            memcpy(stack, kLineWifi1, n);
        } else if(mode == 2) {
            n = strlen(kLineBleNamed);
            memcpy(stack, kLineBleNamed, n);
        } else if(mode == 3) {
            static const char* msgs[] = {
                "StartingWardrive. Stop with stopscan",
                "Stopping WiFi tran/recv",
                "END OF NMEA STREAM",
                "Stopping GPS data updates",
                "AP config set error, Maurauder SSID might visible : err=0x3005",
            };
            const char* s = msgs[xs32(&seed) % 5u];
            n = strlen(s);
            memcpy(stack, s, n);
        } else if(mode == 4) {
            n = (size_t)(xs32(&seed) % 80u);
            for(i = 0; i < (int)n; i++) {
                stack[i] = (char)(xs32(&seed) & 0xFFu);
            }
        } else if(mode == 5) {
            n = strlen(kLineWifi1);
            memcpy(stack, kLineWifi1, n);
            /* Remove one comma -> malformed */
            for(i = 0; i < (int)n; i++) {
                if(stack[i] == ',') {
                    memmove(stack + i, stack + i + 1, n - (size_t)i - 1U);
                    n--;
                    break;
                }
            }
        } else if(mode == 6) {
            n = strlen(kLineBleComma);
            memcpy(stack, kLineBleComma, n);
        } else if(mode == 7) {
            n = strlen(kLineWifi1);
            memcpy(stack, kLineWifi1, n);
            if(n > 8) {
                n = 8 + (size_t)(xs32(&seed) % (n - 8U));
            }
            cov_trunc++;
        } else if(mode == 8) {
            /* Insert non-ASCII bytes into the WiFi SSID */
            static const char head[] = "1 | AA:BB:CC:DD:EE:FF,";
            static const char tail[] = ",[WPA2],,6,-40,0,0,0,0,WIFI";
            size_t hn = sizeof(head) - 1U;
            size_t tn = sizeof(tail) - 1U;
            memcpy(stack, head, hn);
            stack[hn] = (char)0xE8;
            stack[hn + 1U] = (char)0xB7;
            stack[hn + 2U] = (char)0xAF;
            memcpy(stack + hn + 3U, tail, tn);
            n = hn + 3U + tn;
            cov_nonascii++;
        } else {
            static const char junk[] = "x,y,WIFI";
            n = sizeof(junk) - 1U;
            memcpy(stack, junk, n);
        }

        win = (char*)malloc(n == 0 ? 1U : n);
        CHECK(win != NULL);
        if(win == NULL) {
            break;
        }
        if(n > 0) {
            memcpy(win, stack, n);
        }
        r = sr_codec_marauder.feed_line(&p, win, n, &ev);

        if(r == SrParseOk) {
            cov_ok++;
        } else if(r == SrParseUnknown) {
            cov_unknown++;
        } else if(r == SrParseMalformed) {
            cov_malformed++;
        }
        if(ev.kind == SrEventApFound) {
            cov_ap++;
        } else if(ev.kind == SrEventBleFound) {
            cov_ble++;
        } else if(ev.kind == SrEventScanStarted) {
            cov_started++;
        } else if(ev.kind == SrEventScanStopped) {
            cov_stopped++;
        }
        if(n == 0) {
            cov_empty++;
        }
        if(mode == 6 && r == SrParseOk && ev.kind == SrEventBleFound &&
           strcmp(ev.u.ble.ssid, "foo,bar") == 0) {
            cov_comma_name++;
        }

        /* Cross-check data lines against the oracle (different implementation technique) */
        if((r == SrParseOk) &&
           (ev.kind == SrEventApFound || ev.kind == SrEventBleFound)) {
            if(oracle_parse(win, n, &oa) && oa.ok) {
                const SrApRecord* rec = (ev.kind == SrEventApFound) ? &ev.u.ap : &ev.u.ble;
                if(rec->cursor != oa.cursor || strcmp(rec->bssid, oa.bssid) != 0 ||
                   strcmp(rec->ssid, oa.ssid) != 0 || rec->channel != oa.channel ||
                   rec->rssi != oa.rssi || rec->radio != oa.radio) {
                    sr_test_failures++;
                    if(reported < 5) {
                        fprintf(
                            stderr,
                            "fuzz oracle mismatch iter=%u seed=0x%08X mode=%u n=%zu\n",
                            iter,
                            case_seed,
                            mode,
                            n);
                        reported++;
                    }
                }
            }
        }

        CHECK(p.lines_seen == 1);
        if(r == SrParseMalformed) {
            CHECK(p.lines_malformed == 1);
        } else {
            CHECK(p.lines_malformed == 0);
        }
        free(win);
    }

    fprintf(
        stderr,
        "parse fuzz coverage: ok=%u unknown=%u malformed=%u ap=%u ble=%u "
        "started=%u stopped=%u nonascii=%u comma_name=%u trunc=%u empty=%u\n",
        cov_ok,
        cov_unknown,
        cov_malformed,
        cov_ap,
        cov_ble,
        cov_started,
        cov_stopped,
        cov_nonascii,
        cov_comma_name,
        cov_trunc,
        cov_empty);

    /*
     * The seed is fixed, so the numbers are deterministic. Thresholds are set at
     * roughly 1/3 of the measured values:
     * ok=11152 unknown=4848 malformed=4000 ap=4645 ble=4871
     * started=428 stopped=1208 nonascii=2000 comma_name=2000 trunc=2000 empty=23
     */
    CHECK(cov_ok > 3000u);
    CHECK(cov_unknown > 1500u);
    CHECK(cov_malformed > 1200u);
    CHECK(cov_ap > 1500u);
    CHECK(cov_ble > 1500u);
    CHECK(cov_started > 100u);
    CHECK(cov_stopped > 400u);
    CHECK(cov_nonascii > 600u);
    CHECK(cov_comma_name > 600u);
    CHECK(cov_trunc > 600u);
    CHECK(cov_empty > 5u);

    /* GPS mutations: multi-line sequences. Print the coverage numbers first, then lock in the lower-bound thresholds from them. */
    {
        uint32_t gseed = SR_FUZZ_GPS_SEED;
        uint32_t giter;
        uint32_t gps_emitted = 0, gps_needmore = 0, gps_aborted = 0;
        uint32_t g_ok = 0, g_unknown = 0, g_malformed = 0;

        for(giter = 0; giter < SR_FUZZ_GPS_ITERS; giter++) {
            SrParser gp;
            uint32_t fed = 0;
            uint32_t mode = giter % 12u;
            char lines[12][160];
            size_t ln[12];
            int nlines = 0;
            int li;

            memset(&gp, 0, sizeof(gp));

            if(mode == 0) {
                /* A complete no-fix block */
                const char* seq[] = {
                    "==== GPS Data ====",
                    "  Fix: No",
                    "      Text: 02 ANTSTATUS=OPEN",
                    " Sats: 0",
                    "  Acc: 0.00",
                    "  Lat: 0.0000000",
                    "  Lon: 0.0000000",
                    "  Alt: 0.00",
                    "  D/T: ",
                };
                nlines = 9;
                for(li = 0; li < nlines; li++) {
                    ln[li] = strlen(seq[li]);
                    memcpy(lines[li], seq[li], ln[li]);
                }
            } else if(mode == 1) {
                /* Synthetic fix block (D7: device-unverified) */
                const char* seq[] = {
                    "> ==== GPS Data ====",
                    "Fix: Yes",
                    "Text: 02 ANTSTATUS=OPEN",
                    "Sats: 12",
                    "Acc: 3.14",
                    "Lat: 31.2300000",
                    "Lon: 121.4700000",
                    "Alt: 12.50",
                    "D/T: 2026-08-17 10:30:00",
                };
                nlines = 9;
                for(li = 0; li < nlines; li++) {
                    ln[li] = strlen(seq[li]);
                    memcpy(lines[li], seq[li], ln[li]);
                }
            } else if(mode == 2) {
                const char* seq[] = {
                    "==== GPS Data ====",
                    "  Fix: No",
                    " S#stopscan",
                    "Stopping GPS data updates",
                };
                nlines = 4;
                for(li = 0; li < nlines; li++) {
                    ln[li] = strlen(seq[li]);
                    memcpy(lines[li], seq[li], ln[li]);
                }
            } else if(mode == 3) {
                /* A data line inserted inside a block */
                const char* seq[] = {
                    "==== GPS Data ====",
                    "  Fix: No",
                    kLineWifi1,
                };
                nlines = 3;
                for(li = 0; li < nlines; li++) {
                    ln[li] = strlen(seq[li]);
                    memcpy(lines[li], seq[li], ln[li]);
                }
            } else if(mode == 4) {
                /* Nested block headers */
                const char* seq[] = {
                    "==== GPS Data ====",
                    "Fix: Yes",
                    "==== GPS Data ====",
                    "Fix: No",
                    "Sats: 0",
                    "Acc: 0.00",
                    "Lat: 0.0000000",
                    "Lon: 0.0000000",
                    "Alt: 0.00",
                    "D/T:",
                };
                nlines = 10;
                for(li = 0; li < nlines; li++) {
                    ln[li] = strlen(seq[li]);
                    memcpy(lines[li], seq[li], ln[li]);
                }
            } else if(mode == 5) {
                /* Random indentation + truncated field lines */
                const char* fields[] = {
                    "Fix: No",
                    "Text: 02 ANTSTATUS=OPEN",
                    "Sats: 0",
                    "Acc: 0.00",
                    "Lat: 0.0000000",
                    "Lon: 0.0000000",
                    "Alt: 0.00",
                    "D/T: ",
                };
                int indent = (int)(xs32(&gseed) % 7u);
                int cut;
                ln[0] = strlen("==== GPS Data ====");
                memcpy(lines[0], "==== GPS Data ====", ln[0]);
                nlines = 1;
                for(li = 0; li < 8; li++) {
                    size_t fl = strlen(fields[li]);
                    size_t i;
                    for(i = 0; i < (size_t)indent && i < 8; i++) {
                        lines[nlines][i] = ' ';
                    }
                    memcpy(lines[nlines] + (size_t)indent, fields[li], fl);
                    ln[nlines] = (size_t)indent + fl;
                    if((xs32(&gseed) % 4u) == 0u && ln[nlines] > 2) {
                        cut = (int)(xs32(&gseed) % ln[nlines]);
                        ln[nlines] = (size_t)cut;
                    }
                    nlines++;
                }
            } else if(mode == 6) {
                const char* seq[] = {
                    "==== GPS Data ====",
                    "Fix: Maybe",
                    "noise",
                };
                nlines = 3;
                for(li = 0; li < nlines; li++) {
                    ln[li] = strlen(seq[li]);
                    memcpy(lines[li], seq[li], ln[li]);
                }
            } else if(mode == 7) {
                /* Missing Text */
                const char* seq[] = {
                    "==== GPS Data ====",
                    "Fix: No",
                    "Sats: 0",
                    "Acc: 0.00",
                    "Lat: 0.0000000",
                    "Lon: 0.0000000",
                    "Alt: 0.00",
                    "D/T:",
                };
                nlines = 8;
                for(li = 0; li < nlines; li++) {
                    ln[li] = strlen(seq[li]);
                    memcpy(lines[li], seq[li], ln[li]);
                }
            } else if(mode == 8) {
                /* Malformed CSV, guarantees malformed > 0 */
                const char* seq[] = {
                    "a,b,c,WIFI",
                    "==== GPS Data ====",
                    "x,y,WIFI",
                };
                nlines = 3;
                for(li = 0; li < nlines; li++) {
                    ln[li] = strlen(seq[li]);
                    memcpy(lines[li], seq[li], ln[li]);
                }
            } else if(mode == 9) {
                /* Random noise inside a block */
                size_t i;
                uint32_t noise_n;
                ln[0] = strlen("==== GPS Data ====");
                memcpy(lines[0], "==== GPS Data ====", ln[0]);
                ln[1] = strlen("  Fix: No");
                memcpy(lines[1], "  Fix: No", ln[1]);
                noise_n = 1u + (xs32(&gseed) % 40u);
                for(i = 0; i < noise_n; i++) {
                    lines[2][i] = (char)(xs32(&gseed) & 0xFFu);
                }
                ln[2] = noise_n;
                nlines = 3;
            } else if(mode == 10) {
                const char* seq[] = {
                    "StartingWardrive. Stop with stopscan",
                    "==== GPS Data ====",
                    "      Fix: No",
                    "Stopping GPS data updates",
                };
                nlines = 4;
                for(li = 0; li < nlines; li++) {
                    ln[li] = strlen(seq[li]);
                    memcpy(lines[li], seq[li], ln[li]);
                }
            } else {
                /* Random field lines / random block-header insertion */
                const char* pool[] = {
                    "==== GPS Data ====",
                    "> ==== GPS Data ====",
                    "Fix: Yes",
                    "Fix: No",
                    "Text: zz",
                    "Sats: 1",
                    "Acc: 0.01",
                    "Lat: 1.0",
                    "Lon: 2.0",
                    "Alt: 3.0",
                    "D/T: 2026-08-17 10:30:00",
                    "nope",
                    kLineBleNamed,
                };
                int count = 2 + (int)(xs32(&gseed) % 8u);
                nlines = count;
                for(li = 0; li < nlines; li++) {
                    const char* s = pool[xs32(&gseed) % 13u];
                    ln[li] = strlen(s);
                    memcpy(lines[li], s, ln[li]);
                }
            }

            for(li = 0; li < nlines; li++) {
                char* win;
                SrEvent ev;
                SrParseResult r;
                bool was_in = gp.in_gps_block;
                uint32_t mal_before = gp.lines_malformed;
                size_t n = ln[li];

                win = (char*)malloc(n == 0 ? 1U : n);
                CHECK(win != NULL);
                if(win == NULL) {
                    break;
                }
                if(n > 0) {
                    memcpy(win, lines[li], n);
                }
                memset(&ev, 0, sizeof(ev));
                r = sr_codec_marauder.feed_line(&gp, win, n, &ev);
                fed++;
                CHECK(gp.lines_seen == fed);

                if(r == SrParseNeedMore || (r == SrParseOk && ev.kind == SrEventGps)) {
                    CHECK(gp.lines_malformed == mal_before);
                }
                if(ev.kind == SrEventGps) {
                    CHECK(gp.in_gps_block == false);
                    gps_emitted++;
                }
                if(r == SrParseNeedMore) {
                    gps_needmore++;
                }
                if(was_in && !gp.in_gps_block && ev.kind != SrEventGps) {
                    gps_aborted++;
                }
                if(r == SrParseOk) {
                    g_ok++;
                } else if(r == SrParseUnknown) {
                    g_unknown++;
                } else if(r == SrParseMalformed) {
                    g_malformed++;
                }
                free(win);
            }
        }

        fprintf(
            stderr,
            "parse gps fuzz coverage: gps_emitted=%u gps_needmore=%u gps_aborted=%u "
            "ok=%u unknown=%u malformed=%u\n",
            gps_emitted,
            gps_needmore,
            gps_aborted,
            g_ok,
            g_unknown,
            g_malformed);

        /*
         * The seed is fixed, so the numbers are deterministic. Thresholds are
         * set at roughly 1/3 of the measured values:
         * gps_emitted=2938 gps_needmore=32236 gps_aborted=4595
         * ok=5921 unknown=7499 malformed=1332
         */
        CHECK(gps_emitted > 900u);
        CHECK(gps_needmore > 10000u);
        CHECK(gps_aborted > 1500u);
        CHECK(g_ok > 1800u);
        CHECK(g_unknown > 2400u);
        CHECK(g_malformed > 400u);
    }
}

/* -------------------------------------------------------------------------- */
/* T4.9 L2 side-channel observation:                                          */
/* exhaustive + cumulative + six named assertions                             */
/* -------------------------------------------------------------------------- */

static int cmdack_ends(const char* line, size_t len, const char* suf) {
    size_t n = 0;

    if(line == NULL || suf == NULL) {
        return 0;
    }
    while(suf[n] != '\0') {
        n++;
    }
    if(len < n) {
        return 0;
    }
    return memcmp(line + (len - n), suf, n) == 0;
}

/* Independent oracle: follows the D0-3 suffix rules; the literal strings come from the task card's command words; it does not call the real matcher. */
static SrCmdAckClass cmdack_oracle(const char* line, size_t len) {
    if(cmdack_ends(line, len, "#wardrive -serial") || cmdack_ends(line, len, "#wardrive")) {
        return SrCmdAckStart;
    }
    if(cmdack_ends(line, len, "#stopscan")) {
        return SrCmdAckStop;
    }
    if(cmdack_ends(line, len, "#gpsdata")) {
        return SrCmdAckGps;
    }
    if(cmdack_ends(line, len, "#info")) {
        return SrCmdAckInfo;
    }
    return SrCmdAckNone;
}

static size_t cmdack_make(char* dst, size_t cap, unsigned pi, unsigned ci, unsigned mi) {
    static const char* const pfx[] = {
        "",
        " ",
        " S",
        "> ",
        "#",
        "##",
        "x#",
        "abcdefghij0123456789",
    };
    static const char* const cmd[] = {
        "wardrive",
        "wardrive -serial",
        "stopscan",
        "gpsdata",
        "info",
        "wardriv",
        "wardrivee",
        "WARDRIVE",
        "stopscan ",
        "",
    };
    const char* a;
    const char* c;
    size_t n = 0;

    a = pfx[pi];
    c = cmd[ci];
    while(*a != '\0') {
        if(n >= cap) {
            return n;
        }
        dst[n++] = *a++;
    }
    if(mi == 0u || mi == 2u) {
        if(n < cap) {
            dst[n++] = '#';
        }
    } else if(mi == 3u) {
        if(n < cap) {
            dst[n++] = '>';
        }
    }
    while(*c != '\0') {
        if(n >= cap) {
            return n;
        }
        dst[n++] = *c++;
    }
    if(mi == 2u) {
        if(n < cap) {
            dst[n++] = ' ';
        }
    }
    return n;
}

static SrParseResult cmdack_feed(SrParser* p, const char* s, size_t n, SrEvent* ev) {
    char* w;
    SrParseResult r;

    memset(ev, 0, sizeof(*ev));
    if(n == 0u) {
        return sr_codec_marauder.feed_line(p, s, 0u, ev);
    }
    w = (char*)malloc(n);
    CHECK(w != NULL);
    if(w == NULL) {
        return SrParseUnknown;
    }
    memcpy(w, s, n);
    r = sr_codec_marauder.feed_line(p, w, n, ev);
    free(w);
    return r;
}

static SrCmdAckClass cmdack_observed(const SrParser* p) {
    unsigned i;
    unsigned hits = 0;
    SrCmdAckClass cls = SrCmdAckNone;

    CHECK(p->cmdack.count[SrCmdAckNone] == 0u);
    for(i = 1u; i < (unsigned)SrCmdAckClassCount; i++) {
        if(p->cmdack.count[i] != 0u) {
            hits++;
            cls = (SrCmdAckClass)i;
        }
    }
    if(hits == 0u) {
        CHECK(p->cmdack.rev == 0u);
        return SrCmdAckNone;
    }
    CHECK(hits == 1u);
    CHECK(p->cmdack.rev == 1u);
    CHECK(p->cmdack.count[cls] == 1u);
    return cls;
}

static void cmdack_feed_blob(const char* raw, size_t n, SrParser* parser) {
    SrLine line;
    size_t off = 0;

    sr_line_init(&line);
    memset(parser, 0, sizeof(*parser));
    while(off < n) {
        size_t used = sr_line_feed(&line, raw + off, n - off);
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
            SrEvent ev;

            memset(&ev, 0, sizeof(ev));
            (void)sr_codec_marauder.feed_line(parser, text, len, &ev);
            sr_line_consume(&line);
        }
    }
}

static void cmdack_feed_builder_echo(
    SrParser* p, const char* cmd, size_t n, SrEvent* ev, SrParseResult* r) {
    char echo[32];
    size_t body;

    CHECK(n > 0u && n < sizeof(echo));
    CHECK(cmd[n - 1u] == '\n');
    body = n - 1u;
    echo[0] = '#';
    memcpy(echo + 1, cmd, body);
    memset(p, 0, sizeof(*p));
    memset(ev, 0, sizeof(*ev));
    *r = sr_codec_marauder.feed_line(p, echo, body + 1u, ev);
}

static void test_cmdack(void) {
    unsigned start = 0;
    unsigned stop = 0;
    unsigned gps = 0;
    unsigned info = 0;
    unsigned none = 0;
    unsigned total = 0;
    unsigned pi;
    unsigned ci;
    unsigned mi;
    unsigned named_alpha = 0;
    unsigned named_beta = 0;
    unsigned named_gamma = 0;
    unsigned named_delta = 0;
    unsigned named_epsilon = 0;
    unsigned named_zeta = 0;
    SrParser acc;
    SrParser p;
    SrEvent ev;
    SrParseResult r;
    char buf[64];
    char cmdbuf[32];
    SrScanCfg cfg;
    char* gamma;

    /* A3-① 320 combinations: each one uses a brand-new parser and goes through the real feed_line. */
    for(pi = 0; pi < 8u; pi++) {
        for(ci = 0; ci < 10u; ci++) {
            for(mi = 0; mi < 4u; mi++) {
                size_t n = cmdack_make(buf, sizeof(buf), pi, ci, mi);
                SrCmdAckClass exp;
                SrCmdAckClass got;

                CHECK(n < sizeof(buf));
                memset(&p, 0, sizeof(p));
                (void)cmdack_feed(&p, buf, n, &ev);
                exp = cmdack_oracle(buf, n);
                got = cmdack_observed(&p);
                CHECK(got == exp);
                if(exp != SrCmdAckNone) {
                    CHECK(p.cmdack.rev == 1u);
                } else {
                    CHECK(p.cmdack.rev == 0u);
                }
                if(got == SrCmdAckStart) {
                    start++;
                } else if(got == SrCmdAckStop) {
                    stop++;
                } else if(got == SrCmdAckGps) {
                    gps++;
                } else if(got == SrCmdAckInfo) {
                    info++;
                } else {
                    none++;
                }
                total++;
            }
        }
    }

    printf(
        "cmdack cover: start=%u stop=%u gps=%u info=%u none=%u total=%u\n",
        start,
        stop,
        gps,
        info,
        none,
        total);
    CHECK(start == 22u);
    CHECK(stop == 11u);
    CHECK(gps == 11u);
    CHECK(info == 11u);
    CHECK(none == 265u);
    CHECK(total == 320u);

    /* A3-② the same parser is fed all 320 combinations in a row; counts accumulate and are never reset. */
    memset(&acc, 0, sizeof(acc));
    for(pi = 0; pi < 8u; pi++) {
        for(ci = 0; ci < 10u; ci++) {
            for(mi = 0; mi < 4u; mi++) {
                size_t n = cmdack_make(buf, sizeof(buf), pi, ci, mi);

                (void)cmdack_feed(&acc, buf, n, &ev);
            }
        }
    }
    CHECK(acc.cmdack.count[SrCmdAckNone] == 0u);
    CHECK(acc.cmdack.count[SrCmdAckStart] == start);
    CHECK(acc.cmdack.count[SrCmdAckStop] == stop);
    CHECK(acc.cmdack.count[SrCmdAckGps] == gps);
    CHECK(acc.cmdack.count[SrCmdAckInfo] == info);
    CHECK(acc.cmdack.rev == 55u);
    CHECK(acc.cmdack.rev == start + stop + gps + info);

    /* α: stop_sequence.txt:4, the literal " S#stopscan" line, exercises the line_eq path. */
    memset(&p, 0, sizeof(p));
    r = cmdack_feed(&p, " S#stopscan", 11u, &ev);
    CHECK(r == SrParseUnknown);
    CHECK(cmdack_observed(&p) == SrCmdAckStop);
    if(p.cmdack.count[SrCmdAckStop] == 1u && p.cmdack.rev == 1u) {
        named_alpha = 1;
    }

    /* β: trailing space, exercises substring-contains matching. */
    memset(&p, 0, sizeof(p));
    r = cmdack_feed(&p, "#wardrive ", 10u, &ev);
    CHECK(r == SrParseUnknown);
    CHECK(cmdack_observed(&p) == SrCmdAckNone);
    if(p.cmdack.rev == 0u && p.cmdack.count[SrCmdAckStart] == 0u) {
        named_beta = 1;
    }

    /* γ: an exact 9-byte window with no NUL, targeting any function that only stops at a NUL; zero ASan hits. */
    gamma = (char*)malloc(9u);
    CHECK(gamma != NULL);
    if(gamma != NULL) {
        memcpy(gamma, "#wardriveZZZZ", 9u);
        memset(&p, 0, sizeof(p));
        memset(&ev, 0, sizeof(ev));
        r = sr_codec_marauder.feed_line(&p, gamma, 9u, &ev);
        CHECK(r == SrParseUnknown);
        CHECK(ev.kind == SrEventUnknown);
        CHECK(p.cmdack.count[SrCmdAckStart] == 1u);
        CHECK(p.cmdack.rev == 1u);
        if(p.cmdack.count[SrCmdAckStart] == 1u && p.cmdack.rev == 1u) {
            named_gamma = 1;
        }
        free(gamma);
    }

    /* δ: strip the trailing \n off the builder's output, prepend #, and feed it back -- catches drift in the second copy of the literal. */
    memset(&cfg, 0, sizeof(cfg));
    {
        size_t n = sr_codec_marauder.build_start_cmd(&cfg, cmdbuf, sizeof(cmdbuf));
        cmdack_feed_builder_echo(&p, cmdbuf, n, &ev, &r);
        CHECK(r == SrParseUnknown);
        CHECK(cmdack_observed(&p) == SrCmdAckStart);
        if(p.cmdack.count[SrCmdAckStart] == 1u) {
            named_delta++;
        }
    }
    cfg.mirror_to_serial = true;
    {
        size_t n = sr_codec_marauder.build_start_cmd(&cfg, cmdbuf, sizeof(cmdbuf));
        cmdack_feed_builder_echo(&p, cmdbuf, n, &ev, &r);
        CHECK(r == SrParseUnknown);
        CHECK(cmdack_observed(&p) == SrCmdAckStart);
        if(p.cmdack.count[SrCmdAckStart] == 1u) {
            named_delta++;
        }
    }
    {
        size_t n = sr_codec_marauder.build_stop_cmd(cmdbuf, sizeof(cmdbuf));
        cmdack_feed_builder_echo(&p, cmdbuf, n, &ev, &r);
        CHECK(r == SrParseUnknown);
        CHECK(cmdack_observed(&p) == SrCmdAckStop);
        if(p.cmdack.count[SrCmdAckStop] == 1u) {
            named_delta++;
        }
    }
    {
        size_t n = sr_codec_marauder.build_gps_cmd(cmdbuf, sizeof(cmdbuf));
        cmdack_feed_builder_echo(&p, cmdbuf, n, &ev, &r);
        CHECK(r == SrParseUnknown);
        CHECK(cmdack_observed(&p) == SrCmdAckGps);
        if(p.cmdack.count[SrCmdAckGps] == 1u) {
            named_delta++;
        }
    }

    /* ε: feed all four fixtures line by line. gpsdata's stop=0 is the false-positive pin. */
    CHECK(g_wardrive != NULL && g_stop != NULL && g_gpsdata != NULL && g_startup != NULL);
    cmdack_feed_blob(g_wardrive, g_wardrive_n, &p);
    CHECK(p.cmdack.count[SrCmdAckStart] == 1u);
    CHECK(p.cmdack.count[SrCmdAckStop] == 0u);
    CHECK(p.cmdack.count[SrCmdAckGps] == 0u);
    CHECK(p.cmdack.count[SrCmdAckInfo] == 0u);
    if(p.cmdack.count[SrCmdAckStart] == 1u) {
        named_epsilon++;
    }
    cmdack_feed_blob(g_stop, g_stop_n, &p);
    CHECK(p.cmdack.count[SrCmdAckStop] == 1u);
    CHECK(p.cmdack.count[SrCmdAckStart] == 0u);
    if(p.cmdack.count[SrCmdAckStop] == 1u) {
        named_epsilon++;
    }
    cmdack_feed_blob(g_gpsdata, g_gpsdata_n, &p);
    CHECK(p.cmdack.count[SrCmdAckGps] == 1u);
    CHECK(p.cmdack.count[SrCmdAckStop] == 0u);
    if(p.cmdack.count[SrCmdAckGps] == 1u && p.cmdack.count[SrCmdAckStop] == 0u) {
        named_epsilon++;
    }
    cmdack_feed_blob(g_startup, g_startup_n, &p);
    CHECK(p.cmdack.count[SrCmdAckInfo] == 1u);
    CHECK(p.cmdack.count[SrCmdAckStart] == 0u);
    if(p.cmdack.count[SrCmdAckInfo] == 1u) {
        named_epsilon++;
    }

    /* ζ: an echoed command is still Unknown, with pointer equality; catches an echo being carved out of Unknown. */
    {
        const char* zline = "#wardrive";
        memset(&p, 0, sizeof(p));
        memset(&ev, 0, sizeof(ev));
        r = sr_codec_marauder.feed_line(&p, zline, 9u, &ev);
        CHECK(r == SrParseUnknown);
        CHECK(ev.kind == SrEventUnknown);
        CHECK(ev.u.unknown.len == 9u);
        CHECK(ev.u.unknown.text == zline);
        CHECK(p.cmdack.count[SrCmdAckStart] == 1u);
        if(r == SrParseUnknown && ev.kind == SrEventUnknown && ev.u.unknown.len == 9u &&
           ev.u.unknown.text == zline) {
            named_zeta = 1;
        }
    }

    printf(
        "cmdack named: alpha=%u beta=%u gamma=%u delta=%u epsilon=%u zeta=%u\n",
        named_alpha,
        named_beta,
        named_gamma,
        named_delta,
        named_epsilon,
        named_zeta);
    CHECK(named_alpha == 1u);
    CHECK(named_beta == 1u);
    CHECK(named_gamma == 1u);
    CHECK(named_delta == 4u);
    CHECK(named_epsilon == 4u);
    CHECK(named_zeta == 1u);
}

int test_parse_marauder_run(void) {
    sr_test_failures = 0;
    load_all();
    test_fixture_counts();
    test_oracle_fields();
    test_named();
    test_no_nul_windows();
    test_probe();
    test_firmware_feed_startup();
    test_cmds();
    test_session_and_malformed();
    test_gps_snapshots();
    test_sizeof_parser();
    test_gps_window_counts();
    test_gps_interrupt();
    test_gps_synthetic();
    test_fuzz();
    test_cmdack();
    free_all();
    return sr_test_failures;
}
