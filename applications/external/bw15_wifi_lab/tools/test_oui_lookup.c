// Host check for the OUI parser. Not part of the FAP.
//   cc -Wall -Wextra -Werror -std=c11 -Ifap tools/test_oui_lookup.c fap/oui_lookup.c -o /tmp/test_oui
#include "oui_lookup.h"

#include <stdio.h>
#include <string.h>

static int g_fail;

static void expect(int cond, const char* msg) {
    if(cond) return;
    fprintf(stderr, "FAIL %s\n", msg);
    g_fail++;
}

static void expect_str(const char* mac, const char* want, const char* msg) {
    const char* got = g5_oui_lookup(mac);
    if(want == NULL) {
        expect(got == NULL, msg);
        return;
    }
    if(!got || strcmp(got, want) != 0) {
        fprintf(stderr, "FAIL %s (got %s)\n", msg, got ? got : "(null)");
        g_fail++;
    }
}

static void test_parser(void) {
    g5_oui_reset();
    expect_str("00:14:78:00:00:00", NULL, "empty table");
    expect(g5_oui_add_line("") == G5OuiSkip, "blank");
    expect(g5_oui_add_line("   ") == G5OuiSkip, "spaces");
    expect(g5_oui_add_line("# comment") == G5OuiSkip, "hash");
    expect(g5_oui_add_line("  # indented") == G5OuiSkip, "indented hash");
    expect(g5_oui_add_line("00:11") == G5OuiSkip, "short");
    expect(g5_oui_add_line("00:11:22") == G5OuiSkip, "oui only");
    expect(g5_oui_add_line("GG:11:22 Cisco") == G5OuiSkip, "bad hex");
    expect(g5_oui_add_line("00:11:22Cisco") == G5OuiSkip, "no separator");

    expect(g5_oui_add_line("00:14:78 TP-Link") == G5OuiAdded, "colon");
    expect(g5_oui_add_line("001bFC ASUS") == G5OuiAdded, "bare lower");
    expect(g5_oui_add_line("00-00-0C\tCisco") == G5OuiAdded, "dash tab");
    expect(g5_oui_add_line("14:CC:20:AA:BB:CC TP-Link") == G5OuiAdded, "full mac");
    expect(g5_oui_add_line("24:0A:C4 Espressif # note") == G5OuiAdded, "trailing note");
    expect(g5_oui_count() == 5, "five distinct");

    expect_str("00:14:78:11:22:33", "TP-Link", "bssid");
    expect_str("001478112233", "TP-Link", "station mac");
    expect_str("00:1B:FC:00:00:01", "ASUS", "asus case");
    expect_str("00:00:0c:de:ad:00", "Cisco", "cisco lower");
    expect_str("14cc20abcdef", "TP-Link", "full mac oui");
    expect_str("24:0A:C4:00:00:01", "Espressif", "note stripped");
    expect_str("02:00:00:11:22:33", NULL, "unknown local");
    expect_str("AA:BB", NULL, "too short");
    expect_str(NULL, NULL, "null mac");

    expect(g5_oui_add_line("00:14:78 Other") == G5OuiAdded, "replace");
    expect(g5_oui_count() == 5, "replace keeps count");
    expect_str("00:14:78:00:00:00", "Other", "last wins");

    expect(g5_oui_add_line("00:11:22 ABCDEFGHIJKLMNOP") == G5OuiAdded, "long name");
    expect_str("00:11:22:00:00:00", "ABCDEFGHIJKL", "cut to 12");

    expect(g5_oui_add_line("00:11:23 TP Link") == G5OuiAdded, "inner space");
    expect_str("001123000000", "TP Link", "kept space");
}

static void test_cap(void) {
    g5_oui_reset();
    char line[32];
    for(int i = 0; i < G5_OUI_MAX; i++) {
        snprintf(line, sizeof(line), "%06X V", i);
        G5OuiAddResult r = g5_oui_add_line(line);
        if(r != G5OuiAdded) {
            fprintf(stderr, "FAIL fill at %d\n", i);
            g_fail++;
            return;
        }
    }
    expect(g5_oui_count() == G5_OUI_MAX, "full count");
    expect(g5_oui_add_line("FFFFFF Extra") == G5OuiFull, "over cap");
    expect(g5_oui_count() == G5_OUI_MAX, "cap sticks");
    expect(g5_oui_add_line("000000 Renamed") == G5OuiAdded, "overwrite while full");
    expect(g5_oui_count() == G5_OUI_MAX, "overwrite no grow");
    expect_str("00:00:00:00:00:01", "Renamed", "overwrite visible");
    expect_str("FF:FF:FF:00:00:01", NULL, "rejected extra");
}

static void test_starter_file(void) {
    g5_oui_reset();
    FILE* f = fopen("fap/files/oui.txt", "r");
    expect(f != NULL, "open starter");
    if(!f) return;
    char line[128];
    int added = 0;
    while(fgets(line, sizeof(line), f)) {
        G5OuiAddResult r = g5_oui_add_line(line);
        if(r == G5OuiAdded) added++;
        expect(r != G5OuiFull, "starter fits");
    }
    fclose(f);
    expect(added == 275, "starter record count");
    expect(g5_oui_count() == 275, "starter unique");
    expect_str("00:14:78:AA:BB:CC", "TP-Link", "starter tplink");
    expect_str("00000C112233", "Cisco", "starter cisco");
    expect_str("00:1B:FC:00:11:22", "ASUS", "starter asus");
    expect_str("24:0A:C4:01:02:03", "Espressif", "starter esp");
    expect_str("00:0B:86:10:00:01", "HP", "starter hpe");
    expect_str("00:0C:42:00:00:01", "MikroTik", "starter mt");
    expect_str("DE:AD:BE:EF:00:01", NULL, "starter miss");
}

int main(void) {
    test_parser();
    test_cap();
    test_starter_file();
    if(g_fail) {
        fprintf(stderr, "%d failed\n", g_fail);
        return 1;
    }
    printf("oui lookup ok (%u starter)\n", (unsigned)g5_oui_count());
    return 0;
}
