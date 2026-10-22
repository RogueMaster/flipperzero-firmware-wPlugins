#include "sr_test.h"

#include "sr_dialect.h"
#include "sr_line.h"
#include "sr_mau_logs.h"
#include "sr_marauder_upload.h"

#include <stdio.h>
#include <string.h>

static void feed(const char* s) {
    sr_mau_logs_feed(s, strlen(s));
}

static void lines(char* a, char* b) {
    memset(a, 0x5a, (size_t)SR_MAU_COLS + 1u);
    memset(b, 0x5a, (size_t)SR_MAU_COLS + 1u);
    sr_mau_logs_lines(a, b);
    CHECK(strlen(a) <= (size_t)SR_MAU_COLS);
    CHECK(strlen(b) <= (size_t)SR_MAU_COLS);
    CHECK(strcmp(a, "Sent to WiGLE") != 0);
    CHECK(strcmp(a, "No logs in list") != 0);
    CHECK(strcmp(b, "nothing sent") != 0);
    CHECK(strcmp(a, "nothing sent") != 0);
}

static void feed_wire(const char* bytes, size_t n) {
    SrLine line;
    size_t off = 0u;

    sr_line_init(&line);
    while(off < n) {
        size_t step = sr_line_feed(&line, bytes + off, n - off);
        if(step == 0u) {
            break;
        }
        off += step;
        if(!sr_line_ready(&line)) {
            continue;
        }
        {
            size_t len = 0u;
            const char* text = sr_line_text(&line, &len);
            sr_mau_logs_feed(text, len);
        }
        sr_line_consume(&line);
    }
}

/* Official wire: name, TAB, byte size, then the channel barrier closes it. */
static void close_listing(const char* rows) {
    char wire[2048];
    size_t n = 0u;
    size_t i;

    for(i = 0u; rows[i] != '\0' && n + 2u < sizeof(wire); i++) {
        wire[n++] = rows[i];
    }
    wire[n] = '\0';
    sr_mau_logs_arm();
    feed("> #ls /");
    feed_wire(wire, n);
    sr_mau_logs_barrier_sent();
    feed("> #channel");
    CHECK(sr_mau_logs_closed() == true);
}

static void set_ver(SrFirmwareInfo* fw, const char* ver) {
    size_t n = 0u;

    memset(fw, 0, sizeof(*fw));
    while(ver[n] != '\0' && n < (size_t)SR_FW_VERSION_MAX) {
        fw->version[n] = ver[n];
        n++;
    }
    fw->version[n] = '\0';
}

/* Upload tick must call this. The old !upload_lite gate dropped Scout Lite,
 * which is generic, so Help never sent channel and the list never closed. */
static int upload_tick_sends_lite_barrier(void) {
    static const char* paths[] = {
        "../../scenes/scene_upload.c",
        "scenes/scene_upload.c",
        NULL,
    };
    FILE* f = NULL;
    char buf[512];
    int has_call = 0;
    int has_old = 0;
    int i;

    for(i = 0; paths[i] != NULL; i++) {
        f = fopen(paths[i], "r");
        if(f != NULL) {
            break;
        }
    }
    if(f == NULL) {
        return 0;
    }
    while(fgets(buf, (int)sizeof(buf), f) != NULL) {
        if(strstr(buf, "sr_mau_logs_barrier_send(generic, lite_sent)") != NULL) {
            has_call = 1;
        }
        if(strstr(buf, "!upload_lite(app) && sr_mau_logs_barrier_due()") != NULL) {
            has_old = 1;
        }
    }
    fclose(f);
    return has_call == 1 && has_old == 0;
}

static void test_lite_barrier_slot(void) {
    SrFirmwareInfo fw;

    set_ver(&fw, "v1.18.0");
    fw.sigroam_cap = (uint8_t)SR_SIGROAM_CAP_YES;
    CHECK(sr_dialect_lite_status(&fw) == true);
    CHECK(sr_dialect_is_generic_marauder(&fw) == true);

    set_ver(&fw, "v1.14.1-sigroam-7");
    fw.sigroam_cap = (uint8_t)SR_SIGROAM_CAP_YES;
    CHECK(sr_dialect_lite_status(&fw) == false);
    CHECK(sr_dialect_is_generic_marauder(&fw) == false);

    sr_mau_logs_reset();
    CHECK(sr_mau_logs_barrier_send(true, false) == false);
    sr_mau_logs_arm();
    feed("> #ls /");
    CHECK(sr_mau_logs_barrier_due() == true);
    CHECK(sr_mau_logs_barrier_send(true, false) == true);
    CHECK(sr_mau_logs_barrier_send(true, true) == false);
    CHECK(sr_mau_logs_barrier_due() == true);
    CHECK(sr_mau_logs_barrier_send(true, false) == true);
    CHECK(sr_mau_logs_barrier_send(false, false) == false);
    sr_mau_logs_barrier_sent();
    CHECK(sr_mau_logs_barrier_due() == false);
    CHECK(sr_mau_logs_barrier_send(true, false) == false);
    CHECK(upload_tick_sends_lite_barrier() == 1);
}

int test_mau_logs_run(void) {
    char a[SR_MAU_COLS + 1u];
    char b[SR_MAU_COLS + 1u];
    char long_line[SR_RAW_LINE_MAX + 8u];
    SrLine pending;
    size_t i;

    sr_test_failures = 0;
    sr_mau_logs_reset();

    CHECK(strcmp(sr_mau_logs_cmd(), "ls /\n") == 0);
    CHECK(strstr(sr_mau_logs_cmd(), "upload") == NULL);
    CHECK(sr_upload_enter_cmd(true) == NULL);
    CHECK(strcmp(sr_upload_go_cmd(true), "upload -d wigle\n") == 0);

    CHECK(sr_mau_logs_allow(0u, false, false) == true);
    CHECK(sr_mau_logs_allow(2u, false, false) == true);
    CHECK(sr_mau_logs_allow(1u, false, false) == false);
    CHECK(sr_mau_logs_allow(0u, true, false) == false);
    CHECK(sr_mau_logs_allow(2u, false, true) == false);
    CHECK(sr_mau_logs_allow(3u, false, false) == false);

    lines(a, b);
    CHECK(strcmp(a, "press Logs") == 0);
    CHECK(strcmp(b, "not a listing") == 0);
    CHECK(sr_mau_logs_matches() == 0u);
    CHECK(sr_mau_logs_closed() == false);

    sr_mau_logs_arm();
    feed("wardrive_1.log1234");
    CHECK(sr_mau_logs_matches() == 0u);
    feed("#ls /");
    feed("wardrive_1.log1234");
    CHECK(sr_mau_logs_matches() == 1u);
    CHECK(sr_mau_logs_closed() == false);
    lines(a, b);
    CHECK(strcmp(a, "Saw 1 log name") == 0);
    CHECK(strcmp(b, "list not closed") == 0);

    feed("wardrive_1 my.log8");
    CHECK(sr_mau_logs_matches() == 2u);
    lines(a, b);
    CHECK(strcmp(a, "Saw 2 log names") == 0);

    feed("wardrive_1.log.wigle10");
    feed("wardrive_1.gpx3");
    feed("wigle-1.wdg3");
    feed("readme.txt3");
    feed("SCRIPTS0");
    CHECK(sr_mau_logs_matches() == 2u);

    feed("wigle-1.csv4");
    CHECK(sr_mau_logs_matches() == 3u);

    feed("Listing directory: /");
    CHECK(sr_mau_logs_closed() == false);
    CHECK(sr_mau_logs_matches() == 3u);

    feed("> ");
    CHECK(sr_mau_logs_closed() == true);
    CHECK(sr_mau_logs_armed() == false);
    lines(a, b);
    CHECK(strcmp(b, "list finished") == 0);
    CHECK(strcmp(a, "Saw 3 log names") == 0);
    feed("wardrive_9.log1");
    CHECK(sr_mau_logs_matches() == 3u);

    sr_mau_logs_arm();
    feed("#ls /");
    feed(">");
    CHECK(sr_mau_logs_matches() == 0u);
    CHECK(sr_mau_logs_closed() == true);
    lines(a, b);
    CHECK(strcmp(a, "No name matched") == 0);
    CHECK(strcmp(b, "list finished") == 0);

    sr_mau_logs_arm();
    feed("#ls /");
    lines(a, b);
    CHECK(strcmp(a, "Listing") == 0);
    CHECK(strcmp(b, "not finished") == 0);
    CHECK(strcmp(a, "No name matched") != 0);

    sr_mau_logs_arm();
    feed("#ls /");
    feed("Failed to mount SD Card");
    CHECK(sr_mau_logs_card() == true);
    CHECK(sr_mau_logs_closed() == false);
    CHECK(sr_mau_logs_matches() == 0u);
    lines(a, b);
    CHECK(strcmp(a, "Card not ready") == 0);
    CHECK(strcmp(b, "list unknown") == 0);

    sr_mau_logs_arm();
    feed("#ls /");
    feed("SD Card NOT Supported");
    CHECK(sr_mau_logs_card() == true);

    memset(long_line, 'A', sizeof(long_line));
    memcpy(long_line, "wardrive_", 9u);
    sr_mau_logs_arm();
    feed("#ls /");
    sr_mau_logs_feed(long_line, (size_t)SR_RAW_LINE_MAX);
    CHECK(sr_mau_logs_cut() == true);
    CHECK(sr_mau_logs_matches() == 0u);
    lines(a, b);
    CHECK(strcmp(a, "Line cut off") == 0);
    CHECK(strcmp(b, "list not closed") == 0);
    CHECK(strcmp(a, "No name matched") != 0);

    sr_mau_logs_arm();
    feed("#ls /");
    feed("wardrive_2.log9");
    sr_mau_logs_feed(long_line, (size_t)SR_RAW_LINE_MAX);
    lines(a, b);
    CHECK(strcmp(a, "Saw 1, cut") == 0);
    CHECK(strcmp(b, "list not closed") == 0);

    sr_mau_logs_arm();
    feed("#ls /");
    feed("wardrive_12");
    CHECK(sr_mau_logs_matches() == 0u);

    sr_mau_logs_arm();
    feed("#ls /");
    feed("wardrive_2.log9");
    sr_mau_logs_invalidate();
    CHECK(sr_mau_logs_dropped() == true);
    CHECK(sr_mau_logs_armed() == false);
    CHECK(sr_mau_logs_matches() == 0u);
    lines(a, b);
    CHECK(strcmp(a, "Scan started") == 0);
    CHECK(strcmp(b, "list dropped") == 0);
    feed("#ls /");
    feed("wardrive_2.log9");
    CHECK(sr_mau_logs_matches() == 0u);

    sr_mau_logs_reset();
    sr_mau_logs_invalidate();
    CHECK(sr_mau_logs_dropped() == false);
    lines(a, b);
    CHECK(strcmp(a, "press Logs") == 0);

    sr_mau_logs_block_lines(SrMauLogsBlockScan, a, b);
    CHECK(strcmp(a, "Stop scan first") == 0);
    CHECK(strcmp(b, "Logs not sent") == 0);
    sr_mau_logs_block_lines(SrMauLogsBlockUpload, a, b);
    CHECK(strcmp(a, "Upload is active") == 0);
    sr_mau_logs_block_lines(SrMauLogsBlockJoin, a, b);
    CHECK(strcmp(a, "Join is active") == 0);
    sr_mau_logs_block_lines(SrMauLogsBlockBusy, a, b);
    CHECK(strcmp(a, "Command busy") == 0);
    CHECK(strcmp(b, "Logs not sent") == 0);

    sr_mau_logs_arm();
    {
        const char wire[] = "#ls /\nwardrive_1.log\t1234\nwardrive_1.log.wigle\t10\n> ";
        feed_wire(wire, sizeof(wire) - 1u);
    }
    CHECK(sr_mau_logs_matches() == 1u);
    CHECK(sr_mau_logs_closed() == false);
    lines(a, b);
    CHECK(strcmp(b, "list not closed") == 0);

    sr_line_init(&pending);
    {
        const char prompt[] = "> ";
        size_t got = sr_line_feed(&pending, prompt, sizeof(prompt) - 1u);
        CHECK(got == sizeof(prompt) - 1u);
        CHECK(sr_line_ready(&pending) == false);
    }

    sr_mau_logs_arm();
    {
        const char wire[] = "#ls /\n> \n";
        feed_wire(wire, sizeof(wire) - 1u);
    }
    CHECK(sr_mau_logs_matches() == 0u);
    CHECK(sr_mau_logs_closed() == true);
    lines(a, b);
    CHECK(strcmp(a, "No name matched") == 0);

    sr_mau_logs_arm();
    feed_wire("#ls /\n", 6u);
    CHECK(sr_mau_logs_closed() == false);
    CHECK(sr_mau_logs_matches() == 0u);
    lines(a, b);
    CHECK(strcmp(a, "Listing") == 0);
    CHECK(strcmp(b, "not finished") == 0);

    for(i = 0u; i < (size_t)SR_RAW_LINE_MAX + 4u; i++) {
        long_line[i] = 'b';
    }
    memcpy(long_line, "wardrive_", 9u);
    long_line[SR_RAW_LINE_MAX + 4u] = '\n';
    sr_mau_logs_arm();
    feed("#ls /");
    feed_wire(long_line, (size_t)SR_RAW_LINE_MAX + 5u);
    CHECK(sr_mau_logs_cut() == true);
    CHECK(sr_mau_logs_matches() == 0u);

    /* v1.17.0 wire: the previous prompt glues onto the echo. The barrier
     * is due only after that echo, goes out once, and its echo closes. */
    CHECK(strcmp(sr_mau_logs_barrier_cmd(), "channel\n") == 0);
    sr_mau_logs_arm();
    CHECK(sr_mau_logs_barrier_due() == false);
    feed("#channel");
    CHECK(sr_mau_logs_closed() == false);
    {
        const char wire[] =
            "> #ls /\nwardrive_0.log\t1234\nwardrive_0.log.wigle\t5\nwardrive_1.log\t77\n";
        feed_wire(wire, sizeof(wire) - 1u);
    }
    CHECK(sr_mau_logs_matches() == 2u);
    CHECK(sr_mau_logs_barrier_due() == true);
    feed("> #channel");
    CHECK(sr_mau_logs_closed() == false);
    sr_mau_logs_barrier_sent();
    CHECK(sr_mau_logs_barrier_due() == false);
    {
        const char wire[] = "> #channel\n1\n> ";
        feed_wire(wire, sizeof(wire) - 1u);
    }
    CHECK(sr_mau_logs_closed() == true);
    CHECK(sr_mau_logs_armed() == false);
    CHECK(sr_mau_logs_barrier_due() == false);
    lines(a, b);
    CHECK(strcmp(a, "Saw 2 log names") == 0);
    CHECK(strcmp(b, "list finished") == 0);
    CHECK(sr_mau_logs_card_line(a) == true);
    CHECK(strcmp(a, "Card has 2 logs") == 0);

    sr_mau_logs_arm();
    CHECK(sr_mau_logs_card_line(a) == false);
    CHECK(a[0] == '\0');
    feed("> #ls /");
    feed("wardrive_7.log9");
    CHECK(sr_mau_logs_card_line(a) == false);
    sr_mau_logs_barrier_sent();
    feed("#channel");
    CHECK(sr_mau_logs_card_line(a) == true);
    CHECK(strcmp(a, "Card has 1 log") == 0);

    sr_mau_logs_arm();
    feed("> #ls /");
    sr_mau_logs_barrier_sent();
    feed("#channel");
    CHECK(sr_mau_logs_card_line(a) == false);
    CHECK(sr_mau_logs_closed() == true);
    lines(a, b);
    CHECK(strcmp(a, "No name matched") == 0);
    CHECK(strcmp(b, "list finished") == 0);

    sr_mau_logs_arm();
    feed("> #ls /");
    sr_mau_logs_invalidate();
    CHECK(sr_mau_logs_barrier_due() == false);
    sr_mau_logs_barrier_sent();
    feed("> #channel");
    CHECK(sr_mau_logs_dropped() == true);

    /* Nothing to say before a listing has closed. */
    sr_mau_logs_reset();
    CHECK(sr_mau_logs_sent_line(a) == false);
    CHECK(a[0] == '\0');
    CHECK(sr_mau_logs_unsent() == SR_MAU_LOGS_UNKNOWN);
    CHECK(sr_mau_logs_unsent_entry(0u, a) == false);
    CHECK(a[0] == '\0');

    /* Lab shape: one log, no sidecar. Size is bytes (SDInterface.cpp 380). */
    close_listing("wardrive_3.log\t18000\n");
    CHECK(sr_mau_logs_sent_line(a) == true);
    CHECK(strcmp(a, "1 log, 0 sent") == 0);
    CHECK(sr_mau_logs_unsent() == 1u);
    CHECK(sr_mau_logs_unsent_entry(0u, a) == true);
    CHECK(strcmp(a, "wardrive_3.log 17K") == 0);
    CHECK(sr_mau_logs_unsent_entry(1u, a) == false);
    CHECK(a[0] == '\0');

    /* Marauder writes <log>.wigle after a WiGLE 200 (WiFiScan.cpp 11827).
     * The sidecar may be listed before or after its log. */
    close_listing("wardrive_0.log.wigle\t5\nwardrive_0.log\t1234\nwardrive_1.log\t77\n"
                  "wardrive_2.log\t2097152\nwardrive_2.log.wigle\t4\n");
    CHECK(sr_mau_logs_matches() == 3u);
    CHECK(sr_mau_logs_sent_line(a) == true);
    CHECK(strcmp(a, "3 logs, 2 sent") == 0);
    CHECK(sr_mau_logs_unsent() == 1u);
    CHECK(sr_mau_logs_unsent_entry(0u, a) == true);
    CHECK(strcmp(a, "wardrive_1.log 77B") == 0);
    CHECK(sr_mau_logs_unsent_entry(1u, a) == false);

    /* An orphan sidecar is not a sent log. .wdg and .gpx are not WiGLE. */
    close_listing("wardrive_5.log.wigle\t5\nwardrive_6.log\t1048576\nwardrive_6.log.wdg\t3\n"
                  "wardrive_6.gpx\t9\nwigle-1.csv\t1023\n");
    CHECK(sr_mau_logs_sent_line(a) == true);
    CHECK(strcmp(a, "2 logs, 0 sent") == 0);
    CHECK(sr_mau_logs_unsent() == 2u);
    CHECK(sr_mau_logs_unsent_entry(0u, a) == true);
    CHECK(strcmp(a, "wardrive_6.log 1M") == 0);
    CHECK(sr_mau_logs_unsent_entry(1u, a) == true);
    CHECK(strcmp(a, "wigle-1.csv 1023B") == 0);

    /* All sent. */
    close_listing("wardrive_0.log\t9\nwardrive_0.log.wigle\t5\n");
    CHECK(sr_mau_logs_sent_line(a) == true);
    CHECK(strcmp(a, "1 log, 1 sent") == 0);
    CHECK(sr_mau_logs_unsent() == 0u);
    CHECK(sr_mau_logs_unsent_entry(0u, a) == false);

    /* No log names: no sent line, unsent is a proven zero. */
    close_listing("readme.txt\t3\n");
    CHECK(sr_mau_logs_sent_line(a) == false);
    CHECK(sr_mau_logs_unsent() == 0u);

    /* A long name keeps its size and ends in ~ to fit 20 columns. */
    close_listing("wardrive_123456789012.log\t2048\n");
    CHECK(sr_mau_logs_unsent_entry(0u, a) == true);
    CHECK(strcmp(a, "wardrive_1234567~ 2K") == 0);
    CHECK(strlen(a) == (size_t)SR_MAU_COLS);

    /* A name over the table width: count only, no sent claim. */
    close_listing("wardrive_0.log\t9\nwardrive_0123456789012345678901.log\t9\n");
    CHECK(sr_mau_logs_matches() == 2u);
    CHECK(sr_mau_logs_unsent() == SR_MAU_LOGS_UNKNOWN);
    CHECK(sr_mau_logs_sent_line(a) == true);
    CHECK(strcmp(a, "Card has 2 logs") == 0);
    CHECK(sr_mau_logs_unsent_entry(0u, a) == false);

    /* More distinct names than the table holds: same fallback. */
    {
        char wire[64u * (SR_MAU_LOG_SLOTS + 2u)];
        size_t used = 0u;
        unsigned k;

        for(k = 0u; k < (unsigned)SR_MAU_LOG_SLOTS + 1u; k++) {
            used += (size_t)snprintf(wire + used, sizeof(wire) - used, "wardrive_%u.log\t9\n", k);
        }
        close_listing(wire);
    }
    CHECK(sr_mau_logs_matches() == (uint32_t)SR_MAU_LOG_SLOTS + 1u);
    CHECK(sr_mau_logs_unsent() == SR_MAU_LOGS_UNKNOWN);
    CHECK(sr_mau_logs_sent_line(a) == true);
    CHECK(strcmp(a, "Card has 17 logs") == 0);

    /* A listing that did not close says nothing about sent. */
    sr_mau_logs_arm();
    feed("> #ls /");
    feed("wardrive_0.log9");
    CHECK(sr_mau_logs_sent_line(a) == false);
    CHECK(sr_mau_logs_unsent() == SR_MAU_LOGS_UNKNOWN);
    CHECK(sr_mau_logs_unsent_entry(0u, a) == false);

    /* A new arm forgets the old table. */
    close_listing("wardrive_0.log\t9\n");
    sr_mau_logs_arm();
    CHECK(sr_mau_logs_unsent() == SR_MAU_LOGS_UNKNOWN);
    CHECK(sr_mau_logs_unsent_entry(0u, a) == false);

    CHECK(strcmp(SR_MAU_LOGS_HELP_1, "CLI list empty") == 0);
    CHECK(strcmp(SR_MAU_LOGS_HELP_2, "SD logs may exist") == 0);
    CHECK(strcmp(SR_MAU_LOGS_HELP_3, "Read card on PC") == 0);
    CHECK(strcmp(SR_MAU_LOGS_HELP_4, "Upload via WiGLE") == 0);

    test_lite_barrier_slot();
    sr_mau_logs_reset();
    return sr_test_failures;
}
