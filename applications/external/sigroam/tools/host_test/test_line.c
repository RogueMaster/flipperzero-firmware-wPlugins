#include "sr_test.h"

#include "sr_types.h"
#include "sr_line.h"

#include <string.h>
#include <stdio.h>

/* -------------------------------------------------------------------------- */
/* Sizes locked down                                                          */
/*                                                                            */
/* The benefit of ADR-010 must be enforced at compile time, or a future       */
/* contributor who stuffs an array into the union will silently regress back  */
/* to 528 B -- with zero symptoms on the host, only blowing up on Flipper's   */
/* 2048 B worker stack. We use sizeof expressions instead of hardcoded        */
/* numbers: the device is 32-bit ARM, pointers are 4 bytes.                   */
/* -------------------------------------------------------------------------- */

_Static_assert(
    sizeof(SrRawView) == sizeof(const char*) + sizeof(size_t),
    "SrRawView must stay a borrowed pointer+len view (ADR-010)");
_Static_assert(sizeof(SrEvent) <= 256, "SrEvent regressed toward the 528 B copy layout");
_Static_assert(sizeof(SrLine) <= 576, "SrLine over Plan 3.5 line-buffer budget");

/* -------------------------------------------------------------------------- */
/* Line-sequence container + a standalone oracle                              */
/* -------------------------------------------------------------------------- */

enum { ORACLE_MAX_LINES = 1100 }; /* fuzz input caps at 2048 B; worst case is "a\n" x1024 */

typedef struct {
    char text[SR_RAW_LINE_MAX + 1];
    size_t len;
    bool truncated;
} OracleLine;

typedef struct {
    OracleLine lines[ORACLE_MAX_LINES];
    size_t count;
    bool overflow_lines;
    uint32_t empty;
} OracleOut;

/* static: each is ~312 KB, kept off the stack. */
static OracleOut g_exp;
static OracleOut g_got_a;
static OracleOut g_got_b;

static void out_push(OracleOut* o, const char* text, size_t len, bool truncated) {
    if(o->count >= (size_t)ORACLE_MAX_LINES) {
        o->overflow_lines = true;
        return;
    }
    memcpy(o->lines[o->count].text, text, len);
    o->lines[o->count].text[len] = '\0';
    o->lines[o->count].len = len;
    o->lines[o->count].truncated = truncated;
    o->count++;
}

/*
 * Reference implementation. **Deliberately written differently from sr_line.c**: this one is
 * segment-based -- split on the newline character first, then filter and truncate within each
 * segment. If both sides used the same incremental state machine, the comparison would degrade
 * into a tautology.
 */
static void oracle_run(const char* data, size_t n, OracleOut* o) {
    size_t seg_start = 0;
    size_t i;

    memset(o, 0, sizeof(*o));

    for(i = 0; i < n; i++) {
        char clean[SR_RAW_LINE_MAX + 1];
        size_t clen = 0;
        bool trunc = false;
        size_t j;

        if(data[i] != '\n') {
            continue;
        }

        for(j = seg_start; j < i; j++) {
            unsigned char c = (unsigned char)data[j];
            if(c == '\r' || c < 0x20U || c == 0x7FU) {
                continue;
            }
            if(clen < (size_t)SR_RAW_LINE_MAX) {
                clean[clen++] = (char)c;
            } else {
                trunc = true;
            }
        }
        seg_start = i + 1;

        if(clen == 0) {
            o->empty++;
            continue;
        }
        clean[clen] = '\0';
        out_push(o, clean, clen, trunc);
    }
    /* seg_start..n is the ragged tail half-line: without a '\n' it is never delivered. */
}

/* -------------------------------------------------------------------------- */
/* Run the same input through sr_line                                         */
/* -------------------------------------------------------------------------- */

static void collect_ready(OracleOut* o, const SrLine* l) {
    size_t len = 0;
    const char* t = sr_line_text(l, &len);
    size_t k;

    CHECK(t != NULL);
    if(t == NULL) {
        return;
    }
    CHECK(len <= (size_t)SR_RAW_LINE_MAX);
    CHECK(strlen(t) == len);
    for(k = 0; k < len; k++) {
        unsigned char c = (unsigned char)t[k];
        CHECK(c >= 0x20U && c != 0x7FU); /* no control characters may remain */
    }
    out_push(o, t, len, sr_line_truncated(l));
}

/*
 * chunk == 0 means feed everything in one call; otherwise feed it in chunks of the given size.
 * This also checks two invariants along the way: consumption never exceeds what was fed in, and
 * there is always progress while not yet ready.
 */
static void line_run(const char* data, size_t n, size_t chunk, OracleOut* o, SrLine* l) {
    size_t off = 0;

    memset(o, 0, sizeof(*o));
    sr_line_init(l);

    while(off < n) {
        size_t end = (chunk == 0) ? n : off + chunk;
        size_t p;
        if(end > n) {
            end = n;
        }
        p = off;
        while(p < end) {
            size_t used = sr_line_feed(l, data + p, end - p);
            CHECK(used <= end - p);
            CHECK(used >= 1); /* not-ready on entry to feed guarantees progress */
            if(used == 0) {
                return; /* guards against an infinite loop -- already flagged by the CHECK above */
            }
            p += used;
            if(sr_line_ready(l)) {
                collect_ready(o, l);
                sr_line_consume(l);
            }
        }
        off = end;
    }
    o->empty = l->lines_empty;
}

static bool lines_equal(const OracleOut* a, const OracleOut* b) {
    size_t i;
    if(a->count != b->count || a->overflow_lines != b->overflow_lines) {
        return false;
    }
    for(i = 0; i < a->count; i++) {
        if(a->lines[i].len != b->lines[i].len) {
            return false;
        }
        if(a->lines[i].truncated != b->lines[i].truncated) {
            return false;
        }
        if(strcmp(a->lines[i].text, b->lines[i].text) != 0) {
            return false;
        }
    }
    return true;
}

/* -------------------------------------------------------------------------- */
/* Targeted test cases                                                        */
/* -------------------------------------------------------------------------- */

static void test_basic_lines(void) {
    /* V-002's real line shape (data lines end in a literal '\n') */
    static const char in[] = "12 | AA:BB:CC:DD:EE:FF,Home_WiFi,[WPA2_PSK],"
                             "2026-08-16 12:00:00,6,-67,31.23,121.47,10.0,5.0,WIFI\n"
                             "11:22:33:44:55:66,,[BLE],2026-08-16 12:00:01,0,-80,"
                             "31.23,121.47,10.0,5.0,BLE\n";
    SrLine l;

    line_run(in, sizeof(in) - 1, 0, &g_got_a, &l);
    CHECK(g_got_a.count == 2);
    CHECK(g_got_a.lines[0].truncated == false);
    CHECK(strncmp(g_got_a.lines[0].text, "12 | AA:BB:CC:DD:EE:FF,", 23) == 0);
    CHECK(strncmp(g_got_a.lines[1].text, "11:22:33:44:55:66,,[BLE],", 25) == 0);
    CHECK(l.lines_total == 2);
    CHECK(l.lines_truncated == 0);
    CHECK(l.ctrl_dropped == 0);
    CHECK(l.cr_dropped == 0);
}

static void test_crlf_and_double_terminator(void) {
    SrLine l;

    /*
     * V-008: the CLI's println terminator is unverified, and help/ascii_art already contain a
     * literal \r\n. CRLF, \r\r\n, and the blank lines produced when println stacks a literal \r\n
     * on top must all be swallowed.
     */
    static const char in[] = "Firmware: Marauder\r\n"
                             "Version: v1.14.1\r\r\n"
                             "\r\n"
                             "Hardware: ESP32-C5 DevKit\n";
    line_run(in, sizeof(in) - 1, 0, &g_got_a, &l);
    CHECK(g_got_a.count == 3);
    CHECK(strcmp(g_got_a.lines[0].text, "Firmware: Marauder") == 0);
    CHECK(strcmp(g_got_a.lines[1].text, "Version: v1.14.1") == 0);
    CHECK(strcmp(g_got_a.lines[2].text, "Hardware: ESP32-C5 DevKit") == 0);
    CHECK(l.cr_dropped == 4);
    CHECK(l.lines_empty == 1);

    /* an in-line '\r' is dropped the same way, without splitting the line */
    {
        static const char in2[] = "a\rb\n";
        line_run(in2, sizeof(in2) - 1, 0, &g_got_a, &l);
        CHECK(g_got_a.count == 1);
        CHECK(strcmp(g_got_a.lines[0].text, "ab") == 0);
    }
}

static void test_empty_lines_dropped(void) {
    static const char in[] = "a\n\n\n\r\n\r\rb\n";
    SrLine l;

    line_run(in, sizeof(in) - 1, 0, &g_got_a, &l);
    CHECK(g_got_a.count == 2);
    CHECK(strcmp(g_got_a.lines[0].text, "a") == 0);
    CHECK(strcmp(g_got_a.lines[1].text, "b") == 0);
    CHECK(l.lines_empty == 3);
    CHECK(l.lines_total == 2);
}

static void test_ctrl_and_nul_dropped(void) {
    /* embedded NUL: keeping it would make len and strlen disagree */
    static const char in[] = {'a', '\0', 'b', 0x01, 0x7F, '\t', 0x1F, 'c', '\n'};
    SrLine l;
    size_t len = 0;
    const char* t;

    memset(&g_got_a, 0, sizeof(g_got_a));
    sr_line_init(&l);
    CHECK(sr_line_feed(&l, in, sizeof(in)) == sizeof(in));
    CHECK(sr_line_ready(&l) == true);
    t = sr_line_text(&l, &len);
    CHECK(t != NULL);
    CHECK(len == 3);
    CHECK(strlen(t) == 3);
    CHECK(strcmp(t, "abc") == 0);
    CHECK(l.ctrl_dropped == 5); /* NUL 0x01 0x7F TAB 0x1F */
    CHECK(l.cr_dropped == 0);
}

static void test_high_bytes_preserved(void) {
    /*
     * Real SSIDs often contain UTF-8. ADR-010: the data layer must not corrupt it (a departure
     * from the Plan §4 P8 wording).
     */
    static const char in[] = "AA:BB:CC:DD:EE:FF,\xE5\xB0\x8F\xE7\xB1\xB3_5G,[WPA2_PSK]\n";
    SrLine l;
    size_t len = 0;
    const char* t;

    memset(&g_got_a, 0, sizeof(g_got_a));
    sr_line_init(&l);
    sr_line_feed(&l, in, sizeof(in) - 1);
    CHECK(sr_line_ready(&l) == true);
    t = sr_line_text(&l, &len);
    CHECK(t != NULL);
    CHECK(len == sizeof(in) - 2); /* only the trailing '\n' is missing */
    CHECK(strstr(t, "\xE5\xB0\x8F\xE7\xB1\xB3_5G") != NULL);
    CHECK(l.ctrl_dropped == 0);
}

static void test_truncation(void) {
    static char in[2048];
    SrLine l;
    size_t len = 0;
    const char* t;
    size_t pos;

    /* exactly 511: no truncation */
    memset(in, 'A', (size_t)SR_RAW_LINE_MAX);
    in[SR_RAW_LINE_MAX] = '\n';
    sr_line_init(&l);
    sr_line_feed(&l, in, (size_t)SR_RAW_LINE_MAX + 1U);
    CHECK(sr_line_ready(&l) == true);
    CHECK(sr_line_truncated(&l) == false);
    t = sr_line_text(&l, &len);
    CHECK(len == (size_t)SR_RAW_LINE_MAX);
    CHECK(t != NULL && strlen(t) == (size_t)SR_RAW_LINE_MAX);
    CHECK(l.overflow_dropped == 0);

    /* 512: truncates by one byte */
    memset(in, 'B', (size_t)SR_RAW_LINE_MAX + 1U);
    in[SR_RAW_LINE_MAX + 1] = '\n';
    sr_line_init(&l);
    sr_line_feed(&l, in, (size_t)SR_RAW_LINE_MAX + 2U);
    CHECK(sr_line_ready(&l) == true);
    CHECK(sr_line_truncated(&l) == true);
    t = sr_line_text(&l, &len);
    CHECK(len == (size_t)SR_RAW_LINE_MAX);
    CHECK(t != NULL && strlen(t) == (size_t)SR_RAW_LINE_MAX);
    CHECK(l.overflow_dropped == 1);
    CHECK(l.lines_truncated == 1);

    /*
     * A 1000-char overlong line immediately followed by a normal line: the overflow state must be
     * fully cleared.
     */
    memset(in, 'C', 1000);
    in[1000] = '\n';
    pos = 1001;
    memcpy(in + pos, "ok\n", 3);
    pos += 3;

    sr_line_init(&l);
    memset(&g_got_a, 0, sizeof(g_got_a));
    line_run(in, pos, 0, &g_got_a, &l);
    CHECK(g_got_a.count == 2);
    CHECK(g_got_a.lines[0].truncated == true);
    CHECK(g_got_a.lines[0].len == (size_t)SR_RAW_LINE_MAX);
    CHECK(g_got_a.lines[1].truncated == false);
    CHECK(strcmp(g_got_a.lines[1].text, "ok") == 0);
    CHECK(l.lines_total == 2);
    CHECK(l.lines_truncated == 1);
    CHECK(l.overflow_dropped == 1000U - (uint32_t)SR_RAW_LINE_MAX);

    /* behavior is identical when the truncated line is fed in byte by byte */
    memset(&g_got_b, 0, sizeof(g_got_b));
    line_run(in, pos, 1, &g_got_b, &l);
    CHECK(lines_equal(&g_got_a, &g_got_b));
}

static void test_ready_backpressure(void) {
    static const char in[] = "one\ntwo\n";
    SrLine l;
    size_t used;

    sr_line_init(&l);
    used = sr_line_feed(&l, in, sizeof(in) - 1);
    CHECK(used == 4); /* only consumes up to the first '\n' */
    CHECK(sr_line_ready(&l) == true);

    /* feeding again before consuming returns 0 and does not overwrite the ready line */
    CHECK(sr_line_feed(&l, in + used, sizeof(in) - 1 - used) == 0);
    CHECK(strcmp(sr_line_text(&l, NULL), "one") == 0);
    CHECK(l.lines_total == 1);

    sr_line_consume(&l);
    CHECK(sr_line_ready(&l) == false);
    CHECK(sr_line_text(&l, NULL) == NULL);
    CHECK(sr_line_truncated(&l) == false);

    used += sr_line_feed(&l, in + used, sizeof(in) - 1 - used);
    CHECK(used == sizeof(in) - 1);
    CHECK(sr_line_ready(&l) == true);
    CHECK(strcmp(sr_line_text(&l, NULL), "two") == 0);
}

static void test_partial_tail(void) {
    SrLine l;

    /*
     * Without a '\n' nothing is delivered -- a half-line must never be fed to the parser as
     * though it were a complete line.
     */
    sr_line_init(&l);
    sr_line_feed(&l, "AA:BB:CC", 8);
    CHECK(sr_line_ready(&l) == false);
    CHECK(sr_line_text(&l, NULL) == NULL);
    CHECK(l.lines_total == 0);

    /* only delivered once the remaining bytes arrive, and the concatenation is correct */
    sr_line_feed(&l, ":DD:EE:FF\n", 10);
    CHECK(sr_line_ready(&l) == true);
    CHECK(strcmp(sr_line_text(&l, NULL), "AA:BB:CC:DD:EE:FF") == 0);
}

static void test_reset_and_init(void) {
    SrLine l;

    sr_line_init(&l);
    sr_line_feed(&l, "half", 4);
    sr_line_feed(&l, "\nx\n", 3); /* delivers "half"; "x" has not finished yet */
    CHECK(sr_line_ready(&l) == true);

    /* reset: drops the half-line and any unconsumed ready line, keeps the stats */
    sr_line_reset(&l);
    CHECK(sr_line_ready(&l) == false);
    CHECK(l.len == 0);
    CHECK(l.lines_total == 1);

    sr_line_feed(&l, "new\n", 4);
    CHECK(sr_line_ready(&l) == true);
    CHECK(strcmp(sr_line_text(&l, NULL), "new") == 0);
    CHECK(l.lines_total == 2);

    /* init: clears the stats along with everything else */
    sr_line_init(&l);
    CHECK(l.lines_total == 0);
    CHECK(l.lines_empty == 0);
    CHECK(l.cr_dropped == 0);
    CHECK(l.ctrl_dropped == 0);
    CHECK(l.overflow_dropped == 0);
    CHECK(sr_line_ready(&l) == false);

    /* NULL arguments must not crash */
    sr_line_init(NULL);
    sr_line_reset(NULL);
    sr_line_consume(NULL);
    CHECK(sr_line_feed(NULL, "a", 1) == 0);
    CHECK(sr_line_feed(&l, NULL, 1) == 0);
    CHECK(sr_line_feed(&l, "a", 0) == 0);
    CHECK(sr_line_ready(NULL) == false);
    CHECK(sr_line_truncated(NULL) == false);
    {
        size_t len = 123;
        CHECK(sr_line_text(NULL, &len) == NULL);
        CHECK(len == 0);
    }
}

/* -------------------------------------------------------------------------- */
/* Fuzz                                                                       */
/* -------------------------------------------------------------------------- */

#define SR_FUZZ_ITERS 20000u
#define SR_FUZZ_SEED  0x5A17C0DEu

static uint32_t xs32(uint32_t* s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

/*
 * nl_in controls the newline density: there is a 1/nl_in chance of producing a newline character.
 * The low density modes exist specifically to generate overlong lines, to cover the truncation
 * path.
 */
static char fuzz_byte(uint32_t* s, uint32_t nl_in) {
    uint32_t r = xs32(s);
    if((r % nl_in) == 0) {
        return '\n';
    }
    switch((r >> 8) % 8) {
    case 0:
        return '\r';
    case 1:
        return (char)((r >> 16) & 0x1FU); /* control characters, including NUL */
    case 2:
        return (char)0x7F;
    case 3:
    case 4:
        return (char)(0x80U | ((r >> 16) & 0x7FU)); /* high-bit byte */
    default:
        return (char)(0x20U + ((r >> 16) % 95U)); /* printable ASCII */
    }
}

static void test_fuzz(void) {
    static char buf[2048];
    /*
     * Newline density comes in four tiers. The last tier almost never emits a newline,
     * specifically to feed overlong lines -- the first version only had three tiers (8/64/2048)
     * with a 1024 B buf, and across 20000 iterations that only ever hit 18 truncated lines,
     * meaning the truncation path went untested. See WORKLOG for the measured numbers.
     */
    static const uint32_t NL_DENSITY[4] = {8u, 64u, 512u, 1000000u};
    uint32_t seed = SR_FUZZ_SEED;
    uint32_t iter;
    int reported = 0;
    uint32_t cov_lines = 0, cov_trunc = 0, cov_empty = 0;
    uint32_t cov_ctrl = 0, cov_cr = 0, cov_high = 0;

    for(iter = 0; iter < SR_FUZZ_ITERS; iter++) {
        uint32_t case_seed = seed;
        uint32_t nl_in = NL_DENSITY[iter % 4u];
        size_t n = (size_t)(xs32(&seed) % sizeof(buf)) + 1U;
        size_t i;
        size_t chunk;
        SrLine l;

        for(i = 0; i < n; i++) {
            buf[i] = fuzz_byte(&seed, nl_in);
        }
        /*
         * The lowest-density bucket forces a trailing newline; otherwise the whole segment would
         * have no terminator, no line would ever be delivered, and the truncation path could
         * never be reached. The other three buckets are left natural, to cover the "ragged tail
         * half-line" case.
         */
        if((iter % 4u) == 3u) {
            buf[n - 1U] = '\n';
        }

        oracle_run(buf, n, &g_exp);
        line_run(buf, n, 0, &g_got_a, &l); /* feed it all at once */

        chunk = (size_t)(xs32(&seed) % 7U) + 1U;
        line_run(buf, n, chunk, &g_got_b, &l); /* feed it in random chunks */

        if(!lines_equal(&g_exp, &g_got_a) || !lines_equal(&g_exp, &g_got_b)) {
            sr_test_failures++;
            if(reported < 5) {
                fprintf(
                    stderr,
                    "fuzz mismatch: iter=%u case_seed=0x%08X n=%zu nl_in=%u chunk=%zu "
                    "(oracle=%zu once=%zu chunked=%zu)\n",
                    iter,
                    case_seed,
                    n,
                    nl_in,
                    chunk,
                    g_exp.count,
                    g_got_a.count,
                    g_got_b.count);
                reported++;
            }
        }
        if(g_exp.count == g_got_a.count) {
            CHECK(g_exp.empty == g_got_a.empty);
        }

        cov_lines += l.lines_total;
        cov_trunc += l.lines_truncated;
        cov_empty += l.lines_empty;
        cov_ctrl += l.ctrl_dropped;
        cov_cr += l.cr_dropped;
        for(i = 0; i < g_got_a.count; i++) {
            size_t k;
            for(k = 0; k < g_got_a.lines[i].len; k++) {
                if((unsigned char)g_got_a.lines[i].text[k] >= 0x80U) {
                    cov_high++;
                }
            }
        }
    }

    fprintf(
        stderr,
        "fuzz coverage: lines=%u trunc=%u empty=%u ctrl=%u cr=%u high=%u\n",
        cov_lines,
        cov_trunc,
        cov_empty,
        cov_ctrl,
        cov_cr,
        cov_high);

    /*
     * Coverage floor. The seed is hardcoded, so these numbers are deterministic and will not
     * flake at random. Thresholds are set to roughly 1/3 of the measured values: enough headroom
     * for minor parameter tweaks, but an immediate failure the moment some path stops being
     * exercised. "Ran it 20000 times" does not mean "it got covered" -- the first version trunc
     * count of only 18 is exactly that example.
     */
    CHECK(cov_lines > 200000u);
    CHECK(cov_trunc > 200u);   /* overlong truncation path */
    CHECK(cov_empty > 40000u); /* empty-line drop path */
    CHECK(cov_ctrl > 1000000u);
    CHECK(cov_cr > 500000u);
    CHECK(cov_high > 1000000u); /* >= 0x80 preserved-byte path */
}

/* -------------------------------------------------------------------------- */

int test_line_run(void) {
    sr_test_failures = 0;
    test_basic_lines();
    test_crlf_and_double_terminator();
    test_empty_lines_dropped();
    test_ctrl_and_nul_dropped();
    test_high_bytes_preserved();
    test_truncation();
    test_ready_backpressure();
    test_partial_tail();
    test_reset_and_init();
    test_fuzz();
    return sr_test_failures;
}
