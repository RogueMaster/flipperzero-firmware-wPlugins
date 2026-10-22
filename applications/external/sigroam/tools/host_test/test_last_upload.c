#include "sr_test.h"

#include "sr_last_upload.h"
#include "sr_marauder_upload.h"

#include <string.h>

static bool same(const char* got, const char* want) {
    if(got == NULL || want == NULL) {
        return false;
    }
    return strcmp(got, want) == 0;
}

static void feed(const char* s) {
    sr_mau_up_feed(s, strlen(s));
}

static void test_format(void) {
    SrLastUpload u;
    SrLastUpload back;
    char buf[80];
    char line[40];
    size_t n;

    memset(&u, 0, sizeof(u));
    u.month = 10u;
    u.day = 20u;
    u.hour = 14u;
    u.minute = 3u;
    u.ok_n = 3u;
    u.fail_n = 0u;
    u.wigle200 = true;
    memset(&back, 0, sizeof(back));
    n = sr_last_upload_format(&u, buf, sizeof(buf));
    CHECK(n > 0u);
    CHECK(sr_last_upload_parse(buf, n, &back) == SrLastOk);
    CHECK(back.month == 10u);
    CHECK(back.day == 20u);
    CHECK(back.hour == 14u);
    CHECK(back.minute == 3u);
    CHECK(back.ok_n == 3u);
    CHECK(back.fail_n == 0u);
    CHECK(back.wigle200 == true);
    CHECK(sr_last_upload_line(SrLastOk, &u, line, sizeof(line)) > 0u);
    CHECK(same(line, "Last: 10-20 14:03 OK 3"));

    u.fail_n = 1u;
    u.ok_n = 2u;
    CHECK(sr_last_upload_line(SrLastOk, &u, line, sizeof(line)) > 0u);
    CHECK(same(line, "Last: 10-20 14:03 1 fail"));
    CHECK(sr_last_upload_line(SrLastNone, &u, line, sizeof(line)) > 0u);
    CHECK(same(line, "Last: none"));
    CHECK(sr_last_upload_line(SrLastBad, &u, line, sizeof(line)) > 0u);
    CHECK(same(line, "Last: ?"));
    CHECK(sr_last_upload_parse("", 0u, &back) == SrLastBad);
    CHECK(sr_last_upload_parse("v1", 2u, &back) == SrLastBad);
    CHECK(sr_last_upload_parse("nope\n", 5u, &back) == SrLastBad);
    CHECK(
        sr_last_upload_parse("v1 13 20 14 3 0 0 1\n", strlen("v1 13 20 14 3 0 0 1\n"), &back) ==
        SrLastBad);
    CHECK(sr_last_upload_parse(NULL, 0u, &back) == SrLastBad);

    CHECK(sr_last_upload_pending(true, 4u, line, sizeof(line)) > 0u);
    CHECK(same(line, "Not uploaded: 4 logs"));
    CHECK(sr_last_upload_pending(false, 0u, line, sizeof(line)) > 0u);
    CHECK(same(line, "Not uploaded: ?"));
}

static void test_tally(void) {
    SrMauUpTally t;
    const char* ok = "[UPLOAD] WiGLE upload succeeded";
    const char* bad = "[UPLOAD] WiGLE upload failed";

    sr_mau_up_reset();
    CHECK(sr_mau_up_tally(&t) == false);

    sr_mau_up_on_send();
    feed("Uploading a.log...");
    feed(ok);
    feed("Uploading b.log...");
    feed(ok);
    feed("Uploading c.log...");
    feed(ok);
    CHECK(sr_mau_up_tally(&t) == false);
    feed("Upload complete");
    CHECK(sr_mau_up_tally(&t) == true);
    CHECK(t.ready == true);
    CHECK(t.ok_n == 3u);
    CHECK(t.fail_n == 0u);
    CHECK(t.wigle200 == true);

    sr_mau_up_on_send();
    feed("Uploading a.log...");
    feed(ok);
    feed("Uploading b.log...");
    feed(bad);
    feed("Upload complete");
    CHECK(sr_mau_up_tally(&t) == true);
    CHECK(t.ok_n == 1u);
    CHECK(t.fail_n == 1u);
    CHECK(t.wigle200 == true);

    sr_mau_up_on_send();
    feed("Upload complete");
    CHECK(sr_mau_up_tally(&t) == true);
    CHECK(t.ok_n == 0u);
    CHECK(t.fail_n == 0u);
    CHECK(t.wigle200 == false);
}

int test_last_upload_run(void) {
    sr_test_failures = 0;
    test_format();
    test_tally();
    return sr_test_failures;
}
