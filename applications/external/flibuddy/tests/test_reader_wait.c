#include "minitest.h"

#include "reader_wait.h"

/* The SDK's sync pollers wait on and clear bit 0 of the calling thread. */
static void test_reader_flags_stay_off_bit_0(void) {
    CHECK((READER_FLAG_STOP & 1U) == 0);
    CHECK((READER_FLAG_DETECTED & 1U) == 0);
    CHECK((READER_FLAG_STOP & READER_FLAG_DETECTED) == 0);
    CHECK((READER_FLAG_STOP & READER_FLAG_ERROR_BIT) == 0);
    CHECK((READER_FLAG_DETECTED & READER_FLAG_ERROR_BIT) == 0);
}

static void test_stop_flag_stops(void) {
    CHECK(reader_wait_stop(READER_FLAG_STOP, false));
    CHECK(reader_wait_stop(READER_FLAG_STOP | READER_FLAG_DETECTED, false));
}

static void test_stopping_wins_without_a_flag(void) {
    /* the flag was swallowed or never delivered */
    CHECK(reader_wait_stop(0, true));
    CHECK(reader_wait_stop(READER_FLAG_ERROR_BIT | 0xFEU, true)); /* timeout */
}

static void test_timeout_and_other_flags_do_not_stop(void) {
    CHECK(!reader_wait_stop(0xFFFFFFFEU, false)); /* FuriFlagErrorTimeout */
    CHECK(!reader_wait_stop(READER_FLAG_DETECTED, false));
    CHECK(!reader_wait_stop(1U, false)); /* bit 0 is not ours */
    CHECK(!reader_wait_stop(0, false));
}

static void test_error_status_with_stop_bit_is_not_a_stop(void) {
    CHECK(!reader_wait_stop(READER_FLAG_ERROR_BIT | READER_FLAG_STOP, false));
}

static void test_zeroed_uid_is_no_answer(void) {
    /* an early wake returns success with a zeroed result */
    CHECK(!reader_tag_answered(true, 0));
    CHECK(!reader_tag_answered(false, 4));
    CHECK(reader_tag_answered(true, 4));
    CHECK(reader_tag_answered(true, 7));
}

int main(void) {
    RUN_TEST(test_reader_flags_stay_off_bit_0);
    RUN_TEST(test_stop_flag_stops);
    RUN_TEST(test_stopping_wins_without_a_flag);
    RUN_TEST(test_timeout_and_other_flags_do_not_stop);
    RUN_TEST(test_error_status_with_stop_bit_is_not_a_stop);
    RUN_TEST(test_zeroed_uid_is_no_answer);
    return MINITEST_RESULT();
}
