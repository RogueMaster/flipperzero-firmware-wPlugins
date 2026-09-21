/* Host tests for the logbook line-wrapper.
 *
 * The viewer's TextBox wraps by character, not by word, so a long detail came
 * out split mid-word and un-indented:
 *
 *     2026-09-21 12:19:28
 *       WATCH  contact 2 at 8s fiel
 *     d 17% peak 100% m:boost
 *
 * Two things matter here and they pull in different directions: the output has
 * to LOOK right (no line over the width, no word torn in half), and it has to
 * still BE an entry - specter_log_filter() decides what belongs to a finding by
 * indentation, and looks for the type at the front of the first indented line.
 * Wrap it wrong and filtering by WATCH silently loses half of every entry, so
 * the last block here runs the wrapped output straight through the filter.
 *
 *   make -C test
 */

#include "../helpers/log_wrap.h"
#include "../helpers/log_filter.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

static void check(int cond, const char* what) {
    checks++;
    if(!cond) {
        failures++;
        printf("  FAIL: %s\n", what);
    }
}

/* longest line in a "\n"-separated block */
static size_t longest_line(const char* s) {
    size_t best = 0, cur = 0;
    for(const char* p = s; *p; p++) {
        if(*p == '\n') {
            if(cur > best) best = cur;
            cur = 0;
        } else {
            cur++;
        }
    }
    return cur > best ? cur : best;
}

/* every word of `s`, in order, space-separated and collapsed */
static void words_of(const char* s, char* out, size_t cap) {
    size_t w = 0;
    int first = 1;
    const char* p = s;
    while(*p) {
        while(*p == ' ' || *p == '\n') p++;
        if(!*p) break;
        if(!first && w + 1 < cap) out[w++] = ' ';
        first = 0;
        while(*p && *p != ' ' && *p != '\n' && w + 1 < cap) out[w++] = *p++;
        while(*p && *p != ' ' && *p != '\n') p++;
    }
    out[w < cap ? w : cap - 1] = '\0';
}

int main(void) {
    char buf[512];

    /* ---- the exact line from the hardware screenshot ---------------------- */
    {
        const char* detail = "contact 2 at 8s field 17% peak 100% m:boost";
        size_t n = specter_log_wrap(buf, sizeof(buf), "WATCH", detail);
        check(n > 0, "the reported entry wraps at all");
        check(longest_line(buf) <= SPECTER_LOG_LINE_MAX, "no line is wider than the TextBox");
        check(strstr(buf, "fiel\n") == NULL, "'field' is no longer torn in half");
        check(strstr(buf, "field") != NULL, "'field' survives intact");
        check(buf[0] == ' ' && buf[1] == ' ', "the entry is indented as a detail line");
        check(strncmp(buf + 2, "WATCH", 5) == 0, "the type is where the filter looks for it");
        check(buf[n - 1] == '\n', "the block ends with a newline");

        char got[256], want[256];
        words_of(buf + 2 + SPECTER_LOG_TYPE_W, got, sizeof(got));
        words_of(detail, want, sizeof(want));
        check(strcmp(got, want) == 0, "wrapping loses no words");
    }

    /* ---- every real format the app writes --------------------------------- */
    {
        const struct {
            const char* type;
            const char* detail;
        } real[] = {
            {"WATCH", "contact 2 at 8s field 17% peak 100% m:boost"},
            {"SURVEY", "60s ACTIVE READER peak 74% avg 21% infield 38% hits 5 m:boost"},
            {"SWEEP", "field 78% peak 86% hits 3 m:boost"},
            {"READER", "POLLING period 204ms burst 24ms duty 11% conf 88%"},
            {"READER", "CONTINUOUS duty 98% conf 100%"},
            {"SURVEY", "10s TOO SHORT peak 0% avg 0% infield 0% hits 0 m:raw"},
        };
        for(size_t i = 0; i < sizeof(real) / sizeof(real[0]); i++) {
            size_t n = specter_log_wrap(buf, sizeof(buf), real[i].type, real[i].detail);
            check(n > 0, "a real entry wraps");
            check(longest_line(buf) <= SPECTER_LOG_LINE_MAX, "a real entry stays inside the width");

            char got[256], want[256];
            words_of(buf + 2 + SPECTER_LOG_TYPE_W, got, sizeof(got));
            words_of(real[i].detail, want, sizeof(want));
            check(strcmp(got, want) == 0, "a real entry loses no words");

            /* continuation lines must stay indented or the filter disowns them */
            int ok_indent = 1;
            for(const char* p = buf; *p; p++) {
                if(*p == '\n' && *(p + 1) != '\0' && *(p + 1) != ' ') ok_indent = 0;
            }
            check(ok_indent, "every continuation line is indented");
        }
    }

    /* ---- edges ------------------------------------------------------------ */
    {
        size_t n = specter_log_wrap(buf, sizeof(buf), "SWEEP", "");
        check(n > 0 && longest_line(buf) <= SPECTER_LOG_LINE_MAX, "an empty detail is harmless");

        n = specter_log_wrap(buf, sizeof(buf), "READER", "short one");
        check(strchr(buf, '\n') == buf + strlen(buf) - 1, "a short detail stays on one line");

        /* a single word longer than a line has to be split somewhere */
        n = specter_log_wrap(buf, sizeof(buf), "SWEEP", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        check(n > 0, "an over-long word does not fail the write");
        check(longest_line(buf) <= SPECTER_LOG_LINE_MAX, "an over-long word is hard-split, not overflowed");

        /* too small a buffer must fail cleanly rather than truncate an entry */
        char tiny[8];
        n = specter_log_wrap(tiny, sizeof(tiny), "SURVEY", "60s ACTIVE READER peak 74%");
        check(n == 0 && tiny[0] == '\0', "a buffer that cannot hold the entry yields nothing");
        check(specter_log_wrap(NULL, 0, "X", "y") == 0, "a null buffer is refused");
    }

    /* ---- the property that actually matters: it is still ONE entry -------- */
    {
        char book[1024];
        size_t w = 0;
        const struct {
            const char* stamp;
            const char* type;
            const char* detail;
        } entries[] = {
            {"2026-09-21 12:19:28", "WATCH", "contact 2 at 8s field 17% peak 100% m:boost"},
            {"2026-09-21 12:20:02", "SWEEP", "field 78% peak 86% hits 3 m:boost"},
            {"2026-09-21 12:21:44", "WATCH", "contact 3 at 96s field 61% peak 100% m:boost"},
        };
        for(size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
            w += (size_t)snprintf(book + w, sizeof(book) - w, "%s\n", entries[i].stamp);
            w += specter_log_wrap(book + w, sizeof(book) - w, entries[i].type, entries[i].detail);
        }

        char out[1024];
        size_t kept = specter_log_filter(book, "WATCH", out, sizeof(out));
        check(kept == 2, "filtering by WATCH keeps both WATCH entries");
        check(strstr(out, "2026-09-21 12:19:28") != NULL, "a wrapped entry keeps its timestamp");
        check(strstr(out, "m:boost") != NULL, "and keeps the tail of its wrapped detail");
        check(strstr(out, "SWEEP") == NULL, "and drops the entry that did not match");

        kept = specter_log_filter(book, "SWEEP", out, sizeof(out));
        check(kept == 1, "filtering by SWEEP keeps exactly the one");
        check(strstr(out, "hits 3") != NULL, "including its wrapped remainder");
    }

    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
