#include "sr_test.h"

#include "sr_marauder_join.h"
#include "sr_model.h"
#include "sr_rawlog.h"
#include "sr_uart_cap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* load_fixture(size_t* out_n) {
    static const char* paths[] = {
        "fixtures/marauder_v117_join.txt",
        "tools/host_test/fixtures/marauder_v117_join.txt",
    };
    FILE* f = NULL;
    long sz;
    char* buf;
    size_t i;

    for(i = 0u; i < 2u; i++) {
        f = fopen(paths[i], "rb");
        if(f != NULL) {
            break;
        }
    }
    if(f == NULL) {
        fprintf(stderr, "fixture open failed\n");
        CHECK(0);
        *out_n = 0u;
        return NULL;
    }
    if(fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        CHECK(0);
        *out_n = 0u;
        return NULL;
    }
    sz = ftell(f);
    if(sz < 0) {
        fclose(f);
        CHECK(0);
        *out_n = 0u;
        return NULL;
    }
    if(fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        CHECK(0);
        *out_n = 0u;
        return NULL;
    }
    buf = malloc((size_t)sz + 1u);
    if(buf == NULL) {
        fclose(f);
        CHECK(0);
        *out_n = 0u;
        return NULL;
    }
    if(fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        CHECK(0);
        *out_n = 0u;
        return NULL;
    }
    fclose(f);
    buf[sz] = '\0';
    *out_n = (size_t)sz;
    return buf;
}

static void feed_fixture(void) {
    char* buf;
    size_t n = 0u;
    size_t i = 0u;
    unsigned aps = 0u;
    unsigned ends = 0u;
    unsigned banners = 0u;
    unsigned fails = 0u;
    unsigned usings = 0u;
    uint16_t index = 0u;
    char ssid[SR_JOIN_SSID_MAX];

    buf = load_fixture(&n);
    if(buf == NULL) {
        return;
    }
    sr_join_listen(SrJoinListenList);
    while(i < n) {
        size_t start = i;
        size_t len;

        while(i < n && buf[i] != '\n') {
            i++;
        }
        len = i - start;
        if(i < n && buf[i] == '\n') {
            i++;
        }
        if(len >= 2u && buf[start] == '#' && buf[start + 1u] == '#') {
            continue;
        }
        if(len == 0u) {
            continue;
        }
        if(sr_join_parse_ap(buf + start, len, &index, ssid, sizeof(ssid))) {
            aps++;
            if(aps == 1u) {
                CHECK(index == 0u);
                CHECK(strcmp(ssid, "HomeNet") == 0);
            }
            if(aps == 2u) {
                CHECK(index == 1u);
                CHECK(strcmp(ssid, "Cafe Guest") == 0);
            }
        }
        if(sr_join_is_list_end(buf + start, len)) {
            ends++;
        }
        if(sr_join_is_scan_banner(buf + start, len)) {
            banners++;
        }
        if(sr_join_is_fail(buf + start, len)) {
            fails++;
        }
        if(sr_join_is_using(buf + start, len)) {
            usings++;
        }
        sr_join_observe(buf + start, len);
    }
    CHECK(aps == 2u);
    CHECK(ends == 1u);
    CHECK(banners == 1u);
    CHECK(fails == 2u);
    CHECK(usings == 1u);
    CHECK(sr_join_saw_list_end() == true);
    CHECK(sr_join_ap_count() == 2u);
    CHECK(sr_join_ap_matches(0u, "HomeNet") == true);
    CHECK(sr_join_ap_matches(1u, "Cafe Guest") == true);
    CHECK(sr_join_ap_matches(0u, "OtherNet") == false);
    free(buf);
}

int test_marauder_join_run(void) {
    char cmd[96];
    char secret[] = "#join -a 3 -p s3cret-Key";
    size_t slen;
    char long_pw[65];
    char short_pw[] = "shortpw";
    char space_pw[] = "password with space";
    char hash_pw[] = "pass#word1";
    char quote_pw[] = "pass\"word1";
    char ok_pw[64];
    size_t i;
    SrModel model;
    SrRawLog raw;
    SrEvent ev;
    char rendered[400];
    uint16_t index = 99u;
    char ssid[8];

    sr_test_failures = 0;
    sr_join_listen(SrJoinListenOff);

    CHECK(strcmp(sr_join_cmd_stop(), "stopscan\n") == 0);
    CHECK(strcmp(sr_join_cmd_scan(), "scanall\n") == 0);
    CHECK(strcmp(sr_join_cmd_list(), "list -a\n") == 0);
    CHECK(strstr(sr_join_cmd_scan(), "sniffbeacon") == NULL);

    CHECK(sr_join_password_ok(NULL) == false);
    CHECK(sr_join_password_ok(short_pw) == false);
    CHECK(sr_join_password_ok(space_pw) == false);
    CHECK(sr_join_password_ok(hash_pw) == false);
    CHECK(sr_join_password_ok(quote_pw) == false);
    for(i = 0u; i < 63u; i++) {
        ok_pw[i] = 'a';
    }
    ok_pw[63] = '\0';
    CHECK(sr_join_password_ok(ok_pw) == true);
    CHECK(sr_join_build(0u, ok_pw, cmd, sizeof(cmd)) > 0u);
    CHECK(sr_join_build(0u, ok_pw, cmd, sizeof(cmd)) < 96u);
    CHECK(strstr(cmd, "join -a 0 -p ") == cmd);
    CHECK(cmd[strlen(cmd) - 1u] == '\n');
    for(i = 0u; i < 64u; i++) {
        long_pw[i] = 'b';
    }
    long_pw[64] = '\0';
    CHECK(sr_join_password_ok(long_pw) == false);
    CHECK(sr_join_build(1000u, ok_pw, cmd, sizeof(cmd)) == 0u);
    CHECK(sr_join_build(2u, short_pw, cmd, sizeof(cmd)) == 0u);

    slen = strlen(secret);
    CHECK(sr_join_redact_echo(secret, &slen, sizeof(secret)) == true);
    CHECK(strcmp(secret, "#join -a 3 -p ***") == 0);
    CHECK(strstr(secret, "s3cret-Key") == NULL);

    {
        const char* row = "[0][CH:6] HomeNet -42";
        CHECK(sr_join_parse_ap(row, strlen(row), &index, ssid, sizeof(ssid)) == true);
    }
    CHECK(index == 0u);

    feed_fixture();

    /* A stale row before this list command's echo cannot authorize a join. */
    sr_join_listen(SrJoinListenList);
    sr_join_observe("[10][CH:6] TargetNet -40", strlen("[10][CH:6] TargetNet -40"));
    sr_join_observe("0 selected", strlen("0 selected"));
    CHECK(sr_join_ap_count() == 0u);
    CHECK(sr_join_saw_list_end() == false);
    CHECK(sr_join_ap_matches(10u, "TargetNet") == false);
    sr_join_observe("> #list -a", strlen("> #list -a"));
    sr_join_observe("[10][CH:6] OtherNet -40", strlen("[10][CH:6] OtherNet -40"));
    sr_join_observe("0 selected", strlen("0 selected"));
    CHECK(sr_join_ap_matches(10u, "TargetNet") == false);
    CHECK(sr_join_ap_matches(10u, "OtherNet") == true);
    CHECK(sr_join_ap_matches(11u, "OtherNet") == false);

    sr_join_listen(SrJoinListenList);
    sr_join_observe("#list -a", strlen("#list -a"));
    sr_join_observe("[4][CH:1] HomeNet -42", strlen("[4][CH:1] HomeNet -42"));
    CHECK(sr_join_ap_matches(4u, "HomeNet") == false);
    sr_join_observe("0 selected", strlen("0 selected"));
    CHECK(sr_join_ap_matches(4u, "HomeNet") == true);

    /* A full 32-byte SSID must be compared without truncating its last byte. */
    {
        char row[80];
        char full[SR_JOIN_SSID_MAX + 1u];
        char different[SR_JOIN_SSID_MAX + 1u];
        memset(full, 'A', SR_JOIN_SSID_MAX);
        full[SR_JOIN_SSID_MAX] = '\0';
        memcpy(different, full, sizeof(full));
        different[SR_JOIN_SSID_MAX - 1u] = 'B';
        CHECK(snprintf(row, sizeof(row), "[3][CH:11] %s -50", full) > 0);
        sr_join_listen(SrJoinListenList);
        sr_join_observe("#list -a", strlen("#list -a"));
        sr_join_observe(row, strlen(row));
        sr_join_observe("0 selected", strlen("0 selected"));
        CHECK(sr_join_ap_matches(3u, full) == true);
        CHECK(sr_join_ap_matches(3u, different) == false);
    }

    sr_join_listen(SrJoinListenScan);
    {
        const char* banner = "Scanning for APs and Stations. Stop with stopscan";
        const char* using = "Using SSID: HomeNet";
        const char* fail = "Could not connect to WiFi network";
        sr_join_observe(banner, strlen(banner));
        CHECK(sr_join_saw_scan() == true);
        sr_join_listen(SrJoinListenJoin);
        sr_join_expect_index(3u);
        sr_join_observe("> #join -a 3 -p ***", strlen("> #join -a 3 -p ***"));
        sr_join_observe(using, strlen(using));
        CHECK(sr_join_saw_using() == true);
        CHECK(sr_join_saw_fail() == false);
        sr_join_observe(fail, strlen(fail));
    }
    CHECK(sr_join_saw_fail() == true);
    CHECK(sr_join_using_matches("HomeNet") == true);
    CHECK(sr_join_using_matches("OtherNet") == false);

    memset(&model, 0, sizeof(model));
    sr_rawlog_init(&raw);
    sr_model_init(&model, NULL, &raw);
    memset(&ev, 0, sizeof(ev));
    ev.kind = SrEventUnknown;
    ev.u.unknown.text = "#join -a 1 -p s3cret-Key";
    ev.u.unknown.len = strlen(ev.u.unknown.text);
    CHECK(sr_model_apply(&model, &ev, 0u) == true);
    CHECK(strstr(model.last_unknown, "s3cret-Key") == NULL);
    CHECK(strstr(model.last_unknown, "-p ***") != NULL);
    sr_rawlog_render(&raw, rendered, sizeof(rendered));
    CHECK(strstr(rendered, "s3cret-Key") == NULL);
    CHECK(strstr(rendered, "***") != NULL);

    /* Boot key import lines never reach last_unknown or the Raw ring in clear. */
    {
        static const char* const imports[] = {
            "Saved WiGLE API Name: AIDexample0001",
            "Saved WiGLE API Token: tok3nValue9",
            "> Saved WDG API Token: wdgValue77",
        };
        static const char* const values[] = {"AIDexample0001", "tok3nValue9", "wdgValue77"};
        size_t k;

        for(k = 0u; k < 3u; k++) {
            memset(&ev, 0, sizeof(ev));
            ev.kind = SrEventUnknown;
            ev.u.unknown.text = imports[k];
            ev.u.unknown.len = strlen(imports[k]);
            CHECK(sr_model_apply(&model, &ev, 0u) == true);
            CHECK(strstr(model.last_unknown, values[k]) == NULL);
            CHECK(strstr(model.last_unknown, ": ***") != NULL);
            CHECK(model.last_unknown_len == strlen(model.last_unknown));
            CHECK(
                sr_join_key_kind(model.last_unknown, model.last_unknown_len) ==
                (unsigned)(SrJoinKeyWigleName + k));
        }
        sr_rawlog_render(&raw, rendered, sizeof(rendered));
        for(k = 0u; k < 3u; k++) {
            CHECK(strstr(rendered, values[k]) == NULL);
        }
        CHECK(
            sr_join_key_kind("Saved WiFi profile", strlen("Saved WiFi profile")) == SrJoinKeyNone);
    }

    /* A cut line and a buffer with no room for the mask still lose the value. */
    {
        char cut[SR_RAW_LINE_MAX + 1u];
        size_t n = sizeof(cut) - 1u;
        static const char head[] = "Saved WiGLE API Token: ";
        char small[sizeof(head) + 1u];

        memset(cut, 'Q', sizeof(cut));
        memcpy(cut, head, sizeof(head) - 1u);
        cut[n] = '\0';
        CHECK(sr_join_redact_key(cut, &n, sizeof(cut)) == true);
        CHECK(n == sizeof(head) - 1u + 3u);
        CHECK(strchr(cut, 'Q') == NULL);

        memcpy(small, head, sizeof(head) - 1u);
        small[sizeof(head) - 1u] = 'Q';
        small[sizeof(head)] = 'Q';
        n = sizeof(small);
        CHECK(sr_join_redact_key(small, &n, sizeof(small)) == true);
        CHECK(n == sizeof(head) - 1u);
        CHECK(small[sizeof(head) - 1u] == '\0');
        CHECK(small[sizeof(head)] == '\0');
    }

    {
        char tape[256];
        size_t n = 0u;
        bool reset = false;
        const char* echo = "#join -a 3 -p s3cret-Key";
        const char* bare = "join -a 1 -p s3cret-Key";

        sr_uart_cap_disarm();
        sr_uart_cap_note(echo, strlen(echo));
        sr_uart_cap_arm(true);
        sr_uart_cap_mark("join");
        sr_uart_cap_note(echo, strlen(echo));
        sr_uart_cap_note(bare, strlen(bare));
        sr_uart_cap_note("Using SSID: HomeNet", strlen("Using SSID: HomeNet"));
        sr_uart_cap_note(
            "Saved WiGLE API Token: tok3nValue9", strlen("Saved WiGLE API Token: tok3nValue9"));
        sr_uart_cap_note(
            "Saved WiGLE API Name: AIDexample0001",
            strlen("Saved WiGLE API Name: AIDexample0001"));
        CHECK(sr_uart_cap_pull(tape, sizeof(tape), &n, &reset) == true);
        CHECK(reset == true);
        tape[n] = '\0';
        CHECK(strstr(tape, "## join\n") == tape);
        CHECK(strstr(tape, "tok3nValue9") == NULL);
        CHECK(strstr(tape, "AIDexample0001") == NULL);
        CHECK(strstr(tape, "Saved WiGLE API Token: ***\n") != NULL);
        CHECK(strstr(tape, "s3cret-Key") == NULL);
        CHECK(strstr(tape, "-p ***") != NULL);
        CHECK(strstr(tape, "Using SSID: HomeNet") != NULL);
        reset = true;
        CHECK(sr_uart_cap_pull(tape, sizeof(tape), &n, &reset) == true);
        CHECK(reset == false);
        CHECK(n == 0u);
        sr_uart_cap_disarm();
    }

    CHECK(strcmp(sr_join_cmd_check(), "channel\n") == 0);
    CHECK(SR_JOIN_CHECK_TIMEOUT_MS == 30000);
    CHECK(strlen(sr_join_fail_why(SrJoinReceiptFail)) < 24u);
    CHECK(sr_join_fail_why(SrJoinReceiptPending) == NULL);
    CHECK(sr_join_fail_why(SrJoinReceiptJoined) == NULL);

    /* Stale rows and a wrong index cannot authorize this transaction. */
    sr_join_listen(SrJoinListenJoin);
    sr_join_expect_index(3u);
    sr_join_observe("Using SSID: StaleNet", strlen("Using SSID: StaleNet"));
    sr_join_observe(
        "Could not connect to WiFi network", strlen("Could not connect to WiFi network"));
    CHECK(sr_join_saw_using() == false);
    CHECK(sr_join_receipt() == SrJoinReceiptPending);
    sr_join_observe("> #join -a 2 -p ***", strlen("> #join -a 2 -p ***"));
    sr_join_observe("Using SSID: StaleNet", strlen("Using SSID: StaleNet"));
    CHECK(sr_join_saw_using() == false);
    sr_join_observe("> #join -a 3 -p ***", strlen("> #join -a 3 -p ***"));
    sr_join_observe("Using SSID: HomeNet", strlen("Using SSID: HomeNet"));
    CHECK(sr_join_using_matches("HomeNet") == true);
    CHECK(sr_join_using_matches("OtherNet") == false);
    sr_join_observe(
        "Connecting to WiFi....> #channel", strlen("Connecting to WiFi....> #channel"));
    CHECK(sr_join_receipt() == SrJoinReceiptPending);
    sr_join_expect_check();
    sr_join_observe(
        "Connecting to WiFi....> #channel", strlen("Connecting to WiFi....> #channel"));
    CHECK(sr_join_receipt() == SrJoinReceiptJoined);

    /* A real join -a failure remains latched after channel is processed. */
    sr_join_listen(SrJoinListenJoin);
    sr_join_expect_index(3u);
    sr_join_observe("> #join -a 3 -p ***", strlen("> #join -a 3 -p ***"));
    sr_join_observe("Using SSID: HomeNet", strlen("Using SSID: HomeNet"));
    sr_join_expect_check();
    sr_join_observe(
        "Could not connect to WiFi network", strlen("Could not connect to WiFi network"));
    sr_join_observe("> #channel", strlen("> #channel"));
    CHECK(sr_join_receipt() == SrJoinReceiptFail);

    /* Text inside an SSID cannot stand in for a command echo. */
    sr_join_listen(SrJoinListenJoin);
    sr_join_expect_index(1u);
    sr_join_observe("> #join -a 1 -p ***", strlen("> #join -a 1 -p ***"));
    sr_join_expect_check();
    sr_join_observe("Using SSID: Net> #channel", strlen("Using SSID: Net> #channel"));
    CHECK(sr_join_receipt() == SrJoinReceiptPending);
    sr_join_observe("> #channel", strlen("> #channel"));
    CHECK(sr_join_receipt() == SrJoinReceiptJoined);

    /* A complete 32-byte target is accepted; an oversized one is rejected. */
    {
        char using_line[64];
        char ssid32[33];
        size_t i32;
        for(i32 = 0u; i32 < 32u; i32++)
            ssid32[i32] = 'A';
        ssid32[32] = '\0';
        CHECK(snprintf(using_line, sizeof(using_line), "Using SSID: %s", ssid32) > 0);
        sr_join_listen(SrJoinListenJoin);
        sr_join_expect_index(4u);
        sr_join_observe("> #join -a 4 -p ***", strlen("> #join -a 4 -p ***"));
        sr_join_observe(using_line, strlen(using_line));
        sr_join_expect_check();
        sr_join_observe("> #channel", strlen("> #channel"));
        CHECK(sr_join_receipt() == SrJoinReceiptJoined);
        CHECK(sr_join_using_matches(ssid32) == true);
    }
    {
        char huge[SR_RAW_LINE_MAX];
        memset(huge, 'B', sizeof(huge));
        memcpy(huge, "Using SSID: ", strlen("Using SSID: "));
        sr_join_listen(SrJoinListenJoin);
        sr_join_expect_index(4u);
        sr_join_observe("> #join -a 4 -p ***", strlen("> #join -a 4 -p ***"));
        sr_join_observe(huge, sizeof(huge));
        sr_join_expect_check();
        sr_join_observe("> #channel", strlen("> #channel"));
        CHECK(sr_join_receipt() == SrJoinReceiptPending);
    }

    return sr_test_failures;
}
