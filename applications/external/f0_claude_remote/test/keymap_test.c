#include "keymap.h"
#include "test.h"

static Keymap* keymap_test_alloc_connected(void) {
    Keymap* keymap = keymap_alloc();
    keymap_set_connected(keymap, true);
    return keymap;
}

static Keymap* keymap_test_alloc_voice(void) {
    Keymap* keymap = keymap_test_alloc_connected();
    keymap_handle(keymap, KeymapKeyOk, KeymapEventLong);
    return keymap;
}

static Keymap* keymap_test_alloc_voice_cut(void) {
    Keymap* keymap = keymap_test_alloc_voice();
    keymap_set_connected(keymap, false);
    return keymap;
}

static Action keymap_test_fresh(KeymapKey key, KeymapEvent event) {
    Keymap* keymap = keymap_test_alloc_connected();
    Action action = keymap_handle(keymap, key, event);
    keymap_free(keymap);
    return action;
}

static void test_tap_acts(void) {
    assert(keymap_test_fresh(KeymapKeyUp, KeymapEventShort) == ActionArrowUp);
    assert(keymap_test_fresh(KeymapKeyDown, KeymapEventShort) == ActionArrowDown);
    assert(keymap_test_fresh(KeymapKeyOk, KeymapEventShort) == ActionEnter);
}

static void test_tap_on_left_or_back_does_nothing(void) {
    assert(keymap_test_fresh(KeymapKeyLeft, KeymapEventShort) == ActionNone);
    assert(keymap_test_fresh(KeymapKeyBack, KeymapEventShort) == ActionNone);
}

static void test_hold_acts(void) {
    assert(keymap_test_fresh(KeymapKeyUp, KeymapEventLong) == ActionEffortHigh);
    assert(keymap_test_fresh(KeymapKeyDown, KeymapEventLong) == ActionEffortMax);
    assert(keymap_test_fresh(KeymapKeyLeft, KeymapEventLong) == ActionClear);
    assert(keymap_test_fresh(KeymapKeyOk, KeymapEventLong) == ActionVoiceStart);
    assert(keymap_test_fresh(KeymapKeyBack, KeymapEventLong) == ActionExit);
}

static void test_right_acts_on_press_alone(void) {
    assert(keymap_test_fresh(KeymapKeyRight, KeymapEventPress) == ActionEsc);
    assert(keymap_test_fresh(KeymapKeyRight, KeymapEventShort) == ActionNone);
    assert(keymap_test_fresh(KeymapKeyRight, KeymapEventLong) == ActionNone);
}

static void test_press_acts_only_on_right(void) {
    for(KeymapKey key = KeymapKeyUp; key < KeymapKeyMAX; key++) {
        if(key != KeymapKeyRight) {
            assert(keymap_test_fresh(key, KeymapEventPress) == ActionNone);
        }
    }
}

static void test_repeat_never_acts(void) {
    for(KeymapKey key = KeymapKeyUp; key < KeymapKeyMAX; key++) {
        assert(keymap_test_fresh(key, KeymapEventRepeat) == ActionNone);
    }
}

static void test_release_without_voice_does_nothing(void) {
    for(KeymapKey key = KeymapKeyUp; key < KeymapKeyMAX; key++) {
        assert(keymap_test_fresh(key, KeymapEventRelease) == ActionNone);
    }
}

static void test_hold_ok_turns_voice_on(void) {
    Keymap* keymap = keymap_test_alloc_connected();
    assert(keymap_mode(keymap) == ModeIdle);
    keymap_handle(keymap, KeymapKeyOk, KeymapEventLong);
    assert(keymap_mode(keymap) == ModeVoice);
    keymap_free(keymap);
}

static void test_release_ok_stops_voice_once(void) {
    Keymap* keymap = keymap_test_alloc_voice();
    assert(keymap_handle(keymap, KeymapKeyOk, KeymapEventRelease) == ActionVoiceStop);
    assert(keymap_mode(keymap) == ModeIdle);
    assert(keymap_handle(keymap, KeymapKeyOk, KeymapEventRelease) == ActionNone);
    keymap_free(keymap);
}

static void test_tap_ok_leaves_voice_off(void) {
    Keymap* keymap = keymap_test_alloc_connected();
    keymap_handle(keymap, KeymapKeyOk, KeymapEventPress);
    keymap_handle(keymap, KeymapKeyOk, KeymapEventShort);
    assert(keymap_mode(keymap) == ModeIdle);
    assert(keymap_handle(keymap, KeymapKeyOk, KeymapEventRelease) == ActionNone);
    keymap_free(keymap);
}

static void test_voice_ignores_every_other_event(void) {
    Keymap* keymap = keymap_test_alloc_voice();
    for(KeymapKey key = KeymapKeyUp; key < KeymapKeyMAX; key++) {
        for(KeymapEvent event = KeymapEventPress; event < KeymapEventMAX; event++) {
            if(key != KeymapKeyOk || event != KeymapEventRelease) {
                assert(keymap_handle(keymap, key, event) == ActionNone);
                assert(keymap_mode(keymap) == ModeVoice);
            }
        }
    }
    keymap_free(keymap);
}

static void test_buttons_act_again_after_voice(void) {
    Keymap* keymap = keymap_test_alloc_voice();
    keymap_handle(keymap, KeymapKeyOk, KeymapEventRelease);
    assert(keymap_handle(keymap, KeymapKeyBack, KeymapEventLong) == ActionExit);
    keymap_free(keymap);
}

static void test_fresh_keymap_has_no_link(void) {
    Keymap* keymap = keymap_alloc();
    assert(keymap_mode(keymap) == ModeNoLink);
    keymap_free(keymap);
}

static void test_link_up_ends_no_link(void) {
    Keymap* keymap = keymap_alloc();
    keymap_set_connected(keymap, true);
    assert(keymap_mode(keymap) == ModeIdle);
    keymap_free(keymap);
}

static void test_link_down_ends_idle(void) {
    Keymap* keymap = keymap_test_alloc_connected();
    keymap_set_connected(keymap, false);
    assert(keymap_mode(keymap) == ModeNoLink);
    keymap_free(keymap);
}

static void test_link_down_cuts_voice(void) {
    Keymap* keymap = keymap_test_alloc_voice();
    keymap_set_connected(keymap, false);
    assert(keymap_mode(keymap) == ModeVoiceCut);
    keymap_free(keymap);
}

static void test_link_up_ends_voice_cut(void) {
    Keymap* keymap = keymap_test_alloc_voice_cut();
    keymap_set_connected(keymap, true);
    assert(keymap_mode(keymap) == ModeIdle);
    keymap_free(keymap);
}

static void test_repeated_link_up_changes_nothing(void) {
    Keymap* keymap = keymap_test_alloc_connected();
    keymap_set_connected(keymap, true);
    assert(keymap_mode(keymap) == ModeIdle);
    keymap_free(keymap);

    keymap = keymap_test_alloc_voice();
    keymap_set_connected(keymap, true);
    assert(keymap_mode(keymap) == ModeVoice);
    keymap_free(keymap);
}

static void test_repeated_link_down_changes_nothing(void) {
    Keymap* keymap = keymap_alloc();
    keymap_set_connected(keymap, false);
    assert(keymap_mode(keymap) == ModeNoLink);
    keymap_free(keymap);

    keymap = keymap_test_alloc_voice_cut();
    keymap_set_connected(keymap, false);
    assert(keymap_mode(keymap) == ModeVoiceCut);
    keymap_free(keymap);
}

static void test_no_link_acts_only_on_held_back(void) {
    Keymap* keymap = keymap_alloc();
    for(KeymapKey key = KeymapKeyUp; key < KeymapKeyMAX; key++) {
        for(KeymapEvent event = KeymapEventPress; event < KeymapEventMAX; event++) {
            Action expected = key == KeymapKeyBack && event == KeymapEventLong ? ActionExit :
                                                                                 ActionNone;
            assert(keymap_handle(keymap, key, event) == expected);
            assert(keymap_mode(keymap) == ModeNoLink);
        }
    }
    keymap_free(keymap);
}

static void test_voice_cut_acts_only_on_held_back(void) {
    Keymap* keymap = keymap_test_alloc_voice_cut();
    for(KeymapKey key = KeymapKeyUp; key < KeymapKeyMAX; key++) {
        for(KeymapEvent event = KeymapEventPress; event < KeymapEventMAX; event++) {
            if(key != KeymapKeyOk || event != KeymapEventRelease) {
                Action expected = key == KeymapKeyBack && event == KeymapEventLong ? ActionExit :
                                                                                     ActionNone;
                assert(keymap_handle(keymap, key, event) == expected);
                assert(keymap_mode(keymap) == ModeVoiceCut);
            }
        }
    }
    keymap_free(keymap);
}

static void test_release_ok_after_cut_stops_nothing(void) {
    Keymap* keymap = keymap_test_alloc_voice_cut();
    assert(keymap_handle(keymap, KeymapKeyOk, KeymapEventRelease) == ActionNone);
    assert(keymap_mode(keymap) == ModeNoLink);
    keymap_free(keymap);
}

static void test_release_ok_after_link_returned_stops_nothing(void) {
    Keymap* keymap = keymap_test_alloc_voice_cut();
    keymap_set_connected(keymap, true);
    assert(keymap_handle(keymap, KeymapKeyOk, KeymapEventRelease) == ActionNone);
    assert(keymap_mode(keymap) == ModeIdle);
    keymap_free(keymap);
}

static void test_hold_ok_after_cut_starts_voice_again(void) {
    Keymap* keymap = keymap_test_alloc_voice_cut();
    keymap_handle(keymap, KeymapKeyOk, KeymapEventRelease);
    keymap_set_connected(keymap, true);
    assert(keymap_handle(keymap, KeymapKeyOk, KeymapEventLong) == ActionVoiceStart);
    assert(keymap_mode(keymap) == ModeVoice);
    keymap_free(keymap);
}

static void test_buttons_act_after_link_returns(void) {
    Keymap* keymap = keymap_alloc();
    keymap_handle(keymap, KeymapKeyUp, KeymapEventShort);
    keymap_set_connected(keymap, true);
    assert(keymap_handle(keymap, KeymapKeyUp, KeymapEventShort) == ActionArrowUp);
    keymap_free(keymap);
}

static void test_cut_answers_voice_stop_once(void) {
    Keymap* keymap = keymap_test_alloc_voice();
    assert(keymap_set_connected(keymap, false) == ActionVoiceStop);
    assert(keymap_set_connected(keymap, false) == ActionNone);
    keymap_free(keymap);
}

static void test_link_changes_without_voice_answer_none(void) {
    Keymap* keymap = keymap_alloc();
    assert(keymap_set_connected(keymap, false) == ActionNone);
    assert(keymap_set_connected(keymap, true) == ActionNone);
    assert(keymap_set_connected(keymap, true) == ActionNone);
    assert(keymap_set_connected(keymap, false) == ActionNone);
    keymap_free(keymap);
}

static void test_link_up_during_voice_answers_none(void) {
    Keymap* keymap = keymap_test_alloc_voice();
    assert(keymap_set_connected(keymap, true) == ActionNone);
    keymap_free(keymap);
}

static void test_link_up_after_cut_answers_none(void) {
    Keymap* keymap = keymap_test_alloc_voice_cut();
    assert(keymap_set_connected(keymap, true) == ActionNone);
    keymap_free(keymap);
}

int main(void) {
    test_run(test_tap_acts);
    test_run(test_tap_on_left_or_back_does_nothing);
    test_run(test_hold_acts);
    test_run(test_right_acts_on_press_alone);
    test_run(test_press_acts_only_on_right);
    test_run(test_repeat_never_acts);
    test_run(test_release_without_voice_does_nothing);
    test_run(test_hold_ok_turns_voice_on);
    test_run(test_release_ok_stops_voice_once);
    test_run(test_tap_ok_leaves_voice_off);
    test_run(test_voice_ignores_every_other_event);
    test_run(test_buttons_act_again_after_voice);
    test_run(test_fresh_keymap_has_no_link);
    test_run(test_link_up_ends_no_link);
    test_run(test_link_down_ends_idle);
    test_run(test_link_down_cuts_voice);
    test_run(test_link_up_ends_voice_cut);
    test_run(test_repeated_link_up_changes_nothing);
    test_run(test_repeated_link_down_changes_nothing);
    test_run(test_no_link_acts_only_on_held_back);
    test_run(test_voice_cut_acts_only_on_held_back);
    test_run(test_release_ok_after_cut_stops_nothing);
    test_run(test_release_ok_after_link_returned_stops_nothing);
    test_run(test_hold_ok_after_cut_starts_voice_again);
    test_run(test_buttons_act_after_link_returns);
    test_run(test_cut_answers_voice_stop_once);
    test_run(test_link_changes_without_voice_answer_none);
    test_run(test_link_up_during_voice_answers_none);
    test_run(test_link_up_after_cut_answers_none);
    return 0;
}
