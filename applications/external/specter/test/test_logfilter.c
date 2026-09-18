/* Host tests for logbook filtering. */
#include "../helpers/log_filter.h"

#include <stdio.h>
#include <string.h>

static int failures = 0, checks = 0;
static void check(int cond, const char* what) {
    checks++;
    if(!cond) {
        failures++;
        printf("  FAIL: %s\n", what);
    }
}

static const char* LOG = "2026-08-01 10:00:00\n  SWEEP  field 40% peak 61% hits 2\n"
                         "2026-08-01 10:01:00\n  READER POLLING period 204ms\n"
                         "2026-08-01 10:02:00\n  SURVEY 60s ACTIVE max 100%\n"
                         "2026-08-01 10:03:00\n  WATCH  contact 1 at 12s\n"
                         "2026-08-01 10:04:00\n  READER CONTINUOUS duty 99%\n";

int main(void) {
    printf("log_filter\n");
    char out[1024];

    check(specter_log_filter(LOG, "READER", out, sizeof(out)) == 2, "two READER entries");
    check(strstr(out, "POLLING") && strstr(out, "CONTINUOUS"), "both kept");
    check(!strstr(out, "SWEEP") && !strstr(out, "SURVEY"), "others dropped");
    check(strstr(out, "2026-08-01 10:01:00") != NULL, "timestamp kept with its detail");
    check(strstr(out, "2026-08-01 10:00:00") == NULL, "dropped entry loses its timestamp too");

    check(specter_log_filter(LOG, "WATCH", out, sizeof(out)) == 1, "one WATCH entry");
    check(specter_log_filter(LOG, "SURVEY", out, sizeof(out)) == 1, "one SURVEY entry");
    check(specter_log_filter(LOG, NULL, out, sizeof(out)) == 5, "NULL type keeps everything");
    check(specter_log_filter(LOG, "", out, sizeof(out)) == 5, "empty type keeps everything");
    check(specter_log_filter(LOG, "NOPE", out, sizeof(out)) == 0, "no matches is empty");
    check(out[0] == '\0', "and the buffer says so");

    /* robustness */
    check(specter_log_filter(NULL, "READER", out, sizeof(out)) == 0, "NULL text is safe");
    check(specter_log_filter(LOG, "READER", out, 0) == 0, "zero-length buffer is safe");
    {
        char tiny[24];
        specter_log_filter(LOG, NULL, tiny, sizeof(tiny));
        check(strlen(tiny) < sizeof(tiny), "a small buffer is never overrun");
    }
    {
        /* a trailing entry with no detail line must not loop forever */
        const char* ragged = "2026-08-01 10:00:00\n  SWEEP x\n2026-08-01 10:05:00";
        check(specter_log_filter(ragged, NULL, out, sizeof(out)) == 2, "ragged tail terminates");
    }
    /* --- REGRESSION: the viewer hands us a tail that starts mid-entry -------
     * read_tail shows only the last few KB. Whenever that cut landed inside a
     * timestamp line the text began on an indented DETAIL line, every line was
     * read off by one, and a logbook full of READER entries filtered to zero. */
    {
        const char* midentry = "  SWEEP  field 40% peak 61% hits 2\n" /* orphaned fragment */
                               "2026-08-01 10:01:00\n  READER POLLING period 204ms\n"
                               "2026-08-01 10:02:00\n  SURVEY 60s ACTIVE max 100%\n"
                               "2026-08-01 10:03:00\n  READER CONTINUOUS duty 99%\n";
        check(
            specter_log_filter(midentry, "READER", out, sizeof(out)) == 2,
            "a tail starting mid-entry still finds both READERs");
        check(strstr(out, "POLLING") && strstr(out, "CONTINUOUS"), "both kept");
        check(strstr(out, "2026-08-01 10:01:00") != NULL, "with their timestamps");
        check(
            specter_log_filter(midentry, "SWEEP", out, sizeof(out)) == 0,
            "the orphaned fragment is not counted as an entry");
        check(
            specter_log_filter(midentry, NULL, out, sizeof(out)) == 3,
            "and unfiltered shows the 3 whole entries, not the fragment");
        check(strstr(out, "field 40%") == NULL, "the timestamp-less fragment is dropped");
    }

    /* --- an entry with several detail lines stays whole --------------------- */
    {
        const char* multi = "2026-08-01 11:00:00\n  SURVEY 60s\n  ACTIVE READER\n  mx74 av21\n"
                            "2026-08-01 11:05:00\n  SWEEP  field 12%\n";
        check(specter_log_filter(multi, "SURVEY", out, sizeof(out)) == 1, "one SURVEY entry");
        check(
            strstr(out, "ACTIVE READER") && strstr(out, "mx74 av21"),
            "all of its detail lines come with it");
        check(strstr(out, "SWEEP") == NULL, "and the next entry does not");
    }

    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
