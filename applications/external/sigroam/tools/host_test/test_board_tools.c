#include "sr_test.h"

#include "sr_board_tools.h"
#include "sr_mau_logs.h"

#include <stdio.h>
#include <string.h>

static void feed(const char* s) {
    sr_bt_feed(s, strlen(s));
}

static void test_gate_and_peer(void) {
    CHECK(sr_bt_gate(0u, false, false) == SrBtGateOk);
    CHECK(sr_bt_gate(2u, false, false) == SrBtGateOk);
    CHECK(sr_bt_gate(1u, false, false) == SrBtGateScan);
    CHECK(sr_bt_gate(1u, true, true) == SrBtGateScan);
    CHECK(sr_bt_gate(0u, true, false) == SrBtGateUpload);
    CHECK(sr_bt_gate(2u, false, true) == SrBtGateJoin);

    CHECK(sr_bt_peer(false, false) == SrBtPeerEmpty);
    CHECK(sr_bt_peer(false, true) == SrBtPeerEmpty);
    CHECK(sr_bt_peer(true, true) == SrBtPeerLite);
    CHECK(sr_bt_peer(true, false) == SrBtPeerOther);
}

static void test_quote_and_cmds(void) {
    char cmd[80];
    char ssid32[33];
    unsigned i;

    CHECK(sr_bt_quote_add_cmd("cafe", 4u, cmd, sizeof(cmd)) == SrBtQuoteOk);
    CHECK(strcmp(cmd, "sigroam ignore add \"cafe\"\n") == 0);
    CHECK(sr_bt_quote_add_cmd("my net", 6u, cmd, sizeof(cmd)) == SrBtQuoteOk);
    CHECK(strcmp(cmd, "sigroam ignore add \"my net\"\n") == 0);
    CHECK(sr_bt_quote_add_cmd("a\"b", 3u, cmd, sizeof(cmd)) == SrBtQuoteOk);
    CHECK(strcmp(cmd, "sigroam ignore add 'a\"b'\n") == 0);
    CHECK(sr_bt_quote_add_cmd("a'b", 3u, cmd, sizeof(cmd)) == SrBtQuoteOk);
    CHECK(strcmp(cmd, "sigroam ignore add \"a'b\"\n") == 0);
    CHECK(sr_bt_quote_add_cmd("a\"b'c", 5u, cmd, sizeof(cmd)) == SrBtQuoteReject);
    CHECK(sr_bt_quote_add_cmd("", 0u, cmd, sizeof(cmd)) == SrBtQuoteReject);
    CHECK(sr_bt_quote_add_cmd(NULL, 1u, cmd, sizeof(cmd)) == SrBtQuoteReject);
    for(i = 0u; i < 32u; i++) {
        ssid32[i] = 'a';
    }
    ssid32[32] = '\0';
    CHECK(sr_bt_quote_add_cmd(ssid32, 32u, cmd, sizeof(cmd)) == SrBtQuoteOk);
    CHECK(sr_bt_quote_add_cmd(ssid32, 33u, cmd, sizeof(cmd)) == SrBtQuoteReject);
    CHECK(sr_bt_quote_add_cmd("x", 1u, cmd, 8u) == SrBtQuoteReject);

    CHECK(sr_bt_ignore_rm_cmd(3u, cmd, sizeof(cmd)) == true);
    CHECK(strcmp(cmd, "sigroam ignore rm 3\n") == 0);
    CHECK(sr_bt_ignore_rm_cmd(0u, cmd, sizeof(cmd)) == false);
    CHECK(sr_bt_split_cmd(0u, cmd, sizeof(cmd)) == true);
    CHECK(strcmp(cmd, "sigroam split 0\n") == 0);
    CHECK(sr_bt_split_cmd(512u, cmd, sizeof(cmd)) == true);
    CHECK(strcmp(cmd, "sigroam split 512\n") == 0);
    CHECK(sr_bt_rm_cmd("wardrive_3.log", cmd, sizeof(cmd)) == true);
    CHECK(strcmp(cmd, "sigroam rm wardrive_3.log\n") == 0);
    CHECK(sr_bt_rm_cmd("a b.log", cmd, sizeof(cmd)) == false);
    CHECK(sr_bt_rm_cmd("a/b.log", cmd, sizeof(cmd)) == false);
    CHECK(strcmp(sr_bt_ignore_cmd(), "sigroam ignore list\n") == 0);
    CHECK(strcmp(sr_bt_barrier_cmd(), "channel\n") == 0);
    CHECK(strcmp(sr_bt_split_query_cmd(), "sigroam split\n") == 0);
    CHECK(strcmp(sr_bt_clear_cmd(), "sigroam ignore clear\n") == 0);
    CHECK(strcmp(sr_bt_rm_all_cmd(), "sigroam rm all\n") == 0);
}

static size_t nlen(const char* s) {
    return strlen(s);
}

static void test_lines(void) {
    char name[40];
    unsigned ord = 0u;
    uint32_t n = 99u;
    uint32_t kb = 99u;
    const char* bad = "Busy: st=1 seal=00";
    const char* added = "Ignore added: my net";
    const char* fail = "Ignore add failed (dup/full/invalid; max 10, 1-32 chars)";
    const char* inv = "Invalid size (0=off, 4-102400 KB)";

    CHECK(sr_bt_line_busy("Busy: stopscan first", nlen("Busy: stopscan first")) == true);
    CHECK(sr_bt_line_busy(bad, nlen(bad)) == false);
    CHECK(sr_bt_line_ignore_empty("Ignore list empty", nlen("Ignore list empty")) == true);
    CHECK(sr_bt_line_ignore_empty("Ignore list empty ", nlen("Ignore list empty ")) == false);
    CHECK(
        sr_bt_line_ignore_item("1: cafe wifi", nlen("1: cafe wifi"), &ord, name, sizeof(name)) ==
        true);
    CHECK(ord == 1u);
    CHECK(strcmp(name, "cafe wifi") == 0);
    CHECK(sr_bt_line_ignore_item("2: home", nlen("2: home"), &ord, name, sizeof(name)) == true);
    CHECK(ord == 2u);
    CHECK(strcmp(name, "home") == 0);
    CHECK(sr_bt_line_ignore_item("1:cafe", nlen("1:cafe"), &ord, name, sizeof(name)) == false);
    CHECK(sr_bt_line_ignore_item("0: nope", nlen("0: nope"), &ord, name, sizeof(name)) == false);
    CHECK(sr_bt_line_ignore_added(added, nlen(added)) == true);
    CHECK(sr_bt_line_ignore_add_fail(fail, nlen(fail)) == true);
    CHECK(sr_bt_line_ignore_removed("Ignore 2 removed", nlen("Ignore 2 removed"), &ord) == true);
    CHECK(ord == 2u);
    CHECK(sr_bt_line_ignore_bad_index("Invalid index", nlen("Invalid index")) == true);
    CHECK(sr_bt_line_ignore_cleared("Ignore list cleared", nlen("Ignore list cleared")) == true);
    CHECK(
        sr_bt_line_removed_file(
            "Removed file: wardrive_3.log",
            nlen("Removed file: wardrive_3.log"),
            name,
            sizeof(name)) == true);
    CHECK(strcmp(name, "wardrive_3.log") == 0);
    CHECK(sr_bt_line_removed_n("Removed 3 files", nlen("Removed 3 files"), &n) == true);
    CHECK(n == 3u);
    CHECK(sr_bt_line_removed_n("Removed 0 files", nlen("Removed 0 files"), &n) == true);
    CHECK(n == 0u);
    CHECK(sr_bt_line_removed_n("Removed file: x", nlen("Removed file: x"), &n) == false);
    CHECK(
        sr_bt_line_remove_fail(
            "Remove failed (usage: sigroam rm)", nlen("Remove failed (usage: sigroam rm)")) ==
        true);
    CHECK(
        sr_bt_line_remove_fail(
            "Could not remove file: wardrive_1.log",
            nlen("Could not remove file: wardrive_1.log")) == true);
    CHECK(sr_bt_line_sd_missing("SD not available", nlen("SD not available")) == true);
    CHECK(
        sr_bt_line_split(
            "Split: 1024 KB (next wardrive)", nlen("Split: 1024 KB (next wardrive)"), &kb) ==
        true);
    CHECK(kb == 1024u);
    CHECK(
        sr_bt_line_split(
            "Split: 0 KB (next wardrive)", nlen("Split: 0 KB (next wardrive)"), &kb) == true);
    CHECK(kb == 0u);
    CHECK(sr_bt_line_split("Split: 512 KB", nlen("Split: 512 KB"), &kb) == true);
    CHECK(kb == 512u);
    CHECK(sr_bt_line_split_invalid(inv, nlen(inv)) == true);
    CHECK(sr_bt_line_split(inv, nlen(inv), &kb) == false);
}

static void test_list_feed(void) {
    char ssid[40];
    unsigned ord = 0u;

    sr_bt_reset();
    sr_bt_arm_list();
    feed("1: early");
    CHECK(sr_bt_ignore_count() == 0u);
    feed("> #sigroam ignore list");
    CHECK(sr_bt_ignore_barrier_due() == true);
    feed("Ignore list empty");
    sr_bt_ignore_barrier_sent();
    feed("#channel");
    CHECK(sr_bt_ignore_closed() == true);
    CHECK(sr_bt_ignore_busy() == false);
    CHECK(sr_bt_ignore_count() == 0u);

    sr_bt_arm_list();
    feed("#sigroam ignore list");
    feed("1: cafe wifi");
    feed("2: home");
    CHECK(sr_bt_ignore_closed() == false);
    sr_bt_ignore_barrier_sent();
    feed("> #channel");
    CHECK(sr_bt_ignore_closed() == true);
    CHECK(sr_bt_ignore_count() == 2u);
    CHECK(sr_bt_ignore_item(0u, &ord, ssid, sizeof(ssid)) == true);
    CHECK(ord == 1u);
    CHECK(strcmp(ssid, "cafe wifi") == 0);
    CHECK(sr_bt_ignore_item(1u, &ord, ssid, sizeof(ssid)) == true);
    CHECK(ord == 2u);
    CHECK(strcmp(ssid, "home") == 0);

    sr_bt_arm_list();
    feed("#sigroam ignore list");
    feed("Busy: stopscan first");
    CHECK(sr_bt_ignore_busy() == true);
    CHECK(sr_bt_ignore_closed() == true);

    sr_bt_arm_reply();
    feed("Ignore added: cafe wifi");
    CHECK(sr_bt_reply_kind() == (unsigned)SrBtReplyAdded);
    feed("Ignore list cleared");
    CHECK(sr_bt_reply_kind() == (unsigned)SrBtReplyCleared);
    feed("Split: 512 KB (next wardrive)");
    {
        uint32_t kb = 0u;
        CHECK(sr_bt_split_value(&kb) == true);
        CHECK(kb == 512u);
    }
    feed("Removed 4 files");
    CHECK(sr_bt_reply_kind() == (unsigned)SrBtReplyFiles);
    CHECK(sr_bt_reply_n() == 4u);
}

static void test_delete_machine(void) {
    SrBtDel d;
    uint32_t t;

    memset(&d, 0, sizeof(d));
    sr_bt_del_open_all(&d, 1000u);
    CHECK(d.phase == (uint8_t)SrBtDelAll1);
    CHECK(sr_bt_del_on_key(&d, SrBtKeyCenter, 1000u, false) == SrBtDelActNext);
    CHECK(d.phase == (uint8_t)SrBtDelAll2);
    CHECK(sr_bt_del_on_key(&d, SrBtKeyCenter, 1001u, false) == SrBtDelActNone);
    CHECK(d.phase == (uint8_t)SrBtDelAll2);
    CHECK(sr_bt_del_on_key(&d, SrBtKeyRight, 1500u, false) == SrBtDelActNone);
    CHECK(d.phase == (uint8_t)SrBtDelAll2);
    t = d.opened_ms;
    CHECK(sr_bt_del_on_key(&d, SrBtKeyRight, t + 1000u, false) == SrBtDelActSendAll);
    CHECK(d.phase == (uint8_t)SrBtDelIdle);

    sr_bt_del_open_all(&d, 10u);
    CHECK(sr_bt_del_on_key(&d, SrBtKeyCenter, 10u, false) == SrBtDelActNext);
    CHECK(sr_bt_del_on_key(&d, SrBtKeyBack, 20u, false) == SrBtDelActCancel);
    CHECK(d.phase == (uint8_t)SrBtDelIdle);

    sr_bt_del_open_all(&d, 10u);
    CHECK(sr_bt_del_on_key(&d, SrBtKeyLeft, 10u, false) == SrBtDelActCancel);

    sr_bt_del_open_all(&d, 0u);
    CHECK(sr_bt_del_on_key(&d, SrBtKeyCenter, 0u, true) == SrBtDelActNone);
    CHECK(d.phase == (uint8_t)SrBtDelAll1);

    sr_bt_del_open_one(&d);
    CHECK(d.phase == (uint8_t)SrBtDelOne);
    CHECK(sr_bt_del_on_key(&d, SrBtKeyCenter, 0u, true) == SrBtDelActNone);
    CHECK(d.phase == (uint8_t)SrBtDelOne);
    CHECK(sr_bt_del_on_key(&d, SrBtKeyCenter, 5u, false) == SrBtDelActSendOne);
    CHECK(d.phase == (uint8_t)SrBtDelIdle);

    sr_bt_del_open_one(&d);
    CHECK(sr_bt_del_on_key(&d, SrBtKeyBack, 1u, false) == SrBtDelActCancel);

    /* Right stays ignored across the tick wrap until a full second elapses. */
    sr_bt_del_open_all(&d, 0u);
    CHECK(sr_bt_del_on_key(&d, SrBtKeyCenter, 0xFFFFFF00u, false) == SrBtDelActNext);
    CHECK(d.opened_ms == 0xFFFFFF00u);
    CHECK(sr_bt_del_on_key(&d, SrBtKeyRight, 0x10u, false) == SrBtDelActNone);
    CHECK(d.phase == (uint8_t)SrBtDelAll2);
    CHECK(sr_bt_del_on_key(&d, SrBtKeyRight, 0xFFFFFF00u + 1000u, false) == SrBtDelActSendAll);
}

static void test_split_home_row(void) {
    char row[32];
    char ssid[24];

    CHECK(sr_bt_offer_home(1u, "House", false) == true);
    CHECK(sr_bt_offer_home(1u, "House", true) == false);
    CHECK(sr_bt_offer_home(0u, "House", false) == false);
    CHECK(sr_bt_offer_home(1u, "-", false) == false);
    CHECK(sr_bt_offer_home(1u, "", false) == false);
    CHECK(sr_bt_offer_home(1u, NULL, false) == false);
    CHECK(sr_bt_list_full(10u) == true);
    CHECK(sr_bt_list_full(9u) == false);

    CHECK(sr_bt_del_all_detail(3u, 1u, row, sizeof(row)) == true);
    CHECK(strcmp(row, "3 files, 1 not uploaded") == 0);
    CHECK(sr_bt_del_all_detail(3u, UINT32_MAX, row, sizeof(row)) == false);

    CHECK(sr_bt_split_is_preset(0u) == true);
    CHECK(sr_bt_split_is_preset(1024u) == true);
    CHECK(sr_bt_split_is_preset(1000u) == false);
    CHECK(strcmp(sr_bt_split_label(3u), "1024 KB (default)") == 0);
    CHECK(sr_bt_split_kb(0u) == 0u);
    CHECK(sr_bt_split_kb(2u) == 512u);
    CHECK(sr_bt_split_label(6u) == NULL);

    CHECK(sr_bt_split_cursor(true, 512u, 0) == 2);
    CHECK(sr_bt_split_cursor(true, 0u, 3) == 0);
    CHECK(sr_bt_split_cursor(true, 4096u, 0) == 5);
    CHECK(sr_bt_split_cursor(true, 1000u, 2) == SR_BT_SPLIT_CUR_NOW);
    CHECK(sr_bt_split_cursor(false, 0u, 4) == 4);
    CHECK(sr_bt_split_cursor(false, 0u, -1) == 0);
    CHECK(sr_bt_split_cursor(false, 0u, 6) == 0);

    CHECK(sr_bt_log_row("wardrive_3.log", 122880u, false, row, sizeof(row)) == true);
    CHECK(strcmp(row, "wardrive_3.log 120K") == 0);
    CHECK(strlen(row) <= 20u);
    CHECK(sr_bt_log_row("wardrive_3.log", 122880u, true, row, sizeof(row)) == true);
    CHECK(strcmp(row, "wardrive_3.log 120K*") == 0);
    CHECK(strlen(row) <= 20u);
    CHECK(sr_bt_log_more(2u, row, sizeof(row)) == true);
    CHECK(strcmp(row, "+2 more (use PC)") == 0);
    CHECK(sr_bt_log_more(0u, row, sizeof(row)) == false);

    sr_bt_show_ssid("cafe wifi", ssid, sizeof(ssid));
    CHECK(strcmp(ssid, "cafe wifi") == 0);
    sr_bt_show_ssid("abcdefghijklmnopqrstuvwxyz", ssid, 21u);
    CHECK(strlen(ssid) <= 20u);
    CHECK(ssid[19] == '~');
}

static void test_log_slots(void) {
    char name[40];
    uint32_t size = 0u;
    bool sent = false;
    unsigned i;
    char line[40];

    sr_mau_logs_reset();
    sr_mau_logs_arm();
    sr_mau_logs_feed("> #ls /", 7u);
    sr_mau_logs_feed("wardrive_3.log122880", 20u);
    sr_mau_logs_feed("wardrive_3.log.wigle8", 21u);
    sr_mau_logs_barrier_sent();
    sr_mau_logs_feed("> #channel", 10u);
    CHECK(sr_mau_logs_closed() == true);
    CHECK(sr_mau_logs_slot(0u, name, sizeof(name), &size, &sent) == true);
    CHECK(strcmp(name, "wardrive_3.log") == 0);
    CHECK(size == 122880u);
    CHECK(sent == true);
    CHECK(sr_mau_logs_extra() == 0u);
    CHECK(sr_mau_logs_slot(1u, name, sizeof(name), &size, &sent) == false);

    sr_mau_logs_reset();
    sr_mau_logs_arm();
    sr_mau_logs_feed("#ls /", 5u);
    for(i = 1u; i <= 17u; i++) {
        snprintf(line, sizeof(line), "wardrive_%u.log1", i);
        sr_mau_logs_feed(line, strlen(line));
    }
    sr_mau_logs_barrier_sent();
    sr_mau_logs_feed("#channel", 8u);
    CHECK(sr_mau_logs_closed() == true);
    CHECK(sr_mau_logs_extra() == 1u);
    CHECK(sr_mau_logs_slot(15u, name, sizeof(name), &size, &sent) == true);
    CHECK(sent == false);
    CHECK(sr_mau_logs_slot(16u, name, sizeof(name), &size, &sent) == false);
    sr_mau_logs_reset();
}

int test_board_tools_run(void) {
    sr_test_failures = 0;
    test_gate_and_peer();
    test_quote_and_cmds();
    test_lines();
    test_list_feed();
    test_delete_machine();
    test_split_home_row();
    test_log_slots();
    return sr_test_failures;
}
