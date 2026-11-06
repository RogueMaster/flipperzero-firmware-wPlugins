#include "minitest.h"

#include "tag_watch.h"

static const uint8_t uid_a[4] = {0x91, 0xB4, 0xE7, 0xC2};
static const uint8_t uid_b[7] = {0x04, 0xB7, 0xE2, 0x11, 0x6A, 0x3C, 0x80};

static void test_nothing_to_wait_for_at_first(void) {
    TagWatch w;
    tag_watch_clear(&w);
    CHECK(!tag_watch_waiting(&w));
    CHECK(!tag_watch_poll(&w, uid_a, sizeof(uid_a)));
}

static void test_the_tag_still_there_keeps_the_wait(void) {
    TagWatch w;
    tag_watch_set(&w, uid_a, sizeof(uid_a));
    CHECK(tag_watch_waiting(&w));
    for(int i = 0; i < 10; i++)
        CHECK(tag_watch_poll(&w, uid_a, sizeof(uid_a)));
}

static void test_the_tag_is_gone_after_misses_in_a_row(void) {
    TagWatch w;
    tag_watch_set(&w, uid_a, sizeof(uid_a));
    for(int i = 1; i < TAG_WATCH_MISSES; i++)
        CHECK(tag_watch_poll(&w, NULL, 0));
    CHECK(!tag_watch_poll(&w, NULL, 0));
    CHECK(!tag_watch_waiting(&w));
}

static void test_an_answer_resets_the_misses(void) {
    TagWatch w;
    tag_watch_set(&w, uid_a, sizeof(uid_a));
    for(int i = 1; i < TAG_WATCH_MISSES; i++)
        CHECK(tag_watch_poll(&w, NULL, 0));
    CHECK(tag_watch_poll(&w, uid_a, sizeof(uid_a))); /* a flaky poll, not a removal */
    for(int i = 1; i < TAG_WATCH_MISSES; i++)
        CHECK(tag_watch_poll(&w, NULL, 0));
    CHECK(!tag_watch_poll(&w, NULL, 0));
}

static void test_another_tag_ends_the_wait_at_once(void) {
    TagWatch w;
    tag_watch_set(&w, uid_a, sizeof(uid_a));
    CHECK(!tag_watch_poll(&w, uid_b, sizeof(uid_b)));
    CHECK(!tag_watch_waiting(&w));

    /* same prefix, other length */
    tag_watch_set(&w, uid_b, sizeof(uid_b));
    CHECK(!tag_watch_poll(&w, uid_b, 4));
}

static void test_setting_starts_a_fresh_wait(void) {
    TagWatch w;
    tag_watch_set(&w, uid_a, sizeof(uid_a));
    CHECK(tag_watch_poll(&w, NULL, 0));
    tag_watch_set(&w, uid_a, sizeof(uid_a));
    for(int i = 1; i < TAG_WATCH_MISSES; i++)
        CHECK(tag_watch_poll(&w, NULL, 0));
    CHECK(!tag_watch_poll(&w, NULL, 0));
}

static void test_an_invalid_uid_waits_for_nothing(void) {
    TagWatch w;
    tag_watch_set(&w, uid_a, 0);
    CHECK(!tag_watch_waiting(&w));
    uint8_t long_uid[TAG_DUMP_MAX_UID + 1] = {0};
    tag_watch_set(&w, long_uid, sizeof(long_uid));
    CHECK(!tag_watch_waiting(&w));
}

int main(void) {
    RUN_TEST(test_nothing_to_wait_for_at_first);
    RUN_TEST(test_the_tag_still_there_keeps_the_wait);
    RUN_TEST(test_the_tag_is_gone_after_misses_in_a_row);
    RUN_TEST(test_an_answer_resets_the_misses);
    RUN_TEST(test_another_tag_ends_the_wait_at_once);
    RUN_TEST(test_setting_starts_a_fresh_wait);
    RUN_TEST(test_an_invalid_uid_waits_for_nothing);
    return MINITEST_RESULT();
}
