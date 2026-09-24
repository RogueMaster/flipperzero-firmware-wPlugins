#include "sr_test.h"

#include "sr_model.h"
#include "sr_bloom.h"
#include "sr_rawlog.h"
#include "sr_view_fmt.h"

#include <stdio.h>
#include <string.h>

static SrBloom g_bloom;

static void add_row(SrModel* m, SrEventKind kind, const char* mac, int channel) {
    SrEvent ev;

    memset(&ev, 0, sizeof(ev));
    ev.kind = kind;
    if(kind == SrEventBleFound) {
        sr_strlcpy(ev.u.ble.bssid, sizeof(ev.u.ble.bssid), mac);
        ev.u.ble.channel = channel;
        ev.u.ble.rssi = -50;
    } else {
        sr_strlcpy(ev.u.ap.bssid, sizeof(ev.u.ap.bssid), mac);
        sr_strlcpy(ev.u.ap.ssid, sizeof(ev.u.ap.ssid), "net");
        ev.u.ap.channel = channel;
        ev.u.ap.rssi = -40;
    }
    CHECK(sr_model_apply(m, &ev, 10u) == true);
}

static void test_band_counts(void) {
    SrModel m;

    sr_bloom_init(&g_bloom);
    sr_model_init(&m, &g_bloom, NULL);

    add_row(&m, SrEventApFound, "02:00:00:00:00:01", 1);
    add_row(&m, SrEventApFound, "02:00:00:00:00:02", 14);
    add_row(&m, SrEventApFound, "02:00:00:00:00:03", 36);
    add_row(&m, SrEventApFound, "02:00:00:00:00:04", 149);
    add_row(&m, SrEventApFound, "02:00:00:00:00:05", 165);
    add_row(&m, SrEventBleFound, "06:00:00:00:00:06", 0);

    CHECK(m.ap_wifi == 5u);
    CHECK(m.ap_ble == 1u);
    CHECK(m.ap_24 == 2u);
    CHECK(m.ap_5 == 3u);

    add_row(&m, SrEventApFound, "02:00:00:00:00:07", 0);
    add_row(&m, SrEventApFound, "02:00:00:00:00:08", -3);
    CHECK(m.ap_wifi == 7u);
    CHECK(m.ap_24 == 2u);
    CHECK(m.ap_5 == 3u);
    CHECK(m.ap_ble == 1u);

    m.ap_24 = 9u;
    m.ap_5 = 8u;
    sr_model_reset_session(&m, false);
    CHECK(m.ap_24 == 0u);
    CHECK(m.ap_5 == 0u);
    CHECK(m.ap_wifi == 0u);
    CHECK(m.band_partial == false);

    m.sess.ap = 500u;
    m.sess.ble = 7u;
    m.sess.ms = 1000u;
    add_row(&m, SrEventApFound, "02:00:00:00:00:09", 6);
    CHECK(sr_model_seed_from_sess(&m, 5000u) == true);
    CHECK(m.ap_wifi == 500u);
    CHECK(m.band_partial == true);
    add_row(&m, SrEventApFound, "02:00:00:00:00:0A", 44);
    CHECK(m.band_partial == true);
    sr_model_reset_session(&m, false);
    CHECK(m.band_partial == false);
}

static void expect_band(uint32_t a, uint32_t b, size_t cols, const char* want) {
    char out[64];
    size_t n;

    memset(out, 0xA5, sizeof(out));
    n = sr_fmt_band_row(a, b, cols, out, sizeof(out));
    CHECK(n == strlen(want));
    CHECK(strcmp(out, want) == 0);
    CHECK(out[n] == '\0');
}

static void test_band_fmt(void) {
    char tiny[8];
    char out[64];
    size_t n;
    unsigned i;

    expect_band(812u, 312u, 20u, "2.4G 812  5G 312");
    expect_band(812u, 312u, 15u, "2.4G 812 5G 312");
    expect_band(812u, 312u, 14u, "2G812 5G312");
    expect_band(1000000u, 1000000u, 20u, "2G1000000 5G1000000");

    memset(out, 0xA5, sizeof(out));
    n = sr_fmt_band_row(0xFFFFFFFFu, 0xFFFFFFFFu, 20u, out, sizeof(out));
    CHECK(n == 20u);
    CHECK(strcmp(out, "2G4294967295 5G4294~") == 0);
    CHECK(out[20] == '\0');

    memset(tiny, 0xA5, sizeof(tiny));
    n = sr_fmt_band_row(0xFFFFFFFFu, 0xFFFFFFFFu, 20u, tiny, sizeof(tiny));
    CHECK(n < sizeof(tiny));
    CHECK(tiny[n] == '\0');
    for(i = (unsigned)n + 1u; i < sizeof(tiny); i++) {
        CHECK((unsigned char)tiny[i] == 0xA5u);
    }

    CHECK(sr_fmt_band_row(1u, 2u, 20u, NULL, 8u) == 0u);
    tiny[0] = 'Z';
    CHECK(sr_fmt_band_row(1u, 2u, 20u, tiny, 0u) == 0u);
    CHECK(tiny[0] == 'Z');
}

int test_band_run(void) {
    sr_test_failures = 0;
    test_band_counts();
    test_band_fmt();
    return sr_test_failures;
}
