#include "sr_test.h"

#include "sr_bloom.h"
#include "sr_dialect.h"
#include "sr_model.h"
#include "sr_view_fmt.h"
#include "sr_parse_marauder.h"
#include "sr_scan_ctl.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * P5 step1. Scout Lite C5 speaks stock Marauder plus one read-only
 * `sigroam` command. Capability is the reply, not the Version string.
 * The fixture is source-derived (see its // header), not a hardware dump.
 */

_Static_assert(sizeof(SrFirmwareInfo) == 192, "sigroam_cap grew SrFirmwareInfo");
_Static_assert(
    offsetof(SrFirmwareInfo, sigroam_cap) == 167,
    "sigroam_cap must occupy the pad byte before diag_hb");

static char* load_fix(size_t* out_n) {
    static const char* const paths[] = {
        "fixtures/sigroam_c5_status.txt",
        "tools/host_test/fixtures/sigroam_c5_status.txt",
    };
    FILE* f = NULL;
    long sz;
    char* buf;
    size_t n;
    unsigned i;

    for(i = 0; i < 2u; i++) {
        f = fopen(paths[i], "rb");
        if(f != NULL) {
            break;
        }
    }
    if(f == NULL) {
        fprintf(stderr, "fixture open failed: sigroam_c5_status.txt\n");
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
    if(sz < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        CHECK(0);
        *out_n = 0;
        return NULL;
    }
    buf = (char*)malloc(sizeof(char) * ((size_t)sz + 1u));
    if(buf == NULL) {
        fclose(f);
        CHECK(0);
        *out_n = 0;
        return NULL;
    }
    n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    *out_n = n;
    return buf;
}

static void feed_text(SrParser* p, const char* text, size_t n, SrModel* m) {
    size_t i = 0;

    while(i < n) {
        size_t start = i;
        SrEvent ev;
        SrParseResult r;

        while(i < n && text[i] != '\n') {
            i++;
        }
        if(i - start >= 2u && text[start] == '/' && text[start + 1u] == '/') {
            if(i < n && text[i] == '\n') {
                i++;
            }
            continue;
        }
        memset(&ev, 0, sizeof(ev));
        r = sr_codec_marauder.feed_line(p, text + start, i - start, &ev);
        if(m != NULL && r == SrParseOk) {
            (void)sr_model_apply(m, &ev, 1000u);
        }
        if(i < n && text[i] == '\n') {
            i++;
        }
    }
}

static void test_observe(void) {
    uint8_t cap = SR_SIGROAM_CAP_NONE;
    const char* diag = "Diag: st=1 seal=00 hb=7/1/1/0/1/1";
    const char* sess = "Sess: ap=123 ble=45 ms=60000 ih=0 ihmin=0 ps=0 psmin=0";

    sr_sigroam_cap_observe(&cap, diag, strlen(diag));
    CHECK(cap == SR_SIGROAM_CAP_NONE);

    cap = SR_SIGROAM_CAP_NONE;
    sr_sigroam_cap_observe(&cap, "#info", 5u);
    sr_sigroam_cap_observe(&cap, diag, strlen(diag));
    CHECK(cap == SR_SIGROAM_CAP_NONE);

    cap = SR_SIGROAM_CAP_NONE;
    sr_sigroam_cap_observe(&cap, "#sigroam", 8u);
    CHECK(cap == SR_SIGROAM_CAP_ARMED);
    sr_sigroam_cap_observe(&cap, diag, strlen(diag));
    CHECK(cap == SR_SIGROAM_CAP_YES);

    cap = SR_SIGROAM_CAP_NONE;
    sr_sigroam_cap_observe(&cap, "> #sigroam", 10u);
    sr_sigroam_cap_observe(&cap, sess, strlen(sess));
    CHECK(cap == SR_SIGROAM_CAP_YES);

    cap = SR_SIGROAM_CAP_NONE;
    sr_sigroam_cap_observe(&cap, "#sigroam", 8u);
    sr_sigroam_cap_observe(&cap, "#info", 5u);
    CHECK(cap == SR_SIGROAM_CAP_NONE);
    sr_sigroam_cap_observe(&cap, diag, strlen(diag));
    CHECK(cap == SR_SIGROAM_CAP_NONE);

    cap = SR_SIGROAM_CAP_YES;
    sr_sigroam_cap_observe(&cap, "#info", 5u);
    sr_sigroam_cap_observe(&cap, diag, strlen(diag));
    CHECK(cap == SR_SIGROAM_CAP_YES);
}

static void test_refresh_cmd(void) {
    SrFirmwareInfo fw;

    memset(&fw, 0, sizeof(fw));
    CHECK(sr_dialect_dash_may_send_info(&fw) == false);
    CHECK(sr_dialect_dash_refresh_cmd(&fw) == NULL);
    CHECK(sr_dialect_should_probe_sigroam(&fw) == false);
    CHECK(sr_dialect_lite_status(&fw) == false);

    sr_strlcpy(fw.version, sizeof(fw.version), "v1.18.0");
    CHECK(sr_dialect_is_generic_marauder(&fw) == true);
    CHECK(sr_dialect_dash_may_send_info(&fw) == false);
    CHECK(sr_dialect_dash_refresh_cmd(&fw) == NULL);
    CHECK(sr_dialect_should_probe_sigroam(&fw) == true);
    CHECK(sr_dialect_lite_status(&fw) == false);

    fw.sigroam_cap = SR_SIGROAM_CAP_YES;
    CHECK(sr_dialect_dash_may_send_info(&fw) == false);
    {
        const char* cmd = sr_dialect_dash_refresh_cmd(&fw);

        CHECK(cmd != NULL && strcmp(cmd, "sigroam\n") == 0);
    }
    CHECK(sr_dialect_should_probe_sigroam(&fw) == false);
    CHECK(sr_dialect_lite_status(&fw) == true);

    sr_strlcpy(fw.version, sizeof(fw.version), "v1.14.1-sigroam-0");
    fw.sigroam_cap = SR_SIGROAM_CAP_YES;
    CHECK(sr_dialect_dash_may_send_info(&fw) == true);
    {
        const char* cmd = sr_dialect_dash_refresh_cmd(&fw);

        CHECK(cmd != NULL && strcmp(cmd, "info\n") == 0);
    }
    CHECK(sr_dialect_should_probe_sigroam(&fw) == false);
    CHECK(sr_dialect_lite_status(&fw) == false);
}

static void test_ident_and_status(void) {
    SrFirmwareInfo fw;
    SrQualInfo qual;
    char text[256];
    size_t n;

    CHECK(sr_ident_gps_online(false, 0u, true, 60u) == 1);
    CHECK(sr_ident_gps_online(false, 0u, true, 0u) == 0);
    CHECK(sr_ident_gps_online(true, 7u, false, 0u) == 1);
    CHECK(sr_ident_gps_online(false, 0u, false, 0u) == -1);
    CHECK(sr_ident_sd_online(false, 0u, true, 1u) == 1);
    CHECK(sr_ident_sd_online(false, 0u, true, 0u) == 0);
    CHECK(sr_ident_sd_online(true, 1u, false, 0u) == 1);
    CHECK(sr_ident_sd_online(false, 0u, false, 0u) == -1);

    memset(&fw, 0, sizeof(fw));
    memset(&qual, 0, sizeof(qual));
    sr_strlcpy(fw.firmware, sizeof(fw.firmware), "Marauder");
    sr_strlcpy(fw.version, sizeof(fw.version), "v1.18.0");
    fw.diag_seen = true;
    fw.diag_hb[0] = 7u;
    fw.diag_hb[4] = 1u;
    qual.gga = 60u;
    qual.ggafix = 55u;
    qual.sd = 1u;
    qual.sats = 7u;
    n = sr_ident_fmt(&fw, true, &qual, "PINGEQUA", "Scout Lite", text, sizeof(text));
    CHECK(n > 0u);
    CHECK(
        strstr(
            text,
            "PINGEQUA\nScout Lite\nFirmware: Marauder\nVersion: v1.18.0\nGPS online\nSD online") !=
        NULL);
    CHECK(strstr(text, "seal") == NULL);
    CHECK(strstr(text, "ihmin") == NULL);
    CHECK(strstr(text, "rank") == NULL);

    qual.gga = 0u;
    qual.sd = 0u;
    n = sr_ident_fmt(&fw, true, &qual, "PINGEQUA", "Scout Lite", text, sizeof(text));
    CHECK(n > 0u);
    CHECK(strstr(text, "GPS offline\nSD offline") != NULL);

    fw.diag_seen = false;
    n = sr_ident_fmt(&fw, false, NULL, "PINGEQUA", "Scout Lite", text, sizeof(text));
    CHECK(strstr(text, "GPS ?\nSD ?") != NULL);
}

static void test_fixture_parse(void) {
    SrParser parser;
    SrModel model;
    SrBloom bloom;
    char* buf;
    size_t n = 0;
    const char* info = "Firmware: Marauder\n"
                       "Version: v1.18.0\n";

    buf = load_fix(&n);
    CHECK(buf != NULL);
    if(buf == NULL) {
        return;
    }
    CHECK(strstr(buf, "#sigroam\n") != NULL);
    CHECK(strstr(buf, "Diag: st=1 seal=00 hb=7/1/1/0/1/1\n") != NULL);

    sr_bloom_init(&bloom);
    sr_model_init(&model, &bloom, NULL);
    memset(&parser, 0, sizeof(parser));
    feed_text(&parser, info, strlen(info), &model);
    CHECK(strcmp(model.firmware.version, "v1.18.0") == 0);
    CHECK(model.firmware.sigroam_cap == SR_SIGROAM_CAP_NONE);

    {
        /* Diag lands before Busy in printSigRoamStatus. A start is pending. */
        uint32_t at_send = model.busy_rev;
        bool pending = true;

        feed_text(&parser, buf, n, &model);
        if(model.busy_rev != at_send) {
            pending = false;
        }
        CHECK(pending == true);
        CHECK(model.busy_rev == at_send);
        CHECK(model.busy.state == 0u);
        CHECK(model.busy.seal == 0u);
    }
    CHECK(model.firmware.sigroam_cap == SR_SIGROAM_CAP_YES);
    CHECK(strcmp(model.firmware.firmware, "Marauder") == 0);
    CHECK(strcmp(model.firmware.version, "v1.18.0") == 0);
    CHECK(model.firmware.diag_seen == true);
    CHECK(model.firmware.diag_state == 1u);
    CHECK(model.firmware.diag_hb[0] == 7u);
    CHECK(model.firmware.diag_hb[4] == 1u);
    CHECK(model.sess.ap == 123u);
    CHECK(model.sess.ble == 45u);
    CHECK(model.sess.ms == 60000u);
    CHECK(model.qual.gga == 60u);
    CHECK(model.qual.ggafix == 55u);
    CHECK(model.qual.sd == 1u);
    CHECK(model.qual.sats == 7u);
    CHECK(model.radio.wifi == 1u);
    CHECK(model.radio.ble == 1u);
    CHECK(model.up.q == 2u);
    CHECK(model.cfg.key == 1u);
    CHECK(model.cfg.home == 1u);
    CHECK(strcmp(model.cfg.ssid, "OP-5G") == 0);
    /* Placeholder faces are parsed and then not shown. The latch is the
     * point of this feed: Diag after #sigroam, not after #info. */
    CHECK(model.rank.rank == 0u);

    free(buf);

    memset(&parser, 0, sizeof(parser));
    sr_model_init(&model, &bloom, NULL);
    {
        const char* via_info = "#info\n"
                               "Diag: st=1 seal=00 hb=7/1/1/0/1/1\n"
                               "Sess: ap=1 ble=2 ms=3 ih=0 ihmin=0 ps=0 psmin=0\n";
        feed_text(&parser, via_info, strlen(via_info), &model);
    }
    CHECK(model.firmware.diag_seen == true);
    CHECK(model.firmware.sigroam_cap == SR_SIGROAM_CAP_NONE);
    CHECK(model.sess.ap == 1u);
}

static void peer_set(SrModel* m, const char* ver, uint8_t cap) {
    memset(&m->firmware, 0, sizeof(m->firmware));
    sr_strlcpy(m->firmware.version, sizeof(m->firmware.version), ver);
    sr_strlcpy(m->firmware.firmware, sizeof(m->firmware.firmware), "Marauder");
    m->firmware.sigroam_cap = cap;
    m->busy.state = 0xABu;
    m->busy.seal = 0xCDu;
    m->busy_rev = 3u;
}

static void apply_busy(SrModel* m, uint8_t st) {
    SrEvent ev;

    memset(&ev, 0, sizeof(ev));
    ev.kind = SrEventBusy;
    ev.u.busy.state = st;
    ev.u.busy.seal = 0u;
    (void)sr_model_apply(m, &ev, 50u);
}

/* scene_dash.c dash_scan_tick: pending ends only when busy_rev moves. */
static bool pending_after_busy(bool pending, uint32_t busy_rev, uint32_t at_send) {
    if(busy_rev != at_send) {
        return false;
    }
    return pending;
}

static void test_busy_pending(void) {
    SrModel model;
    SrBloom bloom;
    SrParser parser;
    SrEvent ev;
    uint32_t at;
    bool pending;
    const char* stopscan = "Busy: stopscan first";

    sr_bloom_init(&bloom);
    sr_model_init(&model, &bloom, NULL);
    peer_set(&model, "v1.18.0", (uint8_t)SR_SIGROAM_CAP_YES);
    CHECK(sr_dialect_lite_status(&model.firmware) == true);
    CHECK(sr_dialect_busy_is_refusal(&model.firmware) == false);
    at = model.busy_rev;
    pending = true;
    apply_busy(&model, 0u);
    pending = pending_after_busy(pending, model.busy_rev, at);
    CHECK(pending == true);
    CHECK(model.busy_rev == at);
    CHECK(model.busy.state == 0xABu);
    CHECK(model.busy.seal == 0xCDu);
    CHECK(sr_scan_ctl_sealing_ex(false, 0u, model.busy_rev, at, model.busy.state, true) == true);

    sr_model_init(&model, &bloom, NULL);
    peer_set(&model, "v1.14.1-sigroam-0", (uint8_t)SR_SIGROAM_CAP_YES);
    CHECK(sr_dialect_lite_status(&model.firmware) == false);
    CHECK(sr_dialect_busy_is_refusal(&model.firmware) == true);
    at = model.busy_rev;
    pending = true;
    apply_busy(&model, 0u);
    pending = pending_after_busy(pending, model.busy_rev, at);
    CHECK(pending == false);
    CHECK(model.busy_rev == at + 1u);
    CHECK(model.busy.state == 0u);
    CHECK(model.busy.seal == 0u);
    CHECK(sr_scan_ctl_sealing_ex(false, 0u, model.busy_rev, at, model.busy.state, true) == false);

    sr_model_init(&model, &bloom, NULL);
    peer_set(&model, "v1.18.0", (uint8_t)SR_SIGROAM_CAP_NONE);
    CHECK(sr_dialect_lite_status(&model.firmware) == false);
    CHECK(sr_dialect_busy_is_refusal(&model.firmware) == true);
    at = model.busy_rev;
    pending = true;
    apply_busy(&model, 2u);
    pending = pending_after_busy(pending, model.busy_rev, at);
    CHECK(pending == false);
    CHECK(model.busy_rev == at + 1u);
    CHECK(model.busy.state == 2u);

    sr_model_init(&model, &bloom, NULL);
    peer_set(&model, "v1.18.0", (uint8_t)SR_SIGROAM_CAP_ARMED);
    at = model.busy_rev;
    apply_busy(&model, 1u);
    CHECK(model.busy_rev == at + 1u);

    memset(&parser, 0, sizeof(parser));
    memset(&ev, 0, sizeof(ev));
    CHECK(sr_codec_marauder.feed_line(&parser, stopscan, strlen(stopscan), &ev) == SrParseUnknown);
    CHECK(ev.kind == SrEventUnknown);
}

static void apply_qual(SrModel* m, uint32_t gga, uint32_t ggafix) {
    SrEvent ev;

    memset(&ev, 0, sizeof(ev));
    ev.kind = SrEventQual;
    ev.u.qual.gga = gga;
    ev.u.qual.ggafix = ggafix;
    ev.u.qual.sats = 7u;
    ev.u.qual.sd = 1u;
    (void)sr_model_apply(m, &ev, 1000u);
}

static void test_qual_delta(void) {
    SrModel model;
    SrBloom bloom;

    CHECK(sr_lite_counter_live(false, 0u, 10u) == -1);
    CHECK(sr_lite_counter_live(true, 10u, 11u) == 1);
    CHECK(sr_lite_counter_live(true, 11u, 11u) == 0);
    CHECK(sr_lite_counter_live(true, 5u, 4u) == 1);
    CHECK(sr_lite_counter_live(true, 0xFFFFFFFFu, 0u) == 1);

    CHECK(sr_lite_gps_online(true, false, 0u, 60u, true, 7u) == -1);
    CHECK(sr_lite_gps_online(true, true, 60u, 61u, true, 0u) == 1);
    CHECK(sr_lite_gps_online(true, true, 61u, 61u, true, 7u) == 0);
    CHECK(sr_lite_gps_online(true, true, 0xFFFFFFFFu, 0u, false, 0u) == 1);
    CHECK(sr_lite_gps_online(false, false, 0u, 0u, true, 7u) == 1);
    CHECK(sr_lite_gps_online(false, false, 0u, 0u, true, 0u) == 0);
    CHECK(sr_lite_gps_online(false, false, 0u, 0u, false, 0u) == -1);

    sr_bloom_init(&bloom);
    sr_model_init(&model, &bloom, NULL);
    peer_set(&model, "v1.18.0", (uint8_t)SR_SIGROAM_CAP_YES);
    apply_qual(&model, 10u, 4u);
    CHECK(sr_model_lite_gps_face(&model) == -1);
    CHECK(sr_model_lite_fix_face(&model) == -1);
    apply_qual(&model, 11u, 5u);
    CHECK(sr_model_lite_gps_face(&model) == 1);
    CHECK(sr_model_lite_fix_face(&model) == 1);
    apply_qual(&model, 11u, 5u);
    CHECK(sr_model_lite_gps_face(&model) == 0);
    CHECK(sr_model_lite_fix_face(&model) == 0);
    apply_qual(&model, 0u, 0u);
    CHECK(model.qual.gga == 0u);
    CHECK(sr_model_lite_gps_face(&model) == 1);
    CHECK(sr_model_lite_fix_face(&model) == 1);

    sr_model_init(&model, &bloom, NULL);
    apply_qual(&model, 0xFFFFFFFFu, 0xFFFFFFFFu);
    apply_qual(&model, 0u, 0u);
    CHECK(sr_model_lite_gps_face(&model) == 1);
    CHECK(sr_model_lite_fix_face(&model) == 1);
}

/* P5 step1c. Dash row 4 has no clock. <g> is still the ggafix delta:
 * moved -> Fix, stalled -> NoFix, no previous sample -> ?. Stale or no
 * Qual is "GPS ?". The raw counter is never copied. with_dur is ignored. */
static void test_lite_dash_row(void) {
    char out[40];
    size_t n;
    const uint32_t prev = 3141592652u;
    const uint32_t nowfix = 3141592653u;

    n = sr_fmt_lite_dash_row(
        1u, 1000u, 1000u, 15u, true, prev, nowfix, true, 10u, true, 754000u, out, sizeof(out));
    CHECK(n == strlen(out));
    CHECK(strlen(out) <= 20u);
    CHECK(strcmp(out, "Fix 15sat  Up 10") == 0);
    CHECK(strstr(out, "3141592652") == NULL);
    CHECK(strstr(out, "3141592653") == NULL);
    CHECK(strchr(out, ':') == NULL);

    n = sr_fmt_lite_dash_row(
        1u, 1000u, 1000u, 15u, true, nowfix, nowfix, true, 10u, true, 754000u, out, sizeof(out));
    CHECK(strcmp(out, "NoFix 15sat  Up 10") == 0);
    CHECK(strstr(out, "3141592653") == NULL);

    n = sr_fmt_lite_dash_row(
        1u, 1000u, 1000u, 15u, false, 0u, nowfix, true, 10u, true, 754000u, out, sizeof(out));
    CHECK(strcmp(out, "? 15sat  Up 10") == 0);
    CHECK(strstr(out, "3141592653") == NULL);

    n = sr_fmt_lite_dash_row(
        0u, 1000u, 1000u, 15u, true, prev, nowfix, true, 10u, true, 754000u, out, sizeof(out));
    CHECK(strcmp(out, "GPS ?  Up 10") == 0);
    CHECK(strstr(out, "15") == NULL);
    CHECK(strstr(out, "3141592653") == NULL);

    n = sr_fmt_lite_dash_row(
        1u,
        1000u,
        1000u + (uint32_t)SR_QUAL_STALE_MS,
        15u,
        true,
        prev,
        nowfix,
        true,
        10u,
        true,
        754000u,
        out,
        sizeof(out));
    CHECK(strcmp(out, "Fix 15sat  Up 10") == 0);

    n = sr_fmt_lite_dash_row(
        1u,
        1000u,
        1000u + (uint32_t)SR_QUAL_STALE_MS + 1u,
        15u,
        true,
        prev,
        nowfix,
        true,
        10u,
        true,
        754000u,
        out,
        sizeof(out));
    CHECK(strcmp(out, "GPS ?  Up 10") == 0);

    n = sr_fmt_lite_dash_row(
        1u, 5000u, 4999u, 15u, true, prev, nowfix, true, 10u, true, 754000u, out, sizeof(out));
    CHECK(strcmp(out, "GPS ?  Up 10") == 0);

    n = sr_fmt_lite_dash_row(
        1u,
        0xFFFFFFF0u,
        0x00000020u,
        15u,
        true,
        prev,
        nowfix,
        true,
        10u,
        true,
        754000u,
        out,
        sizeof(out));
    CHECK(strcmp(out, "Fix 15sat  Up 10") == 0);
    CHECK(strstr(out, "3141592653") == NULL);

    n = sr_fmt_lite_dash_row(
        1u, 1000u, 1000u, 15u, true, prev, nowfix, false, 99u, true, 754000u, out, sizeof(out));
    CHECK(strcmp(out, "Fix 15sat  Up ?") == 0);
    CHECK(strstr(out, "99") == NULL);

    /* Duration left this row. Idle and Running share the line. */
    n = sr_fmt_lite_dash_row(
        1u, 1000u, 1000u, 15u, true, prev, nowfix, true, 10u, false, 754000u, out, sizeof(out));
    CHECK(strcmp(out, "Fix 15sat  Up 10") == 0);
    CHECK(strchr(out, ':') == NULL);

    n = sr_fmt_lite_dash_row(
        1u, 1000u, 1000u, 15u, true, prev, nowfix, true, 10u, true, 3600000u, out, sizeof(out));
    CHECK(strcmp(out, "Fix 15sat  Up 10") == 0);
    CHECK(strlen(out) <= 20u);
    CHECK(strchr(out, ':') == NULL);

    n = sr_fmt_lite_dash_row(
        1u, 1000u, 1000u, 255u, true, 1u, 2u, true, 4294967295u, true, 36000000u, out, sizeof(out));
    CHECK(strcmp(out, "Fix 255sat") == 0);
    CHECK(strlen(out) <= 20u);
    CHECK(strstr(out, "Up") == NULL);
    CHECK(strstr(out, "4294967295") == NULL);

    n = sr_fmt_lite_dash_row(
        1u, 1000u, 1000u, 15u, true, nowfix, nowfix, true, 4294967295u, false, 0u, out, sizeof(out));
    CHECK(strcmp(out, "NoFix 15sat") == 0);
    CHECK(strlen(out) <= 20u);
    CHECK(strstr(out, "4294967295") == NULL);
    CHECK(strstr(out, "3141592653") == NULL);
}

/* P5 step1c follow-up. The Running lite row clock is a width decision:
 * full row, drop the whole "  Up <q>" segment, or no clock. 128 fits.
 * The 4 px gap is inside the function. */
static void test_lite_row_clock_fit(void) {
    /* 80 + 4 + 20 = 104. Up stays. */
    CHECK(sr_lite_row_clock_fit(80u, 60u, 20u) == SrLiteRowClockFull);
    /* 100 + 4 + 24 = 128. The full row still fits. */
    CHECK(sr_lite_row_clock_fit(100u, 70u, 24u) == SrLiteRowClockFull);
    /* 101 + 4 + 24 = 129. 70 + 4 + 24 = 98. Drop Up. */
    CHECK(sr_lite_row_clock_fit(101u, 70u, 24u) == SrLiteRowClockDropUp);
    /* 110 + 4 + 24 = 138. 100 + 4 + 24 = 128. Drop Up still fits. */
    CHECK(sr_lite_row_clock_fit(110u, 100u, 24u) == SrLiteRowClockDropUp);
    /* 120 + 4 + 24 = 148. 101 + 4 + 24 = 129. Skip the clock. */
    CHECK(sr_lite_row_clock_fit(120u, 101u, 24u) == SrLiteRowClockNone);
    /* 110 + 4 + 30 = 144. 100 + 4 + 30 = 134. Skip the clock. */
    CHECK(sr_lite_row_clock_fit(110u, 100u, 30u) == SrLiteRowClockNone);
}

/* P5 step1c section 6. Running rows between the big baseline and y62.
 * Lifted B is put_big's baseline: tab 11 + profont22 max height 20 - lift 3.
 * The ink-top clamp does not move it (top = 12, tab is 11), so B = 28.
 * That stack is the on-device "about y58 with a band, about y48 without". */
static void test_lite_status_rows(void) {
    SrLiteStatusRows rows;
    const int32_t lifted_b = 28;

    CHECK(SR_LITE_STATUS_Y == 62);
    CHECK(SR_LITE_ROW_MIN_GAP == 9);

    /* 28 + 34/3 = 39, 28 + 68/3 = 50. Gaps 11, 11, 12. */
    rows = sr_lite_status_rows(lifted_b, true);
    CHECK(rows.spread == true);
    CHECK(rows.draw_band == true);
    CHECK(rows.ap_y == 39);
    CHECK(rows.band_y == 50);
    CHECK(rows.status_y == (int32_t)SR_LITE_STATUS_Y);

    /* 28 + 17 = 45. Gaps 17, 17. */
    rows = sr_lite_status_rows(lifted_b, false);
    CHECK(rows.spread == true);
    CHECK(rows.draw_band == false);
    CHECK(rows.ap_y == 45);
    CHECK(rows.band_y == 0);
    CHECK(rows.status_y == (int32_t)SR_LITE_STATUS_Y);

    /* Floor still holds. gap 27, three gaps of 9. */
    rows = sr_lite_status_rows(35, true);
    CHECK(rows.spread == true);
    CHECK(rows.draw_band == true);
    CHECK(rows.ap_y == 44);
    CHECK(rows.band_y == 53);
    CHECK(rows.status_y == 62);

    /* One under the floor. gap 26, first gap 8. Old B+10 / B+20 stack. */
    rows = sr_lite_status_rows(36, true);
    CHECK(rows.spread == false);
    CHECK(rows.draw_band == false);
    CHECK(rows.ap_y == 46);
    CHECK(rows.band_y == 0);
    CHECK(rows.status_y == 56);

    /* No band, gap 18, both gaps 9. */
    rows = sr_lite_status_rows(44, false);
    CHECK(rows.spread == true);
    CHECK(rows.draw_band == false);
    CHECK(rows.ap_y == 53);
    CHECK(rows.status_y == 62);

    /* No band, gap 17, first gap 8. Old stack, status at B+20. */
    rows = sr_lite_status_rows(45, false);
    CHECK(rows.spread == false);
    CHECK(rows.draw_band == false);
    CHECK(rows.ap_y == 55);
    CHECK(rows.status_y == 65);

    /* (2*gap)/3, not 2*(gap/3). gap 32 -> 10 and 21, not 10 and 20. */
    rows = sr_lite_status_rows(30, true);
    CHECK(rows.spread == true);
    CHECK(rows.draw_band == true);
    CHECK(rows.ap_y == 40);
    CHECK(rows.band_y == 51);
    CHECK(rows.status_y == 62);

    /* Odd gap truncates toward zero. gap 33 -> B+16, not B+17. */
    rows = sr_lite_status_rows(29, false);
    CHECK(rows.spread == true);
    CHECK(rows.draw_band == false);
    CHECK(rows.ap_y == 45);
    CHECK(rows.status_y == 62);

    /* Fallback status can be 62. That is B+20, not the pinned row. */
    rows = sr_lite_status_rows(42, true);
    CHECK(rows.spread == false);
    CHECK(rows.draw_band == false);
    CHECK(rows.ap_y == 52);
    CHECK(rows.status_y == 62);
}

/* P5 step1c Probe Ok page. Bold title, description, firmware version,
 * one GPS/SD line. fix/nofix is the ggafix delta, never the counter. */
static void test_lite_probe_page(void) {
    SrFirmwareInfo fw;
    SrQualInfo qual;
    char text[256];
    size_t n;
    const uint32_t prev = 3141592652u;
    const uint32_t nowfix = 3141592653u;
    const char* one = "\x1b#PINGEQUA Scout Lite\n"
                      "Dual-band Wardriver\n"
                      "Marauder v1.18.0\n"
                      "GPS:15sat fix  SD:ok";
    const char* split = "\x1b#WWWWWWWW\n"
                        "\x1b#Scout Lite\n"
                        "Dual-band Wardriver\n"
                        "Marauder v1.18.0\n"
                        "GPS:15sat fix  SD:ok";

    memset(&fw, 0, sizeof(fw));
    memset(&qual, 0, sizeof(qual));
    sr_strlcpy(fw.firmware, sizeof(fw.firmware), "Marauder");
    sr_strlcpy(fw.version, sizeof(fw.version), "v1.18.0");
    fw.sigroam_cap = (uint8_t)SR_SIGROAM_CAP_YES;
    qual.ggafix = nowfix;
    qual.sd = 1u;
    qual.sats = 15u;

    n = sr_ident_fmt_lite(
        &fw, true, &qual, true, prev, 1000u, 1000u, "PINGEQUA", "Scout Lite", text, sizeof(text));
    CHECK(n == strlen(text));
    CHECK(strcmp(text, one) == 0);
    CHECK(strstr(text, "Firmware:") == NULL);
    CHECK(strstr(text, "Version:") == NULL);
    CHECK(strstr(text, "SigRoam") == NULL);
    CHECK(strstr(text, "3141592652") == NULL);
    CHECK(strstr(text, "3141592653") == NULL);

    n = sr_ident_fmt_lite(
        &fw, true, &qual, true, prev, 1000u, 1000u, "WWWWWWWW", "Scout Lite", text, sizeof(text));
    CHECK(strcmp(text, split) == 0);

    n = sr_ident_fmt_lite(
        &fw, true, &qual, true, nowfix, 1000u, 1000u, "PINGEQUA", "Scout Lite", text, sizeof(text));
    CHECK(strstr(text, "\nGPS:15sat nofix  SD:ok") != NULL);
    CHECK(strstr(text, "3141592653") == NULL);

    n = sr_ident_fmt_lite(
        &fw, true, &qual, false, 0u, 1000u, 1000u, "PINGEQUA", "Scout Lite", text, sizeof(text));
    CHECK(strstr(text, "\nGPS:?  SD:ok") != NULL);
    CHECK(strstr(text, "15sat") == NULL);
    CHECK(strstr(text, "3141592653") == NULL);

    n = sr_ident_fmt_lite(
        &fw,
        true,
        &qual,
        true,
        prev,
        1000u,
        1000u + (uint32_t)SR_QUAL_STALE_MS + 1u,
        "PINGEQUA",
        "Scout Lite",
        text,
        sizeof(text));
    CHECK(strstr(text, "\nGPS:?  SD:ok") != NULL);

    qual.sd = 0u;
    n = sr_ident_fmt_lite(
        &fw, true, &qual, true, prev, 1000u, 1000u, "PINGEQUA", "Scout Lite", text, sizeof(text));
    CHECK(strstr(text, "\nSD:no") != NULL);
    CHECK(strstr(text, "GPS:") == NULL);

    n = sr_ident_fmt_lite(
        &fw, false, NULL, false, 0u, 0u, 0u, "PINGEQUA", "Scout Lite", text, sizeof(text));
    CHECK(strstr(text, "\nGPS:?  SD:?") != NULL);
    CHECK(strstr(text, "online") == NULL);
    CHECK(strstr(text, "offline") == NULL);
}

static void test_lite_sess_lines(void) {
    char out[40];
    size_t n;

    n = sr_fmt_lite_sess_brand("PINGEQUA", out, sizeof(out));
    CHECK(n == strlen(out));
    CHECK(strcmp(out, "PINGEQUA Scout Lite") == 0);
    CHECK(strlen(out) <= 20u);
    CHECK(strstr(out, "SigRoam") == NULL);

    n = sr_fmt_lite_sess_fw("Marauder", "v1.18.0", out, sizeof(out));
    CHECK(strcmp(out, "Marauder v1.18.0") == 0);
    CHECK(strlen(out) <= 20u);
    CHECK(strstr(out, "Firmware unknown") == NULL);
    CHECK(strstr(out, "Use Probe firmware") == NULL);
}

int test_sigroam_cap_run(void) {
    sr_test_failures = 0;
    test_observe();
    test_refresh_cmd();
    test_ident_and_status();
    test_fixture_parse();
    test_busy_pending();
    test_qual_delta();
    test_lite_dash_row();
    test_lite_row_clock_fit();
    test_lite_status_rows();
    test_lite_probe_page();
    test_lite_sess_lines();
    return sr_test_failures;
}
