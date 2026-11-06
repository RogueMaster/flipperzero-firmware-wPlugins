#include "minitest.h"

#include "feedback.h"

static AppSettings settings(AppFeedback feedback, bool sound) {
    AppSettings st;
    app_settings_defaults(&st);
    app_settings_set_feedback(&st, feedback);
    app_settings_set_sound(&st, sound);
    return st;
}

static void check_plan(FeedbackPlan p, FeedbackLed led, FeedbackVibro vibro, FeedbackSound sound) {
    CHECK_EQ_INT(p.led, led);
    CHECK_EQ_INT(p.vibro, vibro);
    CHECK_EQ_INT(p.sound, sound);
}

static void test_vibro_and_led_by_default(void) {
    AppSettings st = settings(AppFeedbackVibroLed, false);
    check_plan(
        feedback_plan(AppCueConnected, &st),
        FeedbackLedBlue,
        FeedbackVibroShort,
        FeedbackSoundNone);
    check_plan(
        feedback_plan(AppCueRead, &st), FeedbackLedBlue, FeedbackVibroShort, FeedbackSoundNone);
    check_plan(
        feedback_plan(AppCueSuccess, &st), FeedbackLedGreen, FeedbackVibroShort, FeedbackSoundNone);
    check_plan(
        feedback_plan(AppCueError, &st), FeedbackLedRed, FeedbackVibroLong, FeedbackSoundNone);
}

static void test_no_cue_does_nothing(void) {
    AppSettings st = settings(AppFeedbackVibroLed, true);
    FeedbackPlan p = feedback_plan(AppCueNone, &st);
    check_plan(p, FeedbackLedNone, FeedbackVibroNone, FeedbackSoundNone);
    CHECK(feedback_plan_empty(p));
}

static void test_led_only_drops_the_vibro(void) {
    AppSettings st = settings(AppFeedbackLed, false);
    check_plan(
        feedback_plan(AppCueRead, &st), FeedbackLedBlue, FeedbackVibroNone, FeedbackSoundNone);
    check_plan(
        feedback_plan(AppCueError, &st), FeedbackLedRed, FeedbackVibroNone, FeedbackSoundNone);
}

static void test_off_drops_led_and_vibro(void) {
    AppSettings st = settings(AppFeedbackOff, false);
    CHECK(feedback_plan_empty(feedback_plan(AppCueConnected, &st)));
    CHECK(feedback_plan_empty(feedback_plan(AppCueSuccess, &st)));
    CHECK(feedback_plan_empty(feedback_plan(AppCueError, &st)));
}

static void test_sound_beeps_on_success_and_error_only(void) {
    AppSettings st = settings(AppFeedbackVibroLed, true);
    check_plan(
        feedback_plan(AppCueSuccess, &st),
        FeedbackLedGreen,
        FeedbackVibroShort,
        FeedbackSoundSuccess);
    check_plan(
        feedback_plan(AppCueError, &st), FeedbackLedRed, FeedbackVibroLong, FeedbackSoundError);
    CHECK_EQ_INT(feedback_plan(AppCueRead, &st).sound, FeedbackSoundNone);
    CHECK_EQ_INT(feedback_plan(AppCueConnected, &st).sound, FeedbackSoundNone);
}

static void test_sound_plays_with_feedback_off(void) {
    AppSettings st = settings(AppFeedbackOff, true);
    FeedbackPlan p = feedback_plan(AppCueSuccess, &st);
    check_plan(p, FeedbackLedNone, FeedbackVibroNone, FeedbackSoundSuccess);
    CHECK(!feedback_plan_empty(p));
    CHECK(feedback_plan_empty(feedback_plan(AppCueRead, &st)));
}

int main(void) {
    RUN_TEST(test_vibro_and_led_by_default);
    RUN_TEST(test_no_cue_does_nothing);
    RUN_TEST(test_led_only_drops_the_vibro);
    RUN_TEST(test_off_drops_led_and_vibro);
    RUN_TEST(test_sound_beeps_on_success_and_error_only);
    RUN_TEST(test_sound_plays_with_feedback_off);
    return MINITEST_RESULT();
}
