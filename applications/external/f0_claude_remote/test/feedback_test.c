#include "feedback.h"
#include "test.h"

static void test_sent_actions_vibrate(void) {
    assert(feedback_vibrates(ActionEnter));
    assert(feedback_vibrates(ActionArrowUp));
    assert(feedback_vibrates(ActionArrowDown));
    assert(feedback_vibrates(ActionEsc));
    assert(feedback_vibrates(ActionEffortHigh));
    assert(feedback_vibrates(ActionEffortMax));
    assert(feedback_vibrates(ActionClear));
    assert(feedback_vibrates(ActionVoiceStart));
}

static void test_the_rest_stay_silent(void) {
    assert(!feedback_vibrates(ActionNone));
    assert(!feedback_vibrates(ActionVoiceStop));
    assert(!feedback_vibrates(ActionExit));
    assert(!feedback_vibrates(ActionGiveUp));
    assert(!feedback_vibrates(ActionStartOver));
    assert(!feedback_vibrates(ActionUseUsb));
    assert(!feedback_vibrates(ActionUseBluetooth));
}

static void test_arrows_flash_for_100_ms(void) {
    assert(feedback_flash_ms(ActionArrowUp) == 100);
    assert(feedback_flash_ms(ActionArrowDown) == 100);
}

static void test_enter_and_commands_flash_for_400_ms(void) {
    assert(feedback_flash_ms(ActionEnter) == 400);
    assert(feedback_flash_ms(ActionEsc) == 400);
    assert(feedback_flash_ms(ActionEffortHigh) == 400);
    assert(feedback_flash_ms(ActionEffortMax) == 400);
    assert(feedback_flash_ms(ActionClear) == 400);
}

static void test_the_rest_do_not_flash(void) {
    assert(feedback_flash_ms(ActionNone) == 0);
    assert(feedback_flash_ms(ActionVoiceStart) == 0);
    assert(feedback_flash_ms(ActionVoiceStop) == 0);
    assert(feedback_flash_ms(ActionExit) == 0);
    assert(feedback_flash_ms(ActionGiveUp) == 0);
    assert(feedback_flash_ms(ActionStartOver) == 0);
    assert(feedback_flash_ms(ActionUseUsb) == 0);
    assert(feedback_flash_ms(ActionUseBluetooth) == 0);
}

int main(void) {
    test_run(test_sent_actions_vibrate);
    test_run(test_the_rest_stay_silent);
    test_run(test_arrows_flash_for_100_ms);
    test_run(test_enter_and_commands_flash_for_400_ms);
    test_run(test_the_rest_do_not_flash);
    return 0;
}
