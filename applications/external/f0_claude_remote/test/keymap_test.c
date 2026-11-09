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

static Keymap* keymap_test_alloc_stopped(void) {
    Keymap* keymap = keymap_alloc();
    keymap_time_out(keymap);
    return keymap;
}

static Keymap* keymap_test_alloc_on_usb(void) {
    Keymap* keymap = keymap_alloc();
    keymap_handle(keymap, KeymapKeyRight, KeymapEventPress);
    keymap_handle(keymap, KeymapKeyRight, KeymapEventLong);
    keymap_handle(keymap, KeymapKeyRight, KeymapEventRelease);
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

static void test_no_link_acts_only_on_held_back_or_right(void) {
    Keymap* keymap = keymap_alloc();
    for(KeymapKey key = KeymapKeyUp; key < KeymapKeyMAX; key++) {
        for(KeymapEvent event = KeymapEventPress; event < KeymapEventMAX; event++) {
            Action expected = ActionNone;
            if(event == KeymapEventLong && key == KeymapKeyBack) {
                expected = ActionExit;
            } else if(event == KeymapEventLong && key == KeymapKeyRight) {
                expected = ActionUseUsb;
            }
            assert(keymap_handle(keymap, key, event) == expected);
            assert(keymap_mode(keymap) == ModeNoLink);
        }
    }
    keymap_free(keymap);
}

static void test_right_held_on_no_link_takes_usb_once(void) {
    Keymap* keymap = keymap_alloc();
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventPress) == ActionNone);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventLong) == ActionUseUsb);
    assert(keymap_mode(keymap) == ModeNoLink);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventLong) == ActionNone);
    keymap_free(keymap);
}

static void test_right_held_again_switches_back_and_forth(void) {
    Keymap* keymap = keymap_alloc();
    keymap_handle(keymap, KeymapKeyRight, KeymapEventPress);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventLong) == ActionUseUsb);
    keymap_handle(keymap, KeymapKeyRight, KeymapEventRelease);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventPress) == ActionNone);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventLong) == ActionUseBluetooth);
    assert(keymap_mode(keymap) == ModeNoLink);
    keymap_handle(keymap, KeymapKeyRight, KeymapEventRelease);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventPress) == ActionNone);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventLong) == ActionUseUsb);
    assert(keymap_mode(keymap) == ModeNoLink);
    keymap_free(keymap);
}

static void test_right_pressed_on_usb_stopped_returns_nothing(void) {
    Keymap* keymap = keymap_test_alloc_on_usb();
    keymap_time_out(keymap);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventPress) == ActionStartOver);
    assert(keymap_mode(keymap) == ModeNoLink);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventLong) == ActionNone);
    keymap_free(keymap);
}

static void test_right_pressed_on_usb_idle_returns_nothing(void) {
    Keymap* keymap = keymap_test_alloc_on_usb();
    keymap_set_connected(keymap, true);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventPress) == ActionEsc);
    keymap_set_connected(keymap, false);
    assert(keymap_mode(keymap) == ModeNoLink);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventLong) == ActionNone);
    keymap_free(keymap);
}

static void test_right_held_elsewhere_on_usb_returns_nothing(void) {
    Keymap* idle = keymap_test_alloc_on_usb();
    keymap_set_connected(idle, true);
    keymap_handle(idle, KeymapKeyRight, KeymapEventPress);
    assert(keymap_handle(idle, KeymapKeyRight, KeymapEventLong) == ActionNone);
    keymap_free(idle);

    Keymap* voice = keymap_test_alloc_on_usb();
    keymap_set_connected(voice, true);
    keymap_handle(voice, KeymapKeyOk, KeymapEventLong);
    keymap_handle(voice, KeymapKeyRight, KeymapEventPress);
    assert(keymap_handle(voice, KeymapKeyRight, KeymapEventLong) == ActionNone);
    keymap_set_connected(voice, false);
    assert(keymap_mode(voice) == ModeVoiceCut);
    keymap_handle(voice, KeymapKeyRight, KeymapEventPress);
    assert(keymap_handle(voice, KeymapKeyRight, KeymapEventLong) == ActionNone);
    keymap_free(voice);
}

static void test_right_tapped_on_usb_no_link_does_nothing(void) {
    Keymap* keymap = keymap_test_alloc_on_usb();
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventPress) == ActionNone);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventShort) == ActionNone);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventRelease) == ActionNone);
    assert(keymap_mode(keymap) == ModeNoLink);
    keymap_free(keymap);
}

static void test_right_tapped_on_no_link_does_nothing(void) {
    Keymap* keymap = keymap_alloc();
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventPress) == ActionNone);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventShort) == ActionNone);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventRelease) == ActionNone);
    assert(keymap_mode(keymap) == ModeNoLink);
    keymap_free(keymap);
}

static void test_right_long_with_no_press_does_nothing(void) {
    Keymap* keymap = keymap_alloc();
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventLong) == ActionNone);
    keymap_free(keymap);
}

static void test_right_pressed_on_stopped_takes_no_usb(void) {
    Keymap* keymap = keymap_test_alloc_stopped();
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventPress) == ActionStartOver);
    assert(keymap_mode(keymap) == ModeNoLink);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventLong) == ActionNone);
    keymap_free(keymap);
}

static void test_right_pressed_on_idle_takes_no_usb(void) {
    Keymap* keymap = keymap_test_alloc_connected();
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventPress) == ActionEsc);
    keymap_set_connected(keymap, false);
    assert(keymap_mode(keymap) == ModeNoLink);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventLong) == ActionNone);
    keymap_free(keymap);
}

static void test_right_held_elsewhere_takes_no_usb(void) {
    Keymap* keymaps[] = {
        keymap_test_alloc_connected(),
        keymap_test_alloc_voice(),
        keymap_test_alloc_voice_cut(),
    };
    for(size_t i = 0; i < sizeof(keymaps) / sizeof(keymaps[0]); i++) {
        keymap_handle(keymaps[i], KeymapKeyRight, KeymapEventPress);
        assert(keymap_handle(keymaps[i], KeymapKeyRight, KeymapEventLong) == ActionNone);
        keymap_free(keymaps[i]);
    }
}

static void test_wait_goes_on_after_usb_is_taken(void) {
    Keymap* keymap = keymap_alloc();
    keymap_handle(keymap, KeymapKeyRight, KeymapEventPress);
    keymap_handle(keymap, KeymapKeyRight, KeymapEventLong);
    assert(keymap_wait_ms(keymap) == 60000);
    assert(keymap_time_out(keymap) == ActionGiveUp);
    assert(keymap_mode(keymap) == ModeStopped);
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

static void test_wait_ends_no_link_with_give_up(void) {
    Keymap* keymap = keymap_alloc();
    assert(keymap_time_out(keymap) == ActionGiveUp);
    assert(keymap_mode(keymap) == ModeStopped);
    keymap_free(keymap);
}

static void test_wait_ends_voice_cut_with_give_up(void) {
    Keymap* keymap = keymap_test_alloc_voice_cut();
    assert(keymap_time_out(keymap) == ActionGiveUp);
    assert(keymap_mode(keymap) == ModeStopped);
    keymap_free(keymap);
}

static void test_wait_end_with_a_link_does_nothing(void) {
    Keymap* keymap = keymap_test_alloc_connected();
    assert(keymap_time_out(keymap) == ActionNone);
    assert(keymap_mode(keymap) == ModeIdle);
    keymap_free(keymap);

    keymap = keymap_test_alloc_voice();
    assert(keymap_time_out(keymap) == ActionNone);
    assert(keymap_mode(keymap) == ModeVoice);
    keymap_free(keymap);
}

static void test_wait_end_on_stopped_does_nothing(void) {
    Keymap* keymap = keymap_test_alloc_stopped();
    assert(keymap_time_out(keymap) == ActionNone);
    assert(keymap_mode(keymap) == ModeStopped);
    keymap_free(keymap);
}

static void test_stopped_back_press_exits(void) {
    Keymap* keymap = keymap_test_alloc_stopped();
    assert(keymap_handle(keymap, KeymapKeyBack, KeymapEventPress) == ActionExit);
    keymap_free(keymap);
}

static void test_stopped_back_hold_exits(void) {
    Keymap* keymap = keymap_test_alloc_stopped();
    assert(keymap_handle(keymap, KeymapKeyBack, KeymapEventLong) == ActionExit);
    keymap_free(keymap);
}

static void test_stopped_any_other_press_starts_over(void) {
    for(KeymapKey key = KeymapKeyUp; key < KeymapKeyMAX; key++) {
        if(key != KeymapKeyBack) {
            Keymap* keymap = keymap_test_alloc_stopped();
            assert(keymap_handle(keymap, key, KeymapEventPress) == ActionStartOver);
            assert(keymap_mode(keymap) == ModeNoLink);
            keymap_free(keymap);
        }
    }
}

static void test_stopped_ignores_every_other_event(void) {
    Keymap* keymap = keymap_test_alloc_stopped();
    for(KeymapKey key = KeymapKeyUp; key < KeymapKeyMAX; key++) {
        for(KeymapEvent event = KeymapEventPress; event < KeymapEventMAX; event++) {
            bool back_held = key == KeymapKeyBack && event == KeymapEventLong;
            if(event != KeymapEventPress && !back_held) {
                assert(keymap_handle(keymap, key, event) == ActionNone);
                assert(keymap_mode(keymap) == ModeStopped);
            }
        }
    }
    keymap_free(keymap);
}

static void test_stopped_ignores_link_reports(void) {
    Keymap* keymap = keymap_test_alloc_stopped();
    assert(keymap_set_connected(keymap, true) == ActionNone);
    assert(keymap_mode(keymap) == ModeStopped);
    assert(keymap_set_connected(keymap, false) == ActionNone);
    assert(keymap_mode(keymap) == ModeStopped);
    keymap_free(keymap);
}

static void test_ok_released_after_stopping_stays_stopped(void) {
    Keymap* keymap = keymap_test_alloc_voice_cut();
    keymap_time_out(keymap);
    assert(keymap_handle(keymap, KeymapKeyOk, KeymapEventRelease) == ActionNone);
    assert(keymap_mode(keymap) == ModeStopped);
    keymap_free(keymap);
}

static void test_ok_held_on_stopped_starts_no_voice(void) {
    Keymap* keymap = keymap_test_alloc_stopped();
    keymap_handle(keymap, KeymapKeyOk, KeymapEventPress);
    assert(keymap_handle(keymap, KeymapKeyOk, KeymapEventLong) == ActionNone);
    assert(keymap_mode(keymap) == ModeNoLink);
    keymap_free(keymap);
}

static void test_starting_over_waits_again(void) {
    Keymap* keymap = keymap_test_alloc_stopped();
    keymap_handle(keymap, KeymapKeyUp, KeymapEventPress);
    assert(keymap_wait_ms(keymap) == 60000);
    assert(keymap_time_out(keymap) == ActionGiveUp);
    assert(keymap_mode(keymap) == ModeStopped);
    keymap_free(keymap);
}

static void test_wait_runs_only_while_offering_with_no_link(void) {
    Keymap* keymap = keymap_alloc();
    assert(keymap_wait_ms(keymap) == 60000);
    keymap_free(keymap);

    keymap = keymap_test_alloc_connected();
    assert(keymap_wait_ms(keymap) == 0);
    keymap_free(keymap);

    keymap = keymap_test_alloc_voice();
    assert(keymap_wait_ms(keymap) == 0);
    keymap_free(keymap);

    keymap = keymap_test_alloc_voice_cut();
    assert(keymap_wait_ms(keymap) == 60000);
    keymap_free(keymap);

    keymap = keymap_test_alloc_stopped();
    assert(keymap_wait_ms(keymap) == 0);
    keymap_free(keymap);
}

static void test_press_during_wait_changes_nothing(void) {
    Keymap* keymap = keymap_alloc();
    assert(keymap_handle(keymap, KeymapKeyUp, KeymapEventPress) == ActionNone);
    assert(keymap_wait_ms(keymap) == 60000);
    assert(keymap_time_out(keymap) == ActionGiveUp);
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
    test_run(test_no_link_acts_only_on_held_back_or_right);
    test_run(test_right_held_on_no_link_takes_usb_once);
    test_run(test_right_held_again_switches_back_and_forth);
    test_run(test_right_pressed_on_usb_stopped_returns_nothing);
    test_run(test_right_pressed_on_usb_idle_returns_nothing);
    test_run(test_right_held_elsewhere_on_usb_returns_nothing);
    test_run(test_right_tapped_on_usb_no_link_does_nothing);
    test_run(test_right_tapped_on_no_link_does_nothing);
    test_run(test_right_long_with_no_press_does_nothing);
    test_run(test_right_pressed_on_stopped_takes_no_usb);
    test_run(test_right_pressed_on_idle_takes_no_usb);
    test_run(test_right_held_elsewhere_takes_no_usb);
    test_run(test_wait_goes_on_after_usb_is_taken);
    test_run(test_voice_cut_acts_only_on_held_back);
    test_run(test_release_ok_after_cut_stops_nothing);
    test_run(test_release_ok_after_link_returned_stops_nothing);
    test_run(test_hold_ok_after_cut_starts_voice_again);
    test_run(test_buttons_act_after_link_returns);
    test_run(test_cut_answers_voice_stop_once);
    test_run(test_link_changes_without_voice_answer_none);
    test_run(test_link_up_during_voice_answers_none);
    test_run(test_link_up_after_cut_answers_none);
    test_run(test_wait_ends_no_link_with_give_up);
    test_run(test_wait_ends_voice_cut_with_give_up);
    test_run(test_wait_end_with_a_link_does_nothing);
    test_run(test_wait_end_on_stopped_does_nothing);
    test_run(test_stopped_back_press_exits);
    test_run(test_stopped_back_hold_exits);
    test_run(test_stopped_any_other_press_starts_over);
    test_run(test_stopped_ignores_every_other_event);
    test_run(test_stopped_ignores_link_reports);
    test_run(test_ok_released_after_stopping_stays_stopped);
    test_run(test_ok_held_on_stopped_starts_no_voice);
    test_run(test_starting_over_waits_again);
    test_run(test_wait_runs_only_while_offering_with_no_link);
    test_run(test_press_during_wait_changes_nothing);
    return 0;
}
