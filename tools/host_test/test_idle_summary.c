#include "sr_test.h"

#include "sr_model.h"
#include "sr_bloom.h"
#include "sr_view_fmt.h"
#include "sr_dialect.h"

#include <string.h>

static SrBloom g_bloom;

static void test_last_status(void) {
    char out[32];
    size_t n;

    n = sr_fmt_last_status(4u, true, 0u, false, out, sizeof(out));
    CHECK(strcmp(out, "Sealed") == 0);
    CHECK(n == 6u);

    n = sr_fmt_last_status(5u, true, 1u, true, out, sizeof(out));
    CHECK(strcmp(out, "Uploading...") == 0);
    CHECK(n == 12u);

    n = sr_fmt_last_status(0u, true, 0u, true, out, sizeof(out));
    CHECK(strcmp(out, "Stopped") == 0);

    n = sr_fmt_last_status(4u, false, 3u, true, out, sizeof(out));
    CHECK(strcmp(out, "Stopped, 3 pending") == 0);

    n = sr_fmt_last_status(1u, true, 12u, true, out, sizeof(out));
    CHECK(strcmp(out, "Stopped, 12 pending") == 0);

    n = sr_fmt_last_status(2u, true, 999u, true, out, sizeof(out));
    CHECK(strcmp(out, "Stopped, 999 pending") == 0);
    CHECK(n == 20u);

    n = sr_fmt_last_status(2u, true, 1000u, true, out, sizeof(out));
    CHECK(strcmp(out, "Stopped") == 0);

    n = sr_fmt_last_status(4u, true, 9999u, true, out, sizeof(out));
    CHECK(strcmp(out, "Sealed, 9999 pending") == 0);
    CHECK(n == 20u);

    n = sr_fmt_last_status(4u, true, 10000u, true, out, sizeof(out));
    CHECK(strcmp(out, "Sealed") == 0);

    n = sr_fmt_last_status(4u, true, 3u, false, out, sizeof(out));
    CHECK(strcmp(out, "Sealed") == 0);

    n = sr_fmt_last_status(4u, true, 0u, true, out, sizeof(out));
    CHECK(strcmp(out, "Sealed") == 0);

    CHECK(sr_fmt_last_status(4u, true, 1u, true, NULL, 8u) == 0u);
    out[0] = 'Z';
    CHECK(sr_fmt_last_status(4u, true, 1u, true, out, 0u) == 0u);
    CHECK(out[0] == 'Z');
}

static void test_summary_gate(void) {
    CHECK(sr_dash_idle_summary(2u, 1u, 0u) == true);
    CHECK(sr_dash_idle_summary(2u, 0u, 4u) == true);
    CHECK(sr_dash_idle_summary(2u, 0u, 0u) == false);
    CHECK(sr_dash_idle_summary(1u, 9u, 9u) == false);
    CHECK(sr_dash_idle_summary(0u, 9u, 9u) == false);
    CHECK(sr_dash_idle_summary(2u, 0xFFFFFFFFu, 1u) == true);
}

static void test_last_elapsed(void) {
    SrModel m;
    SrEvent ev;

    sr_bloom_init(&g_bloom);
    sr_model_init(&m, &g_bloom, NULL);

    memset(&ev, 0, sizeof(ev));
    ev.kind = SrEventScanStarted;
    CHECK(sr_model_apply(&m, &ev, 1000u) == true);
    ev.kind = SrEventScanStopped;
    ev.u.stop = SrStopWifiTranRecv;
    CHECK(sr_model_apply(&m, &ev, 2500u) == true);
    CHECK(m.session == SrSessionStopped);
    CHECK(m.last_elapsed_ms == 1500u);

    ev.kind = SrEventScanStarted;
    CHECK(sr_model_apply(&m, &ev, 9000u) == true);
    CHECK(m.session == SrSessionRunning);
    CHECK(m.last_elapsed_ms == 0u);

    /* Tick wrap: stop clock is numerically less than the start clock. */
    sr_model_init(&m, &g_bloom, NULL);
    ev.kind = SrEventScanStarted;
    CHECK(sr_model_apply(&m, &ev, 0xFFFFFFF0u) == true);
    ev.kind = SrEventScanStopped;
    ev.u.stop = SrStopWifiTranRecv;
    CHECK(sr_model_apply(&m, &ev, 32u) == true);
    CHECK(m.last_elapsed_ms == 48u);

    /* Generic Marauder adopts Running from a CSV row, then stops. */
    sr_model_init(&m, &g_bloom, NULL);
    sr_strlcpy(m.firmware.version, sizeof(m.firmware.version), "v1.14.1");
    CHECK(sr_dialect_is_generic_marauder(&m.firmware) == true);
    memset(&ev, 0, sizeof(ev));
    ev.kind = SrEventApFound;
    sr_strlcpy(ev.u.ap.bssid, sizeof(ev.u.ap.bssid), "02:00:00:00:00:11");
    sr_strlcpy(ev.u.ap.ssid, sizeof(ev.u.ap.ssid), "g");
    ev.u.ap.channel = 6;
    CHECK(sr_model_apply(&m, &ev, 100u) == true);
    CHECK(m.session == SrSessionRunning);
    ev.kind = SrEventScanStopped;
    ev.u.stop = SrStopWifiTranRecv;
    CHECK(sr_model_apply(&m, &ev, 400u) == true);
    CHECK(m.session == SrSessionStopped);
    CHECK(m.last_elapsed_ms == 300u);
    CHECK(m.ap_wifi == 1u);
}

int test_idle_summary_run(void) {
    sr_test_failures = 0;
    test_last_status();
    test_summary_gate();
    test_last_elapsed();
    return sr_test_failures;
}
