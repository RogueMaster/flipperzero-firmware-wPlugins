#include "minitest.h"

#include "line_buffer.h"
#include "proto_encode.h"

#define MAX_LINES 8

static char storage[PROTO_LINE_MAX];
static char got[MAX_LINES][PROTO_LINE_MAX];
static size_t got_len[MAX_LINES];
static int got_count;

static void on_line(const char* line, size_t len, void* ctx) {
    (void)ctx;
    if(got_count < MAX_LINES) {
        memcpy(got[got_count], line, len + 1);
        got_len[got_count] = len;
    }
    got_count++;
}

static void setup(LineBuffer* lb, size_t cap) {
    got_count = 0;
    line_buffer_init(lb, storage, cap, on_line, NULL);
}

static size_t feed_str(LineBuffer* lb, const char* s) {
    return line_buffer_feed(lb, (const uint8_t*)s, strlen(s));
}

static void test_whole_line_in_one_packet(void) {
    LineBuffer lb;
    setup(&lb, sizeof(storage));
    CHECK_EQ_INT(feed_str(&lb, "{\"t\":\"hello\"}\n"), 1);
    CHECK_EQ_INT(got_count, 1);
    CHECK_STR_EQ(got[0], "{\"t\":\"hello\"}");
    CHECK_EQ_INT(got_len[0], 13);
}

static void test_line_split_across_packets(void) {
    LineBuffer lb;
    setup(&lb, sizeof(storage));
    CHECK_EQ_INT(feed_str(&lb, "{\"t\":"), 0);
    CHECK_EQ_INT(feed_str(&lb, "\"hel"), 0);
    CHECK_EQ_INT(got_count, 0);
    CHECK_EQ_INT(feed_str(&lb, "lo\"}\n"), 1);
    CHECK_EQ_INT(got_count, 1);
    CHECK_STR_EQ(got[0], "{\"t\":\"hello\"}");
}

static void test_two_lines_in_one_packet(void) {
    LineBuffer lb;
    setup(&lb, sizeof(storage));
    CHECK_EQ_INT(feed_str(&lb, "one\ntwo\nthr"), 2);
    CHECK_EQ_INT(got_count, 2);
    CHECK_STR_EQ(got[0], "one");
    CHECK_STR_EQ(got[1], "two");
    CHECK_EQ_INT(feed_str(&lb, "ee\n"), 1);
    CHECK_STR_EQ(got[2], "three");
}

static void test_crlf_is_stripped(void) {
    LineBuffer lb;
    setup(&lb, sizeof(storage));
    CHECK_EQ_INT(feed_str(&lb, "one\r\ntwo\r"), 1);
    CHECK_EQ_INT(feed_str(&lb, "\n"), 1);
    CHECK_STR_EQ(got[0], "one");
    CHECK_EQ_INT(got_len[0], 3);
    CHECK_STR_EQ(got[1], "two");
}

static void test_empty_lines_are_skipped(void) {
    LineBuffer lb;
    setup(&lb, sizeof(storage));
    CHECK_EQ_INT(feed_str(&lb, "\n\r\n\nx\n\n"), 1);
    CHECK_EQ_INT(got_count, 1);
    CHECK_STR_EQ(got[0], "x");
}

static void test_line_filling_the_buffer_is_kept(void) {
    LineBuffer lb;
    char line[17];
    setup(&lb, 16);
    memset(line, 'a', 15); /* cap 16 holds 15 chars and the NUL */
    line[15] = '\n';
    line[16] = '\0';
    CHECK_EQ_INT(feed_str(&lb, line), 1);
    CHECK_EQ_INT(got_len[0], 15);
    CHECK_EQ_INT(line_buffer_dropped(&lb), 0);
}

static void test_overlong_line_is_dropped_and_next_line_survives(void) {
    LineBuffer lb;
    setup(&lb, 16);
    CHECK_EQ_INT(feed_str(&lb, "0123456789"), 0);
    CHECK_EQ_INT(feed_str(&lb, "abcdef"), 0); /* 16 chars: one too many */
    CHECK_EQ_INT(feed_str(&lb, "still the same long line"), 0);
    CHECK_EQ_INT(feed_str(&lb, "\nok\n"), 1);
    CHECK_EQ_INT(got_count, 1);
    CHECK_STR_EQ(got[0], "ok");
    CHECK_EQ_INT(line_buffer_dropped(&lb), 1);
}

static void test_overlong_line_at_protocol_size(void) {
    static char big[PROTO_LINE_MAX + 2];
    LineBuffer lb;
    setup(&lb, sizeof(storage));
    memset(big, 'x', PROTO_LINE_MAX - 1); /* the largest line the protocol allows */
    big[PROTO_LINE_MAX - 1] = '\n';
    CHECK_EQ_INT(line_buffer_feed(&lb, (const uint8_t*)big, PROTO_LINE_MAX), 1);
    CHECK_EQ_INT(got_len[0], PROTO_LINE_MAX - 1);

    memset(big, 'y', PROTO_LINE_MAX); /* one byte more */
    big[PROTO_LINE_MAX] = '\n';
    CHECK_EQ_INT(line_buffer_feed(&lb, (const uint8_t*)big, PROTO_LINE_MAX + 1), 0);
    CHECK_EQ_INT(got_count, 1);
    CHECK_EQ_INT(line_buffer_dropped(&lb), 1);
}

static void test_reset_discards_partial_line(void) {
    LineBuffer lb;
    setup(&lb, sizeof(storage));
    feed_str(&lb, "partial");
    line_buffer_reset(&lb);
    CHECK_EQ_INT(feed_str(&lb, "next\n"), 1);
    CHECK_STR_EQ(got[0], "next");
}

static void test_reset_ends_overflow(void) {
    LineBuffer lb;
    setup(&lb, 8);
    feed_str(&lb, "much too long");
    line_buffer_reset(&lb);
    CHECK_EQ_INT(feed_str(&lb, "ok\n"), 1);
    CHECK_STR_EQ(got[0], "ok");
}

static void test_nul_bytes_drop_the_line(void) {
    LineBuffer lb;
    const uint8_t bad[] = {'a', 0, 'b', '\n', 'c', '\n'};
    setup(&lb, sizeof(storage));
    CHECK_EQ_INT(line_buffer_feed(&lb, bad, sizeof(bad)), 1);
    CHECK_EQ_INT(got_count, 1);
    CHECK_STR_EQ(got[0], "c");
    CHECK_EQ_INT(line_buffer_dropped(&lb), 1);
}

int main(void) {
    RUN_TEST(test_whole_line_in_one_packet);
    RUN_TEST(test_line_split_across_packets);
    RUN_TEST(test_two_lines_in_one_packet);
    RUN_TEST(test_crlf_is_stripped);
    RUN_TEST(test_empty_lines_are_skipped);
    RUN_TEST(test_line_filling_the_buffer_is_kept);
    RUN_TEST(test_overlong_line_is_dropped_and_next_line_survives);
    RUN_TEST(test_overlong_line_at_protocol_size);
    RUN_TEST(test_reset_discards_partial_line);
    RUN_TEST(test_reset_ends_overflow);
    RUN_TEST(test_nul_bytes_drop_the_line);
    return MINITEST_RESULT();
}
