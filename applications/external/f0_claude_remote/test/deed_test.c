#include "deed.h"
#include "test.h"

static void test_voice_start_earns_a_dictation(void) {
    assert(deed_earned(ActionVoiceStart) == DeedDictation);
}

static void test_clear_earns_a_clear(void) {
    assert(deed_earned(ActionClear) == DeedClear);
}

static void test_the_rest_earn_nothing(void) {
    assert(deed_earned(ActionNone) == DeedNone);
    assert(deed_earned(ActionEnter) == DeedNone);
    assert(deed_earned(ActionArrowUp) == DeedNone);
    assert(deed_earned(ActionArrowDown) == DeedNone);
    assert(deed_earned(ActionEsc) == DeedNone);
    assert(deed_earned(ActionEffortHigh) == DeedNone);
    assert(deed_earned(ActionEffortMax) == DeedNone);
    assert(deed_earned(ActionVoiceStop) == DeedNone);
    assert(deed_earned(ActionExit) == DeedNone);
}

int main(void) {
    test_run(test_voice_start_earns_a_dictation);
    test_run(test_clear_earns_a_clear);
    test_run(test_the_rest_earn_nothing);
    return 0;
}
