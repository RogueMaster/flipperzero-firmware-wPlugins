#include "sr_test.h"

#include "sr_rawlog.h"
#include "sr_model.h"
#include "sr_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

static void fill_bytes(char* dst, size_t n, char c) {
    size_t i;
    for(i = 0; i < n; i++) {
        dst[i] = c;
    }
}

static unsigned test_empty_render(void) {
    SrRawLog log;
    char out[16];
    size_t n;

    sr_rawlog_init(&log);
    memset(out, 0xAA, sizeof(out));
    n = sr_rawlog_render(&log, out, sizeof(out));
    CHECK(n == 0);
    CHECK(out[0] == '\0');
    return 1;
}

static unsigned test_lens(void) {
    SrRawLog log;
    char one[1];
    char exact[SR_RAWLOG_LINE_MAX];
    char over[SR_RAWLOG_LINE_MAX + 1];
    char huge[SR_RAW_LINE_MAX];
    unsigned hits = 0;

    sr_rawlog_init(&log);

    sr_rawlog_push(&log, "x", 0);
    CHECK(log.count == 1);
    CHECK(log.e[0].len == 0);
    CHECK(log.e[0].cut == false);
    CHECK(log.e[0].text[0] == '\0');
    hits++;

    one[0] = 'A';
    sr_rawlog_push(&log, one, 1);
    CHECK(log.count == 2);
    CHECK(log.e[1].len == 1);
    CHECK(log.e[1].cut == false);
    CHECK(log.e[1].text[0] == 'A');
    CHECK(log.e[1].text[1] == '\0');
    hits++;

    fill_bytes(exact, (size_t)SR_RAWLOG_LINE_MAX, 'B');
    sr_rawlog_push(&log, exact, (size_t)SR_RAWLOG_LINE_MAX);
    CHECK(log.e[2].len == (uint8_t)SR_RAWLOG_LINE_MAX);
    CHECK(log.e[2].cut == false);
    CHECK(log.e[2].text[0] == 'B');
    CHECK(log.e[2].text[SR_RAWLOG_LINE_MAX - 1] == 'B');
    CHECK(log.e[2].text[SR_RAWLOG_LINE_MAX] == '\0');
    hits++;

    fill_bytes(over, (size_t)SR_RAWLOG_LINE_MAX + 1u, 'C');
    sr_rawlog_push(&log, over, (size_t)SR_RAWLOG_LINE_MAX + 1u);
    CHECK(log.e[3].len == (uint8_t)SR_RAWLOG_LINE_MAX);
    CHECK(log.e[3].cut == true);
    CHECK(log.e[3].text[0] == 'C');
    CHECK(log.e[3].text[SR_RAWLOG_LINE_MAX] == '\0');
    hits++;

    fill_bytes(huge, (size_t)SR_RAW_LINE_MAX, 'D');
    sr_rawlog_push(&log, huge, (size_t)SR_RAW_LINE_MAX);
    CHECK(log.e[4].len == (uint8_t)SR_RAWLOG_LINE_MAX);
    CHECK(log.e[4].cut == true);
    CHECK(log.e[4].text[0] == 'D');
    hits++;

    CHECK(log.pushed == 5);
    return hits;
}

static unsigned test_no_overread(void) {
    SrRawLog log;
    char* buf;
    unsigned i;

    /* An 8 B heap block, all non-zero bytes, no NUL terminator. A
     * strlen-based implementation must trigger an ASan heap-buffer-overflow
     * report. */
    buf = malloc(8);
    CHECK(buf != NULL);
    if(buf == NULL) {
        return 0;
    }
    for(i = 0; i < 8u; i++) {
        buf[i] = (char)(0x41 + i);
    }

    sr_rawlog_init(&log);
    sr_rawlog_push(&log, buf, 8);
    CHECK(log.count == 1);
    CHECK(log.e[0].len == 8);
    CHECK(log.e[0].cut == false);
    CHECK(memcmp(log.e[0].text, "ABCDEFGH", 8) == 0);
    CHECK(log.e[0].text[8] == '\0');

    free(buf);
    return 1;
}

static unsigned test_wrap(void) {
    SrRawLog log;
    char line[3];
    char out[SR_RAWLOG_LINES * 8];
    size_t n;
    unsigned i;

    sr_rawlog_init(&log);
    for(i = 0; i < 17u; i++) {
        line[0] = (char)('A' + (i / 10u));
        line[1] = (char)('0' + (i % 10u));
        line[2] = (char)0xFF; /* no NUL here; read only by len=2 */
        sr_rawlog_push(&log, line, 2);
    }

    CHECK(log.count == (size_t)SR_RAWLOG_LINES);
    CHECK(log.pushed == 17);
    /* Entry 0 "A0" has already been overwritten; the oldest is entry 1 "A1" */
    CHECK(log.e[0].text[0] == 'B');
    CHECK(log.e[0].text[1] == '6'); /* i=16 → B6 */
    CHECK(log.e[1].text[0] == 'A');
    CHECK(log.e[1].text[1] == '1');

    n = sr_rawlog_render(&log, out, sizeof(out));
    CHECK(n > 2);
    CHECK(out[0] == 'A');
    CHECK(out[1] == '1');
    CHECK(out[2] == '\n');
    return 1;
}

static unsigned test_sanitize(void) {
    SrRawLog log;
    unsigned char src[10];
    const char* expect = "........ ~";
    unsigned hits = 0;
    unsigned i;

    src[0] = '\r';
    src[1] = '\n';
    src[2] = '\t';
    src[3] = 0x00;
    src[4] = 0x1F;
    src[5] = 0x7F;
    src[6] = 0x80;
    src[7] = 0xE4;
    src[8] = 0x20;
    src[9] = 0x7E;

    sr_rawlog_init(&log);
    sr_rawlog_push(&log, (const char*)src, 10);
    CHECK(log.e[0].len == 10);
    CHECK(log.e[0].cut == false);
    CHECK(memcmp(log.e[0].text, expect, 10) == 0);

    for(i = 0; i < 8u; i++) {
        CHECK(log.e[0].text[i] == '.');
        hits++;
    }
    CHECK(log.e[0].text[8] == ' ');
    CHECK(log.e[0].text[9] == '~');
    return hits;
}

static unsigned test_render_cap(void) {
    SrRawLog log;
    char* buf;
    size_t need;
    size_t n;
    char scratch[64];

    sr_rawlog_init(&log);
    sr_rawlog_push(&log, "hello", 5);
    need = sr_rawlog_render(&log, scratch, sizeof(scratch));
    CHECK(need == 6); /* "hello\n" */
    CHECK(scratch[need] == '\0');

    /* cap is 1 B less than "content + NUL": still NUL-terminated, no overflow */
    buf = malloc(need); /* just enough for need-1 chars + NUL */
    CHECK(buf != NULL);
    if(buf == NULL) {
        return 0;
    }
    memset(buf, 0xAA, need);
    n = sr_rawlog_render(&log, buf, need);
    CHECK(n < need);
    CHECK(buf[n] == '\0');
    CHECK(n == need - 1u);
    free(buf);
    return 1;
}

static unsigned test_nulls(void) {
    SrRawLog log;
    char out[8];
    size_t n;
    unsigned hits = 0;

    sr_rawlog_init(&log);
    sr_rawlog_push(NULL, "abcd", 5);
    sr_rawlog_push(&log, NULL, 5);
    CHECK(log.count == 0);
    CHECK(log.pushed == 0);
    hits++;

    memset(out, 0xAA, sizeof(out));
    n = sr_rawlog_render(&log, out, sizeof(out));
    CHECK(n == 0);
    CHECK(out[0] == '\0');
    hits++;
    return hits;
}

/* Independent oracle: XOR is non-zero. Written differently from the
 * implementation's != check; writing it as > would diverge from this at
 * the wraparound point. */
static bool oracle_should_render(uint32_t pushed_now, uint32_t pushed_shown) {
    return (pushed_now ^ pushed_shown) != 0u;
}

static unsigned test_should_render_same(void) {
    bool got;

    got = sr_rawlog_should_render(0u, 0u);
    CHECK(got == false);
    CHECK(got == oracle_should_render(0u, 0u));
    return 1;
}

static unsigned test_should_render_diff(void) {
    bool got;
    unsigned hits = 0;

    got = sr_rawlog_should_render(5u, 3u);
    CHECK(got == true);
    CHECK(got == oracle_should_render(5u, 3u));
    hits++;

    got = sr_rawlog_should_render(0u, 1u);
    CHECK(got == true);
    CHECK(got == oracle_should_render(0u, 1u));
    hits++;

    return hits;
}

static unsigned test_should_render_wrap(void) {
    const uint32_t pushed_shown = 0xFFFFFFFFu;
    const uint32_t pushed_now = 0u;
    bool got;

    got = sr_rawlog_should_render(pushed_now, pushed_shown);
    CHECK(got == true);
    CHECK(oracle_should_render(pushed_now, pushed_shown) == true);
    return 1;
}

static unsigned test_e2e_model(void) {
    SrRawLog log;
    SrModel m;
    SrEvent ev;
    unsigned char raw[8];
    unsigned i;

    /* Window view without a NUL terminator: contains bytes that need sanitizing */
    raw[0] = 'H';
    raw[1] = '\n';
    raw[2] = 'i';
    raw[3] = 0x80;
    raw[4] = '~';
    raw[5] = 0xAA;
    raw[6] = 0xBB;
    raw[7] = 0xCC;

    sr_rawlog_init(&log);
    sr_model_init(&m, NULL, &log);

    memset(&ev, 0, sizeof(ev));
    ev.kind = SrEventUnknown;
    ev.u.unknown.text = (const char*)raw;
    ev.u.unknown.len = 5;

    CHECK(sr_model_apply(&m, &ev, 1) == true);
    CHECK(m.unknown_lines == 1);
    CHECK(log.count == 1);
    CHECK(log.pushed == 1);
    CHECK(log.e[0].len == 5);
    CHECK(log.e[0].cut == false);
    CHECK(log.e[0].text[0] == 'H');
    CHECK(log.e[0].text[1] == '.');
    CHECK(log.e[0].text[2] == 'i');
    CHECK(log.e[0].text[3] == '.');
    CHECK(log.e[0].text[4] == '~');
    CHECK(log.e[0].text[5] == '\0');
    for(i = 0; i < 5u; i++) {
        CHECK((unsigned char)log.e[0].text[i] != 0xAAu);
    }
    return 1;
}

int test_rawlog_run(void) {
    unsigned empty = 0;
    unsigned lens = 0;
    unsigned no_overread = 0;
    unsigned wrap = 0;
    unsigned sanitize = 0;
    unsigned cap = 0;
    unsigned nulls = 0;
    unsigned e2e = 0;
    unsigned render_same = 0;
    unsigned render_diff = 0;
    unsigned render_wrap = 0;

    sr_test_failures = 0;

    fprintf(stderr, "sizeof(SrModel)=%zu\n", sizeof(SrModel));
    fprintf(stderr, "sizeof(SrRawLog)=%zu\n", sizeof(SrRawLog));
    CHECK(sizeof(SrModel) <= 4096);
    CHECK(sizeof(SrRawLog) == 1352);
    CHECK(sizeof(SrModel) == 3328);

    empty = test_empty_render();
    lens = test_lens();
    no_overread = test_no_overread();
    wrap = test_wrap();
    sanitize = test_sanitize();
    cap = test_render_cap();
    nulls = test_nulls();
    e2e = test_e2e_model();
    render_same = test_should_render_same();
    render_diff = test_should_render_diff();
    render_wrap = test_should_render_wrap();

    printf(
        "rawlog cover: empty=%u lens=%u no_overread=%u wrap=%u sanitize=%u "
        "cap=%u nulls=%u e2e=%u render_same=%u render_diff=%u render_wrap=%u\n",
        empty,
        lens,
        no_overread,
        wrap,
        sanitize,
        cap,
        nulls,
        e2e,
        render_same,
        render_diff,
        render_wrap);

    CHECK(empty == 1);
    CHECK(lens == 5);
    CHECK(no_overread == 1);
    CHECK(wrap == 1);
    CHECK(sanitize == 8);
    CHECK(cap == 1);
    CHECK(nulls == 2);
    CHECK(e2e == 1);
    CHECK(render_same >= 1);
    CHECK(render_diff >= 2);
    CHECK(render_wrap >= 1);

    return sr_test_failures;
}
