#define SR_HOST_TEST 1

#include "sr_test.h"

#include "sr_view_fmt.h"
#include "../../views/sr_view_dash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

static int streq(const char* a, const char* b) {
    size_t i = 0;
    if(a == NULL || b == NULL) {
        return a == b;
    }
    while(a[i] != '\0' && b[i] != '\0') {
        if(a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return a[i] == b[i];
}

int test_view_fmt_run(void) {
    unsigned tab_wrap = 0;
    unsigned bytes_b = 0;
    unsigned bytes_k = 0;
    unsigned bytes_m = 0;
    unsigned dur_short = 0;
    unsigned dur_long = 0;
    unsigned bars = 0;
    unsigned fit_plain = 0;
    unsigned fit_cut = 0;
    unsigned fit_sanitize = 0;
    unsigned cap_short = 0;
    unsigned nulls = 0;
    char out[32];
    size_t n;
    uint8_t t;

    sr_test_failures = 0;

    fprintf(stderr, "sizeof(SrDashModel)=%zu\n", sizeof(SrDashModel));
    CHECK(sizeof(SrDashModel) <= 768);

    /* --- tab wrap --- */
    CHECK(sr_view_tab_next(SR_VIEW_TAB_DASH, 1) == SR_VIEW_TAB_STREAM);
    CHECK(sr_view_tab_next(SR_VIEW_TAB_STREAM, 1) == SR_VIEW_TAB_GPS);
    CHECK(sr_view_tab_next(SR_VIEW_TAB_GPS, 1) == SR_VIEW_TAB_SESSION);
    CHECK(sr_view_tab_next(SR_VIEW_TAB_SESSION, 1) == SR_VIEW_TAB_DASH);
    tab_wrap++;

    CHECK(sr_view_tab_next(SR_VIEW_TAB_DASH, -1) == SR_VIEW_TAB_SESSION);
    CHECK(sr_view_tab_next(SR_VIEW_TAB_SESSION, -1) == SR_VIEW_TAB_GPS);
    CHECK(sr_view_tab_next(SR_VIEW_TAB_GPS, -1) == SR_VIEW_TAB_STREAM);
    CHECK(sr_view_tab_next(SR_VIEW_TAB_STREAM, -1) == SR_VIEW_TAB_DASH);
    tab_wrap++;

    t = sr_view_tab_next(SR_VIEW_TAB_DASH, 0);
    CHECK(t == SR_VIEW_TAB_DASH);
    t = sr_view_tab_next(SR_VIEW_TAB_STREAM, 2);
    CHECK(t == SR_VIEW_TAB_STREAM);
    t = sr_view_tab_next(SR_VIEW_TAB_GPS, -2);
    CHECK(t == SR_VIEW_TAB_GPS);
    tab_wrap++;

    CHECK(sr_view_tab_next(SR_VIEW_TAB_COUNT, 1) == SR_VIEW_TAB_DASH);
    CHECK(sr_view_tab_next(255, -1) == SR_VIEW_TAB_DASH);
    tab_wrap++;

    /* --- bytes B --- */
    n = sr_fmt_bytes(0, out, sizeof(out));
    CHECK(n == 2);
    CHECK(streq(out, "0B"));
    bytes_b++;

    n = sr_fmt_bytes(1, out, sizeof(out));
    CHECK(n == 2);
    CHECK(streq(out, "1B"));
    bytes_b++;

    n = sr_fmt_bytes(1023, out, sizeof(out));
    CHECK(n == 5);
    CHECK(streq(out, "1023B"));
    bytes_b++;

    /* --- bytes K --- */
    n = sr_fmt_bytes(1024, out, sizeof(out));
    CHECK(n == 4);
    CHECK(streq(out, "1.0K"));
    bytes_k++;

    n = sr_fmt_bytes(1536, out, sizeof(out));
    CHECK(n == 4);
    CHECK(streq(out, "1.5K"));
    bytes_k++;

    n = sr_fmt_bytes(1025, out, sizeof(out));
    CHECK(streq(out, "1.0K"));
    bytes_k++;

    n = sr_fmt_bytes(1048575u, out, sizeof(out));
    CHECK(streq(out, "1023.9K"));
    bytes_k++;

    /* --- bytes M --- */
    n = sr_fmt_bytes(1048576u, out, sizeof(out));
    CHECK(n == 4);
    CHECK(streq(out, "1.0M"));
    bytes_m++;

    n = sr_fmt_bytes(1572864u, out, sizeof(out));
    CHECK(streq(out, "1.5M"));
    bytes_m++;

    n = sr_fmt_bytes(0xFFFFFFFFu, out, sizeof(out));
    CHECK(streq(out, "4095.9M"));
    bytes_m++;

    /* --- duration short (< 1h) --- */
    n = sr_fmt_duration(0, out, sizeof(out));
    CHECK(n == 5);
    CHECK(streq(out, "00:00"));
    dur_short++;

    n = sr_fmt_duration(999, out, sizeof(out));
    CHECK(streq(out, "00:00"));
    dur_short++;

    n = sr_fmt_duration(1000, out, sizeof(out));
    CHECK(streq(out, "00:01"));
    dur_short++;

    n = sr_fmt_duration(61000, out, sizeof(out));
    CHECK(streq(out, "01:01"));
    dur_short++;

    n = sr_fmt_duration(3599999u, out, sizeof(out));
    CHECK(streq(out, "59:59"));
    dur_short++;

    /* --- duration long (>= 1h) --- */
    n = sr_fmt_duration(3600000u, out, sizeof(out));
    CHECK(streq(out, "1:00:00"));
    dur_long++;

    n = sr_fmt_duration(3661000u, out, sizeof(out));
    CHECK(streq(out, "1:01:01"));
    dur_long++;

    n = sr_fmt_duration(36000000u, out, sizeof(out));
    CHECK(streq(out, "10:00:00"));
    dur_long++;

    n = sr_fmt_duration(360000000u, out, sizeof(out));
    CHECK(streq(out, "100:00:00"));
    dur_long++;

    /* --- rssi bars: eight boundary points --- */
    CHECK(sr_fmt_rssi_bars(-55) == 4);
    bars++;
    CHECK(sr_fmt_rssi_bars(-56) == 3);
    bars++;
    CHECK(sr_fmt_rssi_bars(-70) == 3);
    bars++;
    CHECK(sr_fmt_rssi_bars(-71) == 2);
    bars++;
    CHECK(sr_fmt_rssi_bars(-80) == 2);
    bars++;
    CHECK(sr_fmt_rssi_bars(-81) == 1);
    bars++;
    CHECK(sr_fmt_rssi_bars(-90) == 1);
    bars++;
    CHECK(sr_fmt_rssi_bars(-91) == 0);
    bars++;
    CHECK(sr_fmt_rssi_bars(0) == 4);
    CHECK(sr_fmt_rssi_bars(-128) == 0);

    /* --- fit plain --- */
    n = sr_fmt_fit("hello", 5, 20, out, sizeof(out));
    CHECK(n == 5);
    CHECK(streq(out, "hello"));
    fit_plain++;

    n = sr_fmt_fit("exactly20chars!!!!!!", 20, 20, out, sizeof(out));
    CHECK(n == 20);
    CHECK(streq(out, "exactly20chars!!!!!!"));
    CHECK(out[20] == '\0');
    fit_plain++;

    /* --- fit cut --- */
    n = sr_fmt_fit("abcdefghijklmnopqrstuvwxyz", 26, 20, out, sizeof(out));
    CHECK(n == 20);
    CHECK(out[19] == '~');
    CHECK(out[20] == '\0');
    CHECK(streq(out, "abcdefghijklmnopqrs~"));
    fit_cut++;

    n = sr_fmt_fit("xyz", 3, 1, out, sizeof(out));
    CHECK(n == 1);
    CHECK(out[0] == '~');
    CHECK(out[1] == '\0');
    fit_cut++;

    /* --- fit sanitize --- */
    {
        char dirty[6];
        dirty[0] = 'A';
        dirty[1] = '\n';
        dirty[2] = '\t';
        dirty[3] = (char)0x7F;
        dirty[4] = (char)0x80;
        dirty[5] = 'Z';
        n = sr_fmt_fit(dirty, 6, 20, out, sizeof(out));
        CHECK(n == 6);
        CHECK(streq(out, "A....Z"));
        /* 0x7F is 127, > 0x7E → '.'; 0x80 → '.' */
        CHECK(out[1] == '.');
        CHECK(out[2] == '.');
        CHECK(out[3] == '.');
        CHECK(out[4] == '.');
        fit_sanitize++;
    }

    {
        char cr[2];
        cr[0] = '\r';
        cr[1] = 'x';
        n = sr_fmt_fit(cr, 2, 20, out, sizeof(out));
        CHECK(n == 2);
        CHECK(out[0] == '.');
        CHECK(out[1] == 'x');
        fit_sanitize++;
    }

    /* --- cap short --- */
    memset(out, 0xAA, sizeof(out));
    n = sr_fmt_bytes(1023, out, 3);
    CHECK(n == 2);
    CHECK(out[0] == '1');
    CHECK(out[1] == '0');
    CHECK(out[2] == '\0');
    cap_short++;

    memset(out, 0xAA, sizeof(out));
    n = sr_fmt_bytes(1023, out, 1);
    CHECK(n == 0);
    CHECK(out[0] == '\0');
    cap_short++;

    memset(out, 0xAA, sizeof(out));
    n = sr_fmt_duration(61000, out, 3);
    CHECK(n == 2);
    CHECK(out[2] == '\0');
    cap_short++;

    memset(out, 0xAA, sizeof(out));
    n = sr_fmt_fit("abcdefghij", 10, 20, out, 4);
    CHECK(n == 3);
    CHECK(out[3] == '\0');
    cap_short++;

    /* --- null / cap 0 --- */
    CHECK(sr_fmt_bytes(100, NULL, 8) == 0);
    CHECK(sr_fmt_bytes(100, out, 0) == 0);
    nulls++;

    CHECK(sr_fmt_duration(1000, NULL, 8) == 0);
    CHECK(sr_fmt_duration(1000, out, 0) == 0);
    nulls++;

    memset(out, 0xAA, sizeof(out));
    n = sr_fmt_fit(NULL, 5, 20, out, sizeof(out));
    CHECK(n == 0);
    CHECK(out[0] == '\0');
    nulls++;

    CHECK(sr_fmt_fit("abc", 3, 20, NULL, 8) == 0);
    CHECK(sr_fmt_fit("abc", 3, 20, out, 0) == 0);
    nulls++;

    /* Heap block without a NUL terminator: a correct implementation must
     * not read past the end (isomorphic regression from A5 ①). */
    {
        char* heap = malloc(8);
        CHECK(heap != NULL);
        memset(heap, 'Q', 8);
        n = sr_fmt_fit(heap, 8, 20, out, sizeof(out));
        CHECK(n == 8);
        CHECK(streq(out, "QQQQQQQQ"));
        free(heap);
        fit_plain++;
    }

    printf(
        "view_fmt cover: tab_wrap=%u bytes_b=%u bytes_k=%u bytes_m=%u "
        "dur_short=%u dur_long=%u bars=%u fit_plain=%u fit_cut=%u "
        "fit_sanitize=%u cap_short=%u null=%u\n",
        tab_wrap,
        bytes_b,
        bytes_k,
        bytes_m,
        dur_short,
        dur_long,
        bars,
        fit_plain,
        fit_cut,
        fit_sanitize,
        cap_short,
        nulls);

    CHECK(tab_wrap > 0);
    CHECK(bytes_b > 0);
    CHECK(bytes_k > 0);
    CHECK(bytes_m > 0);
    CHECK(dur_short > 0);
    CHECK(dur_long > 0);
    CHECK(bars >= 8);
    CHECK(fit_plain > 0);
    CHECK(fit_cut > 0);
    CHECK(fit_sanitize > 0);
    CHECK(cap_short > 0);
    CHECK(nulls > 0);

    /* --- T4.2 Stream: new counters go on line 2; don't touch line 1 above --- */
    {
        unsigned blen_nul = 0;
        unsigned blen_full = 0;
        unsigned clamp_shrink = 0;
        unsigned clamp_short = 0;
        unsigned scroll_down = 0;
        unsigned scroll_up = 0;
        unsigned scroll_top_edge = 0;
        unsigned scroll_bot_edge = 0;
        unsigned row_plain = 0;
        unsigned row_cut = 0;
        unsigned row_ble = 0;
        unsigned row_ch0 = 0;
        unsigned row_hidden = 0;
        unsigned row_sanitize = 0;
        unsigned row_cap = 0;
        unsigned row_null = 0;
        char longssid[20];
        char dirty[4];
        char* heap;
        size_t i;

        CHECK(sr_fmt_bounded_len(NULL, 10) == 0);
        CHECK(sr_fmt_bounded_len("abc", 0) == 0);
        CHECK(sr_fmt_bounded_len("", 8) == 0);
        CHECK(sr_fmt_bounded_len("abc", 4) == 3);
        blen_nul++;
        CHECK(sr_fmt_bounded_len("abc", 3) == 3);
        blen_nul++;

        heap = malloc(25);
        CHECK(heap != NULL);
        memset(heap, 'A', 25);
        n = sr_fmt_bounded_len(heap, 25);
        CHECK(n == 25);
        free(heap);
        blen_full++;

        CHECK(sr_stream_clamp_top(8, 10, 5) == 5);
        clamp_shrink++;
        CHECK(sr_stream_clamp_top(999, 10, 5) == 5);
        clamp_shrink++;

        CHECK(sr_stream_clamp_top(3, 0, 5) == 0);
        clamp_short++;
        CHECK(sr_stream_clamp_top(3, 10, 0) == 0);
        clamp_short++;
        CHECK(sr_stream_clamp_top(3, 5, 5) == 0);
        clamp_short++;
        CHECK(sr_stream_clamp_top(3, 4, 5) == 0);
        clamp_short++;

        CHECK(sr_stream_scroll(0, 10, 5, 1) == 1);
        scroll_down++;
        CHECK(sr_stream_scroll(4, 10, 5, 1) == 5);
        scroll_down++;

        CHECK(sr_stream_scroll(3, 10, 5, -1) == 2);
        scroll_up++;
        CHECK(sr_stream_scroll(1, 10, 5, -1) == 0);
        scroll_up++;

        CHECK(sr_stream_scroll(0, 10, 5, -1) == 0);
        scroll_top_edge++;
        CHECK(sr_stream_scroll(0, 3, 5, -1) == 0);
        scroll_top_edge++;

        CHECK(sr_stream_scroll(5, 10, 5, 1) == 5);
        scroll_bot_edge++;
        CHECK(sr_stream_scroll(5, 10, 5, 2) == 5);
        scroll_bot_edge++;

        n = sr_fmt_stream_row("Cafe", 4, false, 6, 17, out, sizeof(out));
        CHECK(n == 6);
        CHECK(streq(out, "Cafe 6"));
        row_plain++;
        n = sr_fmt_stream_row("Hi", 2, false, 11, 17, out, sizeof(out));
        CHECK(streq(out, "Hi 11"));
        row_plain++;
        n = sr_fmt_stream_row("Z", 1, false, 255, 17, out, sizeof(out));
        CHECK(streq(out, "Z 255"));
        row_plain++;

        for(i = 0; i < sizeof(longssid); i++) {
            longssid[i] = 'A';
        }
        n = sr_fmt_stream_row(longssid, 20, false, 0, 17, out, sizeof(out));
        CHECK(n == 17);
        CHECK(out[16] == '~');
        CHECK(out[17] == '\0');
        row_cut++;
        n = sr_fmt_stream_row(longssid, 20, false, 6, 17, out, sizeof(out));
        CHECK(n == 17);
        CHECK(out[14] == '~');
        CHECK(out[15] == ' ');
        CHECK(out[16] == '6');
        row_cut++;

        n = sr_fmt_stream_row("Test", 4, true, 11, 17, out, sizeof(out));
        CHECK(out[0] == '*');
        CHECK(streq(out, "*Test 11"));
        row_ble++;
        n = sr_fmt_stream_row("X", 1, true, 0, 17, out, sizeof(out));
        CHECK(streq(out, "*X"));
        row_ble++;

        n = sr_fmt_stream_row("Cafe", 4, false, 0, 17, out, sizeof(out));
        CHECK(streq(out, "Cafe"));
        CHECK(n == 4);
        row_ch0++;
        n = sr_fmt_stream_row("Cafe", 4, true, 0, 17, out, sizeof(out));
        CHECK(streq(out, "*Cafe"));
        row_ch0++;

        n = sr_fmt_stream_row("x", 0, false, 0, 17, out, sizeof(out));
        CHECK(streq(out, "(hidden)"));
        row_hidden++;
        n = sr_fmt_stream_row(NULL, 5, true, 1, 17, out, sizeof(out));
        CHECK(streq(out, "*(hidden) 1"));
        row_hidden++;

        dirty[0] = 'A';
        dirty[1] = '\n';
        dirty[2] = (char)0x7F;
        dirty[3] = 'Z';
        n = sr_fmt_stream_row(dirty, 4, false, 0, 17, out, sizeof(out));
        CHECK(n == 4);
        CHECK(streq(out, "A..Z"));
        row_sanitize++;

        memset(out, 0xAA, sizeof(out));
        n = sr_fmt_stream_row("Cafe", 4, false, 6, 17, out, 1);
        CHECK(n == 0);
        CHECK(out[0] == '\0');
        row_cap++;
        memset(out, 0xAA, sizeof(out));
        n = sr_fmt_stream_row("Cafe", 4, false, 0, 17, out, 4);
        CHECK(n == 3);
        CHECK(out[3] == '\0');
        row_cap++;

        CHECK(sr_fmt_stream_row("x", 1, false, 0, 17, NULL, 8) == 0);
        CHECK(sr_fmt_stream_row("x", 1, false, 0, 17, out, 0) == 0);
        row_null++;
        memset(out, 0xAA, sizeof(out));
        n = sr_fmt_stream_row(NULL, 0, false, 0, 17, out, sizeof(out));
        CHECK(streq(out, "(hidden)"));
        row_null++;

        printf(
            "stream cover: blen_nul=%u blen_full=%u clamp_shrink=%u clamp_short=%u "
            "scroll_down=%u scroll_up=%u scroll_top_edge=%u scroll_bot_edge=%u "
            "row_plain=%u row_cut=%u row_ble=%u row_ch0=%u row_hidden=%u "
            "row_sanitize=%u row_cap=%u row_null=%u\n",
            blen_nul,
            blen_full,
            clamp_shrink,
            clamp_short,
            scroll_down,
            scroll_up,
            scroll_top_edge,
            scroll_bot_edge,
            row_plain,
            row_cut,
            row_ble,
            row_ch0,
            row_hidden,
            row_sanitize,
            row_cap,
            row_null);

        CHECK(blen_nul > 0);
        CHECK(blen_full >= 1);
        CHECK(clamp_shrink > 0);
        CHECK(clamp_short > 0);
        CHECK(scroll_down > 0);
        CHECK(scroll_up > 0);
        CHECK(scroll_top_edge >= 2);
        CHECK(scroll_bot_edge > 0);
        CHECK(row_plain > 0);
        CHECK(row_cut >= 2);
        CHECK(row_ble > 0);
        CHECK(row_ch0 > 0);
        CHECK(row_hidden > 0);
        CHECK(row_sanitize > 0);
        CHECK(row_cap > 0);
        CHECK(row_null > 0);
    }

    /* --- T4.3 GPS: new counters go on line 3; don't touch lines 1-2 above --- */
    {
        unsigned val_fix = 0;
        unsigned val_nofix = 0;
        unsigned val_empty = 0;
        unsigned val_null = 0;
        unsigned val_cut = 0;
        unsigned val_sanitize = 0;
        unsigned stamp_dt = 0;
        unsigned stamp_blocks = 0;
        unsigned stamp_nofix = 0;
        unsigned stamp_cap = 0;
        char dirty[8];
        char longv[24];
        const char* dt19 = "2026-08-19 14:30:22";
        size_t i;

        n = sr_fmt_gps_val("22.5431234", 11, true, 20, out, sizeof(out));
        CHECK(n == 10);
        CHECK(streq(out, "22.5431234"));
        val_fix++;

        n = sr_fmt_gps_val("0.0000000", 10, false, 20, out, sizeof(out));
        CHECK(n == 2);
        CHECK(streq(out, "--"));
        CHECK(strcmp(out, "--") == 0);
        val_nofix++;

        n = sr_fmt_gps_val("114.0579876", 12, false, 20, out, sizeof(out));
        CHECK(n == 2);
        CHECK(streq(out, "--"));
        CHECK(strcmp(out, "--") == 0);
        val_nofix++;

        n = sr_fmt_gps_val("", 1, true, 20, out, sizeof(out));
        CHECK(n == 2);
        CHECK(streq(out, "--"));
        val_empty++;

        n = sr_fmt_gps_val(NULL, 8, true, 20, out, sizeof(out));
        CHECK(n == 2);
        CHECK(streq(out, "--"));
        val_null++;

        for(i = 0; i < sizeof(longv); i++) {
            longv[i] = '9';
        }
        n = sr_fmt_gps_val(longv, sizeof(longv), true, 8, out, sizeof(out));
        CHECK(n == 8);
        CHECK(out[7] == '~');
        CHECK(out[8] == '\0');
        val_cut++;

        n = sr_fmt_gps_val(longv, sizeof(longv), true, 4, out, sizeof(out));
        CHECK(n == 4);
        CHECK(out[3] == '~');
        CHECK(out[4] == '\0');
        val_cut++;

        dirty[0] = 'A';
        dirty[1] = (char)0x01;
        dirty[2] = 'B';
        dirty[3] = (char)0x80;
        dirty[4] = 'C';
        dirty[5] = (char)0xE4;
        dirty[6] = 'D';
        dirty[7] = '\0';
        n = sr_fmt_gps_val(dirty, 7, true, 20, out, sizeof(out));
        CHECK(n == 7);
        CHECK(out[1] == '.');
        CHECK(out[3] == '.');
        CHECK(out[5] == '.');
        CHECK(streq(out, "A.B.C.D"));
        val_sanitize++;

        CHECK(sr_fmt_bounded_len(dt19, 20) == 19);
        n = sr_fmt_gps_stamp(dt19, 20, true, 99, out, sizeof(out));
        CHECK(n == 19);
        CHECK(streq(out, dt19));
        stamp_dt++;

        n = sr_fmt_gps_stamp("", 1, true, 3, out, sizeof(out));
        CHECK(streq(out, "Blocks: 3"));
        stamp_blocks++;

        n = sr_fmt_gps_stamp(dt19, 20, false, 3, out, sizeof(out));
        CHECK(streq(out, "Blocks: 3"));
        CHECK(!streq(out, dt19));
        stamp_nofix++;

        memset(out, 0xAA, sizeof(out));
        n = sr_fmt_gps_stamp(dt19, 20, true, 1, out, 8);
        CHECK(n == 7);
        CHECK(out[7] == '\0');
        CHECK((unsigned char)out[8] == 0xAA);
        stamp_cap++;

        printf(
            "gps cover: val_fix=%u val_nofix=%u val_empty=%u val_null=%u val_cut=%u "
            "val_sanitize=%u stamp_dt=%u stamp_blocks=%u stamp_nofix=%u stamp_cap=%u\n",
            val_fix,
            val_nofix,
            val_empty,
            val_null,
            val_cut,
            val_sanitize,
            stamp_dt,
            stamp_blocks,
            stamp_nofix,
            stamp_cap);

        CHECK(val_fix > 0);
        CHECK(val_nofix >= 2);
        CHECK(val_empty > 0);
        CHECK(val_null > 0);
        CHECK(val_cut >= 2);
        CHECK(val_sanitize > 0);
        CHECK(stamp_dt > 0);
        CHECK(stamp_blocks > 0);
        CHECK(stamp_nofix > 0);
        CHECK(stamp_cap > 0);
    }

    /* --- T4.4 Session: new counters go on line 4; don't touch lines 1-3 above --- */
    {
        unsigned label_idle = 0;
        unsigned label_run = 0;
        unsigned label_stop = 0;
        unsigned label_oob = 0;
        unsigned fw_both = 0;
        unsigned fw_a_only = 0;
        unsigned fw_b_only = 0;
        unsigned fw_none = 0;
        unsigned fw_null = 0;
        unsigned fw_cut = 0;
        unsigned fw_sanitize = 0;
        const char* lab;
        char dirty[8];
        char longa[24];
        size_t i;

        lab = sr_fmt_session_label(0);
        CHECK(lab != NULL);
        CHECK(strcmp(lab, "Idle") == 0);
        label_idle++;

        lab = sr_fmt_session_label(1);
        CHECK(lab != NULL);
        CHECK(strcmp(lab, "Running") == 0);
        label_run++;

        lab = sr_fmt_session_label(2);
        CHECK(lab != NULL);
        CHECK(strcmp(lab, "Stopped") == 0);
        label_stop++;

        lab = sr_fmt_session_label(3);
        CHECK(lab != NULL);
        CHECK(strcmp(lab, "?") == 0);
        label_oob++;
        lab = sr_fmt_session_label(255);
        CHECK(lab != NULL);
        CHECK(strcmp(lab, "?") == 0);
        label_oob++;

        n = sr_fmt_fw_pair("Marauder", 9, "v1.14.1", 8, 20, out, sizeof(out));
        CHECK(n == 16);
        CHECK(strcmp(out, "Marauder v1.14.1") == 0);
        fw_both++;

        n = sr_fmt_fw_pair("Marauder", 9, "", 1, 20, out, sizeof(out));
        CHECK(n == 8);
        CHECK(strcmp(out, "Marauder") == 0);
        CHECK(n > 0 && out[n - 1] != ' ');
        fw_a_only++;

        n = sr_fmt_fw_pair("", 1, "v1.14.1", 8, 20, out, sizeof(out));
        CHECK(n == 7);
        CHECK(strcmp(out, "v1.14.1") == 0);
        fw_b_only++;

        n = sr_fmt_fw_pair("", 1, "", 1, 20, out, sizeof(out));
        CHECK(n == 1);
        CHECK(strcmp(out, "?") == 0);
        fw_none++;

        n = sr_fmt_fw_pair(NULL, 8, NULL, 8, 20, out, sizeof(out));
        CHECK(n == 1);
        CHECK(strcmp(out, "?") == 0);
        fw_null++;

        n = sr_fmt_fw_pair("Marauder", 9, NULL, 0, 20, out, sizeof(out));
        CHECK(n == 8);
        CHECK(strcmp(out, "Marauder") == 0);
        CHECK(n > 0 && out[n - 1] != ' ');
        fw_null++;

        for(i = 0; i < sizeof(longa); i++) {
            longa[i] = 'A';
        }
        n = sr_fmt_fw_pair(longa, sizeof(longa), "", 1, 8, out, sizeof(out));
        CHECK(n == 8);
        CHECK(out[7] == '~');
        CHECK(out[8] == '\0');
        fw_cut++;

        n = sr_fmt_fw_pair("ABCDEFGHIJK", 12, "LMNOPQRST", 10, 12, out, sizeof(out));
        CHECK(n == 12);
        CHECK(out[11] == '~');
        CHECK(out[12] == '\0');
        fw_cut++;

        dirty[0] = 'A';
        dirty[1] = (char)0x01;
        dirty[2] = 'B';
        dirty[3] = (char)0x80;
        dirty[4] = 'C';
        dirty[5] = (char)0xE4;
        dirty[6] = 'D';
        dirty[7] = '\0';
        n = sr_fmt_fw_pair(dirty, 7, "", 1, 20, out, sizeof(out));
        CHECK(n == 7);
        CHECK(out[1] == '.');
        CHECK(out[3] == '.');
        CHECK(out[5] == '.');
        CHECK(strcmp(out, "A.B.C.D") == 0);
        fw_sanitize++;

        printf(
            "session cover: label_idle=%u label_run=%u label_stop=%u label_oob=%u "
            "fw_both=%u fw_a_only=%u fw_b_only=%u fw_none=%u fw_null=%u "
            "fw_cut=%u fw_sanitize=%u\n",
            label_idle,
            label_run,
            label_stop,
            label_oob,
            fw_both,
            fw_a_only,
            fw_b_only,
            fw_none,
            fw_null,
            fw_cut,
            fw_sanitize);

        CHECK(label_idle > 0);
        CHECK(label_run > 0);
        CHECK(label_stop > 0);
        CHECK(label_oob >= 2);
        CHECK(fw_both > 0);
        CHECK(fw_a_only > 0);
        CHECK(fw_b_only > 0);
        CHECK(fw_none > 0);
        CHECK(fw_null > 0);
        CHECK(fw_cut >= 2);
        CHECK(fw_sanitize > 0);
    }

    return sr_test_failures;
}
