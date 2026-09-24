#include "sr_test.h"

#include "sr_parse_marauder.h"
#include "sr_model.h"
#include "sr_bloom.h"
#include "sr_view_fmt.h"
#include "sr_scan_ctl.h"

#include <string.h>

static SrBloom g_bloom;

static SrParseResult feed(SrParser* p, const char* line, size_t n, SrEvent* ev) {
    memset(ev, 0, sizeof(*ev));
    return sr_codec_marauder.feed_line(p, line, n, ev);
}

static void test_prefixes(void) {
    static const char* prefs[] = {
        "Up: ",
        "Rank: ",
        "Qual: ",
        "Radio: ",
        "Sess: ",
        "Busy: ",
        "Diag: ",
        "Firmware: ",
        "Version: ",
        "Hardware: ",
        "ESP-IDF: ",
        "Cfg: ",
    };
    size_t i;
    size_t j;

    for(i = 0; i < sizeof(prefs) / sizeof(prefs[0]); i++) {
        for(j = 0; j < sizeof(prefs) / sizeof(prefs[0]); j++) {
            size_t ni;
            size_t nj;
            if(i == j) {
                continue;
            }
            ni = strlen(prefs[i]);
            nj = strlen(prefs[j]);
            if(ni <= nj) {
                CHECK(memcmp(prefs[j], prefs[i], ni) != 0);
            }
        }
    }
}

static void test_cfg_lines(void) {
    SrParser parser;
    SrEvent ev;
    SrModel m;
    char line[96];
    char ssid32[33];
    char ssid40[41];

    CHECK((unsigned)SrEventRank == 13u);
    CHECK((unsigned)SrEventCfg == 14u);
    CHECK(sizeof(SrEvent) == 240u);

    memset(ssid32, 'A', 32u);
    ssid32[32] = '\0';
    memset(ssid40, 'B', 40u);
    ssid40[40] = '\0';

    memset(&parser, 0, sizeof(parser));
    CHECK(feed(&parser, "Cfg: key=1 home=0 ssid=My Network", strlen("Cfg: key=1 home=0 ssid=My Network"), &ev) == SrParseOk);
    CHECK(ev.kind == SrEventCfg);
    CHECK(ev.u.cfg.key == 1u);
    CHECK(ev.u.cfg.home == 0u);
    CHECK(strcmp(ev.u.cfg.ssid, "My Network") == 0);

    memset(&parser, 0, sizeof(parser));
    CHECK(feed(&parser, "Cfg: key=1 home=1 ssid=-", strlen("Cfg: key=1 home=1 ssid=-"), &ev) == SrParseOk);
    CHECK(ev.u.cfg.ssid[0] == '-' && ev.u.cfg.ssid[1] == '\0');
    CHECK(ev.u.cfg.home == 1u);

    snprintf(line, sizeof(line), "Cfg: key=0 home=1 ssid=%s", ssid32);
    memset(&parser, 0, sizeof(parser));
    CHECK(feed(&parser, line, strlen(line), &ev) == SrParseOk);
    CHECK(ev.u.cfg.key == 0u);
    CHECK(strlen(ev.u.cfg.ssid) == 32u);
    CHECK(memcmp(ev.u.cfg.ssid, ssid32, 32u) == 0);

    snprintf(line, sizeof(line), "Cfg: key=1 home=1 ssid=%s", ssid40);
    memset(&parser, 0, sizeof(parser));
    CHECK(feed(&parser, line, strlen(line), &ev) == SrParseOk);
    CHECK(strlen(ev.u.cfg.ssid) == 32u);
    CHECK(memcmp(ev.u.cfg.ssid, ssid40, 32u) == 0);
    CHECK(ev.u.cfg.ssid[32] == '\0');

    memset(&parser, 0, sizeof(parser));
    CHECK(feed(&parser, "Cfg: key=1 home=1", strlen("Cfg: key=1 home=1"), &ev) == SrParseUnknown);
    CHECK(ev.kind == SrEventUnknown);

    memset(&parser, 0, sizeof(parser));
    CHECK(feed(&parser, "Cfg: key=1 ssid=abc", strlen("Cfg: key=1 ssid=abc"), &ev) == SrParseUnknown);

    memset(&parser, 0, sizeof(parser));
    CHECK(feed(&parser, "Cfg: home=1 ssid=abc", strlen("Cfg: home=1 ssid=abc"), &ev) == SrParseUnknown);

    memset(&parser, 0, sizeof(parser));
    CHECK(feed(&parser, "Cfg:x", strlen("Cfg:x"), &ev) == SrParseUnknown);
    CHECK(ev.kind == SrEventUnknown);

    memset(&parser, 0, sizeof(parser));
    CHECK(feed(&parser, "Cfg: key=2 home=1 ssid=abc", strlen("Cfg: key=2 home=1 ssid=abc"), &ev) == SrParseUnknown);
    CHECK(ev.kind == SrEventUnknown);

    /* A real Up: line must not be stolen by the Cfg prefix. */
    {
        static const char kUp[] =
            "Up: up_q=1 up_last_trans=- up_disc_gps=0 up_reason=-";
        memset(&parser, 0, sizeof(parser));
        CHECK(feed(&parser, kUp, strlen(kUp), &ev) == SrParseOk);
        CHECK(ev.kind == SrEventUp);
    }

    sr_bloom_init(&g_bloom);
    sr_model_init(&m, &g_bloom, NULL);
    CHECK(m.cfg_rev == 0u);
    memset(&parser, 0, sizeof(parser));
    CHECK(feed(&parser, "Cfg: key=1 home=1 ssid=Home", strlen("Cfg: key=1 home=1 ssid=Home"), &ev) == SrParseOk);
    CHECK(sr_model_apply(&m, &ev, 3u) == true);
    CHECK(m.cfg_rev == 1u);
    CHECK(m.cfg.key == 1u);
    CHECK(strcmp(m.cfg.ssid, "Home") == 0);
    m.ap_wifi = 4u;
    sr_model_reset_session(&m, false);
    CHECK(m.ap_wifi == 0u);
    CHECK(m.cfg_rev == 1u);
    CHECK(strcmp(m.cfg.ssid, "Home") == 0);
}

static void expect_cfg(uint8_t key, uint8_t home, const char* ssid, const char* want) {
    char out[32];
    size_t n;

    n = sr_fmt_cfg_line(key, home, ssid, 20u, out, sizeof(out));
    CHECK(n == strlen(want));
    CHECK(n <= 20u);
    CHECK(strcmp(out, want) == 0);
}

static void test_cfg_line(void) {
    char out[32];

    expect_cfg(0u, 1u, "Cafe", "Key: none");
    expect_cfg(0u, 0u, "-", "Key: none");
    expect_cfg(1u, 0u, "Cafe", "Home: none");
    expect_cfg(1u, 1u, "-", "Home: none");
    expect_cfg(1u, 1u, "Cafe", "Key: set  Home: Cafe");

    CHECK(
        sr_fmt_cfg_line(1u, 1u, "LongNetworkName", 20u, out, sizeof(out)) <= 20u);
    CHECK(strcmp(out, "Key: set  Home: Lon~") == 0);
}

static void test_pending_gate(void) {
    CHECK(
        sr_pending_prompt_should_show(
            false, 1u, (uint8_t)SrSessionStopped, (uint8_t)SrScanUiIdle, false, false, true, 3u) ==
        true);
    CHECK(
        sr_pending_prompt_should_show(
            false, 1u, (uint8_t)SrSessionIdle, (uint8_t)SrScanUiIdle, false, false, true, 1u) ==
        true);
    CHECK(
        sr_pending_prompt_should_show(
            true, 1u, (uint8_t)SrSessionStopped, (uint8_t)SrScanUiIdle, false, false, true, 3u) ==
        false);
    CHECK(
        sr_pending_prompt_should_show(
            false, 0u, (uint8_t)SrSessionStopped, (uint8_t)SrScanUiIdle, false, false, true, 3u) ==
        false);
    CHECK(
        sr_pending_prompt_should_show(
            false, 1u, (uint8_t)SrSessionRunning, (uint8_t)SrScanUiIdle, false, false, true, 3u) ==
        false);
    CHECK(
        sr_pending_prompt_should_show(
            false,
            1u,
            (uint8_t)SrSessionStopped,
            (uint8_t)SrScanUiStopping,
            false,
            false,
            true,
            3u) == false);
    CHECK(
        sr_pending_prompt_should_show(
            false,
            1u,
            (uint8_t)SrSessionStopped,
            (uint8_t)SrScanUiRunning,
            false,
            false,
            true,
            3u) == false);
    CHECK(
        sr_pending_prompt_should_show(
            false, 1u, (uint8_t)SrSessionStopped, (uint8_t)SrScanUiIdle, true, false, true, 3u) ==
        false);
    CHECK(
        sr_pending_prompt_should_show(
            false, 1u, (uint8_t)SrSessionStopped, (uint8_t)SrScanUiIdle, false, true, true, 3u) ==
        false);
    CHECK(
        sr_pending_prompt_should_show(
            false, 1u, (uint8_t)SrSessionStopped, (uint8_t)SrScanUiIdle, false, false, false, 3u) ==
        false);
    CHECK(
        sr_pending_prompt_should_show(
            false, 1u, (uint8_t)SrSessionStopped, (uint8_t)SrScanUiIdle, false, false, true, 0u) ==
        false);
}

int test_cfg_parse_run(void) {
    sr_test_failures = 0;
    test_prefixes();
    test_cfg_lines();
    test_cfg_line();
    test_pending_gate();
    return sr_test_failures;
}
