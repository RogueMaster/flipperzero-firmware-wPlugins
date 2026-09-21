#include "sr_test.h"

#include "sr_dialect.h"
#include "sr_model.h"
#include "sr_bloom.h"
#include "sr_types.h"

#include <string.h>

static SrModel g_model;
static SrBloom g_bloom;

static void ev_clear(SrEvent* ev) {
    memset(ev, 0, sizeof(*ev));
}

static void ev_ap(SrEvent* ev, SrEventKind kind, const char* mac) {
    SrApRecord* r;

    ev_clear(ev);
    ev->kind = kind;
    r = (kind == SrEventBleFound) ? &ev->u.ble : &ev->u.ap;
    sr_strlcpy(r->bssid, sizeof(r->bssid), mac);
    r->channel = (kind == SrEventBleFound) ? 0 : 6;
    r->rssi = -50;
    r->radio = (kind == SrEventBleFound) ? SrRadioBle : SrRadioWifi;
}

static void set_version(const char* ver) {
    g_model.firmware.version[0] = '\0';
    if(ver != NULL && ver[0] != '\0') {
        sr_strlcpy(g_model.firmware.version, sizeof(g_model.firmware.version), ver);
    }
}

static void fresh(void) {
    sr_bloom_init(&g_bloom);
    sr_model_init(&g_model, &g_bloom, NULL);
}

int test_dialect_run(void) {
    SrEvent ev;
    SrFirmwareInfo fw;
    uint32_t illegal0;
    uint32_t rev0;

    sr_test_failures = 0;

    memset(&fw, 0, sizeof(fw));
    CHECK(sr_dialect_is_sigroam(NULL) == false);
    CHECK(sr_dialect_is_generic_marauder(NULL) == false);
    CHECK(sr_dialect_is_sigroam(&fw) == false);
    CHECK(sr_dialect_is_generic_marauder(&fw) == false);

    sr_strlcpy(fw.version, sizeof(fw.version), "v1.14.1-sigroam-0");
    CHECK(sr_dialect_is_sigroam(&fw) == true);
    CHECK(sr_dialect_is_generic_marauder(&fw) == false);

    sr_strlcpy(fw.version, sizeof(fw.version), "v1.17.0");
    CHECK(sr_dialect_is_sigroam(&fw) == false);
    CHECK(sr_dialect_is_generic_marauder(&fw) == true);

    sr_strlcpy(fw.version, sizeof(fw.version), "v1.14.1");
    CHECK(sr_dialect_is_generic_marauder(&fw) == true);
    CHECK(sr_dialect_is_sigroam(&fw) == false);

    memset(&fw, 0, sizeof(fw));
    CHECK(sr_dialect_dash_may_send_info(NULL) == false);
    CHECK(sr_dialect_dash_may_send_info(&fw) == false);
    sr_strlcpy(fw.version, sizeof(fw.version), "v1.17.0");
    CHECK(sr_dialect_dash_may_send_info(&fw) == false);
    sr_strlcpy(fw.version, sizeof(fw.version), "v1.14.1");
    CHECK(sr_dialect_dash_may_send_info(&fw) == false);
    sr_strlcpy(fw.version, sizeof(fw.version), "v1.14.1-sigroam-0");
    CHECK(sr_dialect_dash_may_send_info(&fw) == true);

    memset(&fw, 0, sizeof(fw));
    CHECK(sr_dialect_probe_should_clear_show_info(&fw) == false);
    CHECK(sr_dialect_show_info_clear_on_start(&fw, false) == SrShowInfoClearNone);
    CHECK(sr_dialect_show_info_clear_on_start(&fw, true) == SrShowInfoClearNone);
    sr_strlcpy(fw.version, sizeof(fw.version), "v1.17.0");
    CHECK(sr_dialect_probe_should_clear_show_info(&fw) == true);
    CHECK(sr_dialect_show_info_clear_on_start(&fw, false) == SrShowInfoClearSendStop);
    CHECK(sr_dialect_show_info_clear_on_start(&fw, true) == SrShowInfoClearWait);
    sr_strlcpy(fw.version, sizeof(fw.version), "v1.14.1-sigroam-0");
    CHECK(sr_dialect_probe_should_clear_show_info(&fw) == false);
    CHECK(sr_dialect_show_info_clear_on_start(&fw, false) == SrShowInfoClearNone);
    CHECK(sr_dialect_show_info_clear_on_start(&fw, true) == SrShowInfoClearNone);
    memset(&fw, 0, sizeof(fw));
    CHECK(sr_dialect_needs_show_info_clear(&fw, false) == false);
    CHECK(sr_dialect_needs_show_info_clear(&fw, true) == false);
    CHECK(sr_dialect_show_info_clear_on_start_ex(&fw, false, true) == SrShowInfoClearNone);
    CHECK(sr_dialect_show_info_clear_on_start_ex(&fw, true, true) == SrShowInfoClearNone);
    sr_strlcpy(fw.version, sizeof(fw.version), "v1.14.1-sigroam-0");
    CHECK(sr_dialect_needs_show_info_clear(&fw, true) == false);
    CHECK(sr_dialect_show_info_clear_on_start_ex(&fw, false, true) == SrShowInfoClearNone);
    sr_strlcpy(fw.version, sizeof(fw.version), "v1.17.0");
    CHECK(sr_dialect_needs_show_info_clear(&fw, false) == true);
    CHECK(sr_dialect_show_info_clear_done(0u, 0u) == false);
    CHECK(sr_dialect_show_info_clear_done(1u, 0u) == true);
    CHECK(sr_dialect_show_info_clear_done(0u, 0xFFFFFFFFu) == true);

    /* Empty version: historical strict rules. Idle + StopWifi is illegal. */
    fresh();
    illegal0 = g_model.illegal_trans;
    rev0 = g_model.session_rev;
    ev_clear(&ev);
    ev.kind = SrEventScanStopped;
    ev.u.stop = SrStopWifiTranRecv;
    CHECK(sr_model_apply(&g_model, &ev, 1) == false);
    CHECK(g_model.session == SrSessionIdle);
    CHECK(g_model.session_rev == rev0);
    CHECK(g_model.illegal_trans == illegal0 + 1u);
    CHECK(g_model.wifi_stop_rev == 1u);

    /* SigRoam: Idle AP row does not start a session. */
    fresh();
    set_version("v1.14.1-sigroam-0");
    ev_ap(&ev, SrEventApFound, "AA:BB:CC:DD:EE:01");
    CHECK(sr_model_apply(&g_model, &ev, 2) == true);
    CHECK(g_model.session == SrSessionIdle);
    CHECK(g_model.ap_wifi == 1u);

    /* SigRoam: duplicate ScanStarted while Running still counts illegal. */
    fresh();
    set_version("v1.14.1-sigroam-0");
    ev_clear(&ev);
    ev.kind = SrEventScanStarted;
    CHECK(sr_model_apply(&g_model, &ev, 3) == true);
    CHECK(g_model.session == SrSessionRunning);
    illegal0 = g_model.illegal_trans;
    rev0 = g_model.session_rev;
    CHECK(sr_model_apply(&g_model, &ev, 4) == false);
    CHECK(g_model.illegal_trans == illegal0 + 1u);
    CHECK(g_model.session_rev == rev0);
    CHECK(g_model.session == SrSessionRunning);

    /* Generic: Idle AP row adopts Running. */
    fresh();
    set_version("v1.17.0");
    illegal0 = g_model.illegal_trans;
    ev_ap(&ev, SrEventApFound, "AA:BB:CC:DD:EE:02");
    CHECK(sr_model_apply(&g_model, &ev, 5) == true);
    CHECK(g_model.session == SrSessionRunning);
    CHECK(g_model.session_rev == 1u);
    CHECK(g_model.illegal_trans == illegal0);
    CHECK(g_model.ap_wifi == 1u);

    /* Generic: Idle BLE row also adopts Running. */
    fresh();
    set_version("v1.17.0");
    ev_ap(&ev, SrEventBleFound, "aa:bb:cc:dd:ee:03");
    CHECK(sr_model_apply(&g_model, &ev, 6) == true);
    CHECK(g_model.session == SrSessionRunning);
    CHECK(g_model.ap_ble == 1u);

    /* Generic: Idle + StopWifi confirms Stopped, no illegal. */
    fresh();
    set_version("v1.17.0");
    illegal0 = g_model.illegal_trans;
    ev_clear(&ev);
    ev.kind = SrEventScanStopped;
    ev.u.stop = SrStopWifiTranRecv;
    CHECK(sr_model_apply(&g_model, &ev, 7) == true);
    CHECK(g_model.session == SrSessionStopped);
    CHECK(g_model.session_rev == 1u);
    CHECK(g_model.illegal_trans == illegal0);
    CHECK(g_model.wifi_stop_rev == 1u);

    /* Generic: duplicate ScanStarted while Running is a no-op. */
    fresh();
    set_version("v1.17.0");
    ev_clear(&ev);
    ev.kind = SrEventScanStarted;
    CHECK(sr_model_apply(&g_model, &ev, 8) == true);
    illegal0 = g_model.illegal_trans;
    rev0 = g_model.session_rev;
    CHECK(sr_model_apply(&g_model, &ev, 9) == false);
    CHECK(g_model.illegal_trans == illegal0);
    CHECK(g_model.session_rev == rev0);
    CHECK(g_model.session == SrSessionRunning);

    /* Generic: Stopped + late AP row must not reopen the session. */
    fresh();
    set_version("v1.17.0");
    ev_clear(&ev);
    ev.kind = SrEventScanStarted;
    CHECK(sr_model_apply(&g_model, &ev, 10) == true);
    ev.kind = SrEventScanStopped;
    ev.u.stop = SrStopWifiTranRecv;
    CHECK(sr_model_apply(&g_model, &ev, 11) == true);
    CHECK(g_model.session == SrSessionStopped);
    rev0 = g_model.session_rev;
    illegal0 = g_model.illegal_trans;
    ev_ap(&ev, SrEventApFound, "AA:BB:CC:DD:EE:04");
    CHECK(sr_model_apply(&g_model, &ev, 12) == true);
    CHECK(g_model.session == SrSessionStopped);
    CHECK(g_model.session_rev == rev0);
    CHECK(g_model.illegal_trans == illegal0);

    /* Generic: second WiFi stop while already Stopped is a no-op. */
    illegal0 = g_model.illegal_trans;
    rev0 = g_model.session_rev;
    ev_clear(&ev);
    ev.kind = SrEventScanStopped;
    ev.u.stop = SrStopWifiTranRecv;
    CHECK(sr_model_apply(&g_model, &ev, 13) == false);
    CHECK(g_model.session == SrSessionStopped);
    CHECK(g_model.session_rev == rev0);
    CHECK(g_model.illegal_trans == illegal0);
    CHECK(g_model.wifi_stop_rev == 2u);

    /* Generic still uses ADR-020 for GPS stop while idle. */
    fresh();
    set_version("v1.17.0");
    illegal0 = g_model.illegal_trans;
    ev_clear(&ev);
    ev.kind = SrEventScanStopped;
    ev.u.stop = SrStopGpsUpdates;
    CHECK(sr_model_apply(&g_model, &ev, 14) == false);
    CHECK(g_model.session == SrSessionIdle);
    CHECK(g_model.gps_stop_rev == 1u);
    CHECK(g_model.wifi_stop_rev == 0u);
    CHECK(g_model.illegal_trans == illegal0);

    CHECK(sizeof(SrModel) == 3424);

    return sr_test_failures;
}
