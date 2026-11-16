#include "keymap.h"
#include "test.h"

static Keymap* keymap_test_alloc_connected(void) {
    Keymap* keymap = keymap_alloc();
    keymap_set_connected(keymap, true);
    return keymap;
}

static Keymap* keymap_test_alloc_voice(void) {
    Keymap* keymap = keymap_test_alloc_connected();
    keymap_handle(keymap, KeymapKeyOk, KeymapEventPress);
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

static Action keymap_test_tap(Keymap* keymap, KeymapKey key) {
    keymap_handle(keymap, key, KeymapEventPress);
    Action action = keymap_handle(keymap, key, KeymapEventShort);
    keymap_handle(keymap, key, KeymapEventRelease);
    return action;
}

static Keymap* keymap_test_alloc_options(void) {
    Keymap* keymap = keymap_test_alloc_connected();
    keymap_test_tap(keymap, KeymapKeyBack);
    return keymap;
}

static Keymap* keymap_test_alloc_options_no_link(void) {
    Keymap* keymap = keymap_test_alloc_options();
    keymap_set_connected(keymap, false);
    return keymap;
}

static Action keymap_test_fresh(KeymapKey key, KeymapEvent event) {
    Keymap* keymap = keymap_test_alloc_connected();
    if(event == KeymapEventShort || event == KeymapEventLong) {
        keymap_handle(keymap, key, KeymapEventPress);
    }
    Action action = keymap_handle(keymap, key, event);
    keymap_free(keymap);
    return action;
}

static void test_tap_acts(void) {
    assert(keymap_test_fresh(KeymapKeyUp, KeymapEventShort) == ActionArrowUp);
    assert(keymap_test_fresh(KeymapKeyDown, KeymapEventShort) == ActionArrowDown);
    assert(keymap_test_fresh(KeymapKeyLeft, KeymapEventShort) == ActionArrowLeft);
    assert(keymap_test_fresh(KeymapKeyRight, KeymapEventShort) == ActionArrowRight);
    assert(keymap_test_fresh(KeymapKeyOk, KeymapEventShort) == ActionEnter);
}

static void test_tap_on_back_opens_the_options(void) {
    Keymap* keymap = keymap_test_alloc_connected();
    assert(keymap_handle(keymap, KeymapKeyBack, KeymapEventPress) == ActionNone);
    assert(keymap_handle(keymap, KeymapKeyBack, KeymapEventShort) == ActionNone);
    assert(keymap_mode(keymap) == ModeOptions);
    assert(keymap_handle(keymap, KeymapKeyBack, KeymapEventRelease) == ActionNone);
    assert(keymap_mode(keymap) == ModeOptions);
    keymap_free(keymap);
}

static void test_hold_acts(void) {
    assert(keymap_test_fresh(KeymapKeyUp, KeymapEventLong) == ActionEffortHigh);
    assert(keymap_test_fresh(KeymapKeyDown, KeymapEventLong) == ActionEffortMax);
    assert(keymap_test_fresh(KeymapKeyLeft, KeymapEventLong) == ActionClear);
    assert(keymap_test_fresh(KeymapKeyRight, KeymapEventLong) == ActionEsc);
    assert(keymap_test_fresh(KeymapKeyOk, KeymapEventLong) == ActionVoiceStart);
    assert(keymap_test_fresh(KeymapKeyBack, KeymapEventLong) == ActionExit);
}

static void test_press_never_acts(void) {
    for(KeymapKey key = KeymapKeyUp; key < KeymapKeyMAX; key++) {
        assert(keymap_test_fresh(key, KeymapEventPress) == ActionNone);
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
    assert(keymap_mode(keymap) == ModeMain);
    keymap_handle(keymap, KeymapKeyOk, KeymapEventPress);
    keymap_handle(keymap, KeymapKeyOk, KeymapEventLong);
    assert(keymap_mode(keymap) == ModeVoice);
    keymap_free(keymap);
}

static void test_release_ok_stops_voice_once(void) {
    Keymap* keymap = keymap_test_alloc_voice();
    assert(keymap_handle(keymap, KeymapKeyOk, KeymapEventRelease) == ActionVoiceStop);
    assert(keymap_mode(keymap) == ModeMain);
    assert(keymap_handle(keymap, KeymapKeyOk, KeymapEventRelease) == ActionNone);
    keymap_free(keymap);
}

static void test_tap_ok_leaves_voice_off(void) {
    Keymap* keymap = keymap_test_alloc_connected();
    keymap_handle(keymap, KeymapKeyOk, KeymapEventPress);
    keymap_handle(keymap, KeymapKeyOk, KeymapEventShort);
    assert(keymap_mode(keymap) == ModeMain);
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

static void test_fresh_keymap_is_on_bluetooth(void) {
    Keymap* keymap = keymap_alloc();
    assert(keymap_link(keymap) == LinkBluetooth);
    keymap_free(keymap);
}

static void test_link_up_ends_no_link(void) {
    Keymap* keymap = keymap_alloc();
    keymap_set_connected(keymap, true);
    assert(keymap_mode(keymap) == ModeMain);
    keymap_free(keymap);
}

static void test_link_down_ends_main(void) {
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
    assert(keymap_mode(keymap) == ModeMain);
    keymap_free(keymap);
}

static void test_repeated_link_up_changes_nothing(void) {
    Keymap* keymap = keymap_test_alloc_connected();
    keymap_set_connected(keymap, true);
    assert(keymap_mode(keymap) == ModeMain);
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
            bool right_held = key == KeymapKeyRight && event == KeymapEventLong;
            Action expected = key == KeymapKeyBack && event == KeymapEventLong ? ActionExit :
                                                                                 ActionNone;
            Link link = keymap_link(keymap);
            assert(keymap_handle(keymap, key, event) == expected);
            assert(keymap_mode(keymap) == ModeNoLink);
            assert((keymap_link(keymap) != link) == right_held);
        }
    }
    keymap_free(keymap);
}

static void test_right_held_on_no_link_takes_usb_once(void) {
    Keymap* keymap = keymap_alloc();
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventPress) == ActionNone);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventLong) == ActionNone);
    assert(keymap_mode(keymap) == ModeNoLink);
    assert(keymap_link(keymap) == LinkUsb);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventLong) == ActionNone);
    assert(keymap_link(keymap) == LinkUsb);
    keymap_free(keymap);
}

static void test_right_held_again_switches_back_and_forth(void) {
    Keymap* keymap = keymap_alloc();
    keymap_handle(keymap, KeymapKeyRight, KeymapEventPress);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventLong) == ActionNone);
    assert(keymap_link(keymap) == LinkUsb);
    keymap_handle(keymap, KeymapKeyRight, KeymapEventRelease);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventPress) == ActionNone);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventLong) == ActionNone);
    assert(keymap_mode(keymap) == ModeNoLink);
    assert(keymap_link(keymap) == LinkBluetooth);
    keymap_handle(keymap, KeymapKeyRight, KeymapEventRelease);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventPress) == ActionNone);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventLong) == ActionNone);
    assert(keymap_mode(keymap) == ModeNoLink);
    assert(keymap_link(keymap) == LinkUsb);
    keymap_free(keymap);
}

static void test_link_stays_through_every_mode(void) {
    Keymap* keymap = keymap_test_alloc_on_usb();
    keymap_set_connected(keymap, true);
    assert(keymap_mode(keymap) == ModeMain);
    assert(keymap_link(keymap) == LinkUsb);
    keymap_handle(keymap, KeymapKeyOk, KeymapEventPress);
    keymap_handle(keymap, KeymapKeyOk, KeymapEventLong);
    assert(keymap_mode(keymap) == ModeVoice);
    assert(keymap_link(keymap) == LinkUsb);
    keymap_set_connected(keymap, false);
    assert(keymap_mode(keymap) == ModeVoiceCut);
    assert(keymap_link(keymap) == LinkUsb);
    keymap_time_out(keymap);
    assert(keymap_mode(keymap) == ModeStopped);
    assert(keymap_link(keymap) == LinkUsb);
    keymap_handle(keymap, KeymapKeyUp, KeymapEventPress);
    assert(keymap_mode(keymap) == ModeNoLink);
    assert(keymap_link(keymap) == LinkUsb);
    keymap_free(keymap);
}

static void test_right_pressed_on_usb_stopped_returns_nothing(void) {
    Keymap* keymap = keymap_test_alloc_on_usb();
    keymap_time_out(keymap);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventPress) == ActionStartOver);
    assert(keymap_mode(keymap) == ModeNoLink);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventLong) == ActionNone);
    assert(keymap_link(keymap) == LinkUsb);
    keymap_free(keymap);
}

static void test_right_pressed_on_usb_main_returns_nothing(void) {
    Keymap* keymap = keymap_test_alloc_on_usb();
    keymap_set_connected(keymap, true);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventPress) == ActionNone);
    keymap_set_connected(keymap, false);
    assert(keymap_mode(keymap) == ModeNoLink);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventLong) == ActionNone);
    assert(keymap_link(keymap) == LinkUsb);
    keymap_free(keymap);
}

static void test_right_held_elsewhere_on_usb_keeps_usb(void) {
    Keymap* main_screen = keymap_test_alloc_on_usb();
    keymap_set_connected(main_screen, true);
    keymap_handle(main_screen, KeymapKeyRight, KeymapEventPress);
    assert(keymap_handle(main_screen, KeymapKeyRight, KeymapEventLong) == ActionEsc);
    assert(keymap_link(main_screen) == LinkUsb);
    keymap_free(main_screen);

    Keymap* voice = keymap_test_alloc_on_usb();
    keymap_set_connected(voice, true);
    keymap_handle(voice, KeymapKeyOk, KeymapEventPress);
    keymap_handle(voice, KeymapKeyOk, KeymapEventLong);
    keymap_handle(voice, KeymapKeyRight, KeymapEventPress);
    assert(keymap_handle(voice, KeymapKeyRight, KeymapEventLong) == ActionNone);
    assert(keymap_link(voice) == LinkUsb);
    keymap_set_connected(voice, false);
    assert(keymap_mode(voice) == ModeVoiceCut);
    keymap_handle(voice, KeymapKeyRight, KeymapEventPress);
    assert(keymap_handle(voice, KeymapKeyRight, KeymapEventLong) == ActionNone);
    assert(keymap_link(voice) == LinkUsb);
    keymap_free(voice);
}

static void test_right_tapped_on_usb_no_link_does_nothing(void) {
    Keymap* keymap = keymap_test_alloc_on_usb();
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventPress) == ActionNone);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventShort) == ActionNone);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventRelease) == ActionNone);
    assert(keymap_mode(keymap) == ModeNoLink);
    assert(keymap_link(keymap) == LinkUsb);
    keymap_free(keymap);
}

static void test_right_tapped_on_no_link_does_nothing(void) {
    Keymap* keymap = keymap_alloc();
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventPress) == ActionNone);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventShort) == ActionNone);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventRelease) == ActionNone);
    assert(keymap_mode(keymap) == ModeNoLink);
    assert(keymap_link(keymap) == LinkBluetooth);
    keymap_free(keymap);
}

static void test_right_long_with_no_press_does_nothing(void) {
    Keymap* keymap = keymap_alloc();
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventLong) == ActionNone);
    assert(keymap_link(keymap) == LinkBluetooth);
    keymap_free(keymap);
}

static void test_right_pressed_on_stopped_takes_no_usb(void) {
    Keymap* keymap = keymap_test_alloc_stopped();
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventPress) == ActionStartOver);
    assert(keymap_mode(keymap) == ModeNoLink);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventLong) == ActionNone);
    assert(keymap_link(keymap) == LinkBluetooth);
    keymap_free(keymap);
}

static void test_right_pressed_on_main_takes_no_usb(void) {
    Keymap* keymap = keymap_test_alloc_connected();
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventPress) == ActionNone);
    keymap_set_connected(keymap, false);
    assert(keymap_mode(keymap) == ModeNoLink);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventLong) == ActionNone);
    assert(keymap_link(keymap) == LinkBluetooth);
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
        keymap_handle(keymaps[i], KeymapKeyRight, KeymapEventLong);
        assert(keymap_link(keymaps[i]) == LinkBluetooth);
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
    assert(keymap_mode(keymap) == ModeMain);
    keymap_free(keymap);
}

static void test_hold_ok_after_cut_starts_voice_again(void) {
    Keymap* keymap = keymap_test_alloc_voice_cut();
    keymap_handle(keymap, KeymapKeyOk, KeymapEventRelease);
    keymap_set_connected(keymap, true);
    keymap_handle(keymap, KeymapKeyOk, KeymapEventPress);
    assert(keymap_handle(keymap, KeymapKeyOk, KeymapEventLong) == ActionVoiceStart);
    assert(keymap_mode(keymap) == ModeVoice);
    keymap_free(keymap);
}

static void test_buttons_act_after_link_returns(void) {
    Keymap* keymap = keymap_alloc();
    keymap_handle(keymap, KeymapKeyUp, KeymapEventPress);
    keymap_handle(keymap, KeymapKeyUp, KeymapEventShort);
    keymap_set_connected(keymap, true);
    keymap_handle(keymap, KeymapKeyUp, KeymapEventPress);
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
    assert(keymap_mode(keymap) == ModeMain);
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

static void test_opening_starts_the_wait(void) {
    Keymap* keymap = keymap_alloc();
    assert(keymap_wait(keymap) == KeymapWaitStart);
    keymap_free(keymap);
}

static void test_drop_starts_the_wait(void) {
    Keymap* keymap = keymap_test_alloc_connected();
    assert(keymap_wait(keymap) == KeymapWaitStop);
    keymap_set_connected(keymap, false);
    assert(keymap_wait(keymap) == KeymapWaitStart);
    keymap_free(keymap);

    keymap = keymap_test_alloc_voice();
    assert(keymap_wait(keymap) == KeymapWaitKeep);
    keymap_set_connected(keymap, false);
    assert(keymap_wait(keymap) == KeymapWaitStart);
    keymap_free(keymap);
}

static void test_starting_over_starts_the_wait(void) {
    Keymap* keymap = keymap_test_alloc_stopped();
    assert(keymap_wait(keymap) == KeymapWaitStop);
    keymap_handle(keymap, KeymapKeyUp, KeymapEventPress);
    assert(keymap_wait(keymap) == KeymapWaitStart);
    keymap_free(keymap);
}

static void test_each_switch_starts_the_wait_afresh(void) {
    Keymap* keymap = keymap_alloc();
    keymap_handle(keymap, KeymapKeyRight, KeymapEventPress);
    assert(keymap_wait(keymap) == KeymapWaitKeep);
    keymap_handle(keymap, KeymapKeyRight, KeymapEventLong);
    assert(keymap_wait(keymap) == KeymapWaitStart);
    keymap_handle(keymap, KeymapKeyRight, KeymapEventRelease);
    assert(keymap_wait(keymap) == KeymapWaitKeep);
    keymap_handle(keymap, KeymapKeyRight, KeymapEventPress);
    assert(keymap_wait(keymap) == KeymapWaitKeep);
    keymap_handle(keymap, KeymapKeyRight, KeymapEventLong);
    assert(keymap_wait(keymap) == KeymapWaitStart);
    keymap_free(keymap);
}

static void test_link_up_stops_the_wait(void) {
    Keymap* keymap = keymap_alloc();
    assert(keymap_wait(keymap) == KeymapWaitStart);
    keymap_set_connected(keymap, true);
    assert(keymap_wait(keymap) == KeymapWaitStop);
    keymap_free(keymap);

    keymap = keymap_test_alloc_voice_cut();
    assert(keymap_wait(keymap) == KeymapWaitStart);
    keymap_set_connected(keymap, true);
    assert(keymap_wait(keymap) == KeymapWaitStop);
    keymap_free(keymap);
}

static void test_wait_end_stops_the_wait(void) {
    Keymap* keymap = keymap_alloc();
    assert(keymap_wait(keymap) == KeymapWaitStart);
    keymap_time_out(keymap);
    assert(keymap_wait(keymap) == KeymapWaitStop);
    keymap_free(keymap);
}

static void test_presses_with_no_link_keep_the_wait(void) {
    Keymap* keymap = keymap_alloc();
    assert(keymap_wait(keymap) == KeymapWaitStart);
    for(KeymapKey key = KeymapKeyUp; key < KeymapKeyMAX; key++) {
        for(KeymapEvent event = KeymapEventPress; event < KeymapEventMAX; event++) {
            if(key != KeymapKeyRight || event != KeymapEventLong) {
                keymap_handle(keymap, key, event);
                assert(keymap_wait(keymap) == KeymapWaitKeep);
            }
        }
    }
    keymap_free(keymap);
}

static void test_ok_released_after_cut_keeps_the_wait(void) {
    Keymap* keymap = keymap_test_alloc_voice_cut();
    assert(keymap_wait(keymap) == KeymapWaitStart);
    keymap_handle(keymap, KeymapKeyOk, KeymapEventRelease);
    assert(keymap_mode(keymap) == ModeNoLink);
    assert(keymap_wait(keymap) == KeymapWaitKeep);
    keymap_free(keymap);
}

static void test_repeated_link_down_keeps_the_wait(void) {
    Keymap* keymap = keymap_alloc();
    assert(keymap_wait(keymap) == KeymapWaitStart);
    keymap_set_connected(keymap, false);
    assert(keymap_wait(keymap) == KeymapWaitKeep);
    keymap_free(keymap);

    keymap = keymap_test_alloc_voice_cut();
    assert(keymap_wait(keymap) == KeymapWaitStart);
    keymap_set_connected(keymap, false);
    assert(keymap_wait(keymap) == KeymapWaitKeep);
    keymap_free(keymap);
}

static void test_events_with_a_link_keep_the_wait(void) {
    Keymap* keymap = keymap_test_alloc_connected();
    assert(keymap_wait(keymap) == KeymapWaitStop);
    keymap_time_out(keymap);
    assert(keymap_wait(keymap) == KeymapWaitKeep);
    keymap_set_connected(keymap, true);
    assert(keymap_wait(keymap) == KeymapWaitKeep);
    keymap_handle(keymap, KeymapKeyOk, KeymapEventPress);
    keymap_handle(keymap, KeymapKeyOk, KeymapEventLong);
    assert(keymap_wait(keymap) == KeymapWaitKeep);
    keymap_handle(keymap, KeymapKeyOk, KeymapEventRelease);
    assert(keymap_wait(keymap) == KeymapWaitKeep);
    keymap_free(keymap);
}

static void test_events_once_stopped_keep_the_wait(void) {
    Keymap* keymap = keymap_test_alloc_stopped();
    assert(keymap_wait(keymap) == KeymapWaitStop);
    keymap_set_connected(keymap, true);
    assert(keymap_wait(keymap) == KeymapWaitKeep);
    keymap_set_connected(keymap, false);
    assert(keymap_wait(keymap) == KeymapWaitKeep);
    keymap_time_out(keymap);
    assert(keymap_wait(keymap) == KeymapWaitKeep);
    keymap_free(keymap);
}

static void test_started_over_on_a_cable_sends_nothing(void) {
    static const struct {
        KeymapKey key;
        KeymapEvent event;
    } gestures[] = {
        {KeymapKeyUp, KeymapEventShort},
        {KeymapKeyOk, KeymapEventLong},
        {KeymapKeyUp, KeymapEventLong},
        {KeymapKeyDown, KeymapEventLong},
        {KeymapKeyLeft, KeymapEventLong},
    };
    for(size_t i = 0; i < sizeof(gestures) / sizeof(gestures[0]); i++) {
        Keymap* keymap = keymap_test_alloc_on_usb();
        keymap_time_out(keymap);
        assert(keymap_handle(keymap, gestures[i].key, KeymapEventPress) == ActionStartOver);
        keymap_set_connected(keymap, true);
        assert(keymap_handle(keymap, gestures[i].key, gestures[i].event) == ActionNone);
        assert(keymap_mode(keymap) == ModeMain);
        keymap_free(keymap);
    }
}

static void test_tap_through_link_return_does_nothing(void) {
    Keymap* keymap = keymap_alloc();
    keymap_handle(keymap, KeymapKeyUp, KeymapEventPress);
    keymap_set_connected(keymap, true);
    assert(keymap_handle(keymap, KeymapKeyUp, KeymapEventShort) == ActionNone);
    keymap_free(keymap);
}

static void test_back_held_through_drop_exits(void) {
    Keymap* keymap = keymap_test_alloc_connected();
    keymap_handle(keymap, KeymapKeyBack, KeymapEventPress);
    keymap_set_connected(keymap, false);
    assert(keymap_handle(keymap, KeymapKeyBack, KeymapEventLong) == ActionExit);
    keymap_free(keymap);
}

static void test_tap_on_back_closes_the_options(void) {
    Keymap* keymap = keymap_test_alloc_options();
    assert(keymap_test_tap(keymap, KeymapKeyBack) == ActionNone);
    assert(keymap_mode(keymap) == ModeMain);
    keymap_free(keymap);

    keymap = keymap_test_alloc_options_no_link();
    assert(keymap_test_tap(keymap, KeymapKeyBack) == ActionNone);
    assert(keymap_mode(keymap) == ModeNoLink);
    keymap_free(keymap);
}

static void test_tap_on_back_opens_nothing_elsewhere(void) {
    Keymap* keymaps[] = {
        keymap_test_alloc_voice(),
        keymap_alloc(),
        keymap_test_alloc_voice_cut(),
    };
    for(size_t i = 0; i < sizeof(keymaps) / sizeof(keymaps[0]); i++) {
        Mode mode = keymap_mode(keymaps[i]);
        assert(keymap_test_tap(keymaps[i], KeymapKeyBack) == ActionNone);
        assert(keymap_mode(keymaps[i]) == mode);
        keymap_free(keymaps[i]);
    }
}

static void test_back_tap_begun_elsewhere_opens_nothing(void) {
    Keymap* keymap = keymap_alloc();
    keymap_handle(keymap, KeymapKeyBack, KeymapEventPress);
    keymap_set_connected(keymap, true);
    assert(keymap_handle(keymap, KeymapKeyBack, KeymapEventShort) == ActionNone);
    assert(keymap_mode(keymap) == ModeMain);
    keymap_free(keymap);

    keymap = keymap_test_alloc_voice();
    keymap_handle(keymap, KeymapKeyBack, KeymapEventPress);
    keymap_handle(keymap, KeymapKeyOk, KeymapEventRelease);
    assert(keymap_handle(keymap, KeymapKeyBack, KeymapEventShort) == ActionNone);
    assert(keymap_mode(keymap) == ModeMain);
    keymap_free(keymap);
}

static void test_options_act_only_on_held_back(void) {
    Keymap* (*const allocs[])(void) = {
        keymap_test_alloc_options,
        keymap_test_alloc_options_no_link,
    };
    for(size_t i = 0; i < sizeof(allocs) / sizeof(allocs[0]); i++) {
        for(KeymapKey key = KeymapKeyUp; key < KeymapKeyMAX; key++) {
            for(KeymapEvent event = KeymapEventPress; event < KeymapEventMAX; event++) {
                Keymap* keymap = allocs[i]();
                Mode mode = keymap_mode(keymap);
                bool back_tapped = key == KeymapKeyBack && event == KeymapEventShort;
                Action expected = key == KeymapKeyBack && event == KeymapEventLong ? ActionExit :
                                                                                     ActionNone;
                if(event == KeymapEventShort || event == KeymapEventLong) {
                    keymap_handle(keymap, key, KeymapEventPress);
                }
                assert(keymap_handle(keymap, key, event) == expected);
                assert((keymap_mode(keymap) != mode) == back_tapped);
                keymap_free(keymap);
            }
        }
    }
}

static void test_fresh_keymap_has_every_option_on(void) {
    Keymap* keymap = keymap_alloc();
    for(Option option = OptionVibration; option < OptionMAX; option++) {
        assert(keymap_option(keymap, option));
    }
    keymap_free(keymap);
}

static void test_fresh_keymap_has_the_cursor_on_vibration(void) {
    Keymap* keymap = keymap_alloc();
    assert(keymap_cursor(keymap) == OptionVibration);
    keymap_free(keymap);
}

static void test_keymap_takes_each_saved_option(void) {
    for(Option option = OptionVibration; option < OptionMAX; option++) {
        Keymap* keymap = keymap_alloc();
        keymap_set_option(keymap, option, false);
        for(Option other = OptionVibration; other < OptionMAX; other++) {
            assert(keymap_option(keymap, other) == (other != option));
        }
        keymap_set_option(keymap, option, true);
        for(Option other = OptionVibration; other < OptionMAX; other++) {
            assert(keymap_option(keymap, other));
        }
        assert(keymap_cursor(keymap) == OptionVibration);
        assert(keymap_mode(keymap) == ModeNoLink);
        assert(keymap_link(keymap) == LinkBluetooth);
        assert(keymap_wait(keymap) == KeymapWaitStart);
        keymap_free(keymap);
    }
}

static void test_side_tap_on_the_options_switches_the_option_under_the_cursor(void) {
    Keymap* (*const allocs[])(void) = {
        keymap_test_alloc_options,
        keymap_test_alloc_options_no_link,
    };
    const KeymapKey keys[] = {KeymapKeyRight, KeymapKeyLeft, KeymapKeyLeft, KeymapKeyRight};
    for(size_t i = 0; i < sizeof(allocs) / sizeof(allocs[0]); i++) {
        for(Option option = OptionVibration; option < OptionMAX; option++) {
            Keymap* keymap = allocs[i]();
            Mode mode = keymap_mode(keymap);
            while(keymap_cursor(keymap) != option) {
                keymap_test_tap(keymap, KeymapKeyDown);
            }
            for(size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
                assert(keymap_test_tap(keymap, keys[k]) == ActionNone);
                for(Option other = OptionVibration; other < OptionMAX; other++) {
                    assert(keymap_option(keymap, other) == (other != option || k % 2 == 1));
                }
                assert(keymap_cursor(keymap) == option);
            }
            assert(keymap_mode(keymap) == mode);
            keymap_free(keymap);
        }
    }
}

static void test_up_or_down_tap_on_the_options_moves_the_cursor(void) {
    Keymap* (*const allocs[])(void) = {
        keymap_test_alloc_options,
        keymap_test_alloc_options_no_link,
    };
    const KeymapKey keys[] = {KeymapKeyDown, KeymapKeyDown, KeymapKeyUp, KeymapKeyUp};
    for(size_t i = 0; i < sizeof(allocs) / sizeof(allocs[0]); i++) {
        Keymap* keymap = allocs[i]();
        Mode mode = keymap_mode(keymap);
        keymap_test_tap(keymap, KeymapKeyRight);
        for(size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
            assert(keymap_test_tap(keymap, keys[k]) == ActionNone);
            assert(keymap_cursor(keymap) == (k % 2 == 0 ? OptionBacklight : OptionVibration));
            assert(!keymap_option(keymap, OptionVibration));
            assert(keymap_option(keymap, OptionBacklight));
        }
        assert(keymap_mode(keymap) == mode);
        keymap_free(keymap);
    }
}

static void test_nothing_else_switches_an_option(void) {
    Keymap* (*const allocs[])(void) = {
        keymap_test_alloc_connected,
        keymap_test_alloc_voice,
        keymap_alloc,
        keymap_test_alloc_voice_cut,
        keymap_test_alloc_stopped,
        keymap_test_alloc_options,
        keymap_test_alloc_options_no_link,
    };
    for(size_t i = 0; i < sizeof(allocs) / sizeof(allocs[0]); i++) {
        for(KeymapKey key = KeymapKeyUp; key < KeymapKeyMAX; key++) {
            for(KeymapEvent event = KeymapEventPress; event < KeymapEventMAX; event++) {
                Keymap* keymap = allocs[i]();
                Mode mode = keymap_mode(keymap);
                bool on_the_options = mode == ModeOptions || mode == ModeOptionsNoLink;
                bool side = key == KeymapKeyLeft || key == KeymapKeyRight;
                if(!on_the_options || !side || event != KeymapEventShort) {
                    if(event == KeymapEventShort || event == KeymapEventLong) {
                        keymap_handle(keymap, key, KeymapEventPress);
                    }
                    keymap_handle(keymap, key, event);
                    for(Option option = OptionVibration; option < OptionMAX; option++) {
                        assert(keymap_option(keymap, option));
                    }
                }
                keymap_free(keymap);
            }
        }
        Keymap* keymap = allocs[i]();
        keymap_set_connected(keymap, false);
        keymap_set_connected(keymap, true);
        keymap_time_out(keymap);
        for(Option option = OptionVibration; option < OptionMAX; option++) {
            assert(keymap_option(keymap, option));
        }
        keymap_free(keymap);
    }
}

static void test_nothing_else_moves_the_cursor(void) {
    Keymap* (*const allocs[])(void) = {
        keymap_test_alloc_connected,
        keymap_test_alloc_voice,
        keymap_alloc,
        keymap_test_alloc_voice_cut,
        keymap_test_alloc_stopped,
        keymap_test_alloc_options,
        keymap_test_alloc_options_no_link,
    };
    for(size_t i = 0; i < sizeof(allocs) / sizeof(allocs[0]); i++) {
        for(KeymapKey key = KeymapKeyUp; key < KeymapKeyMAX; key++) {
            for(KeymapEvent event = KeymapEventPress; event < KeymapEventMAX; event++) {
                Keymap* keymap = allocs[i]();
                Mode mode = keymap_mode(keymap);
                bool on_the_options = mode == ModeOptions || mode == ModeOptionsNoLink;
                bool vertical = key == KeymapKeyUp || key == KeymapKeyDown;
                if(!on_the_options || !vertical || event != KeymapEventShort) {
                    if(event == KeymapEventShort || event == KeymapEventLong) {
                        keymap_handle(keymap, key, KeymapEventPress);
                    }
                    keymap_handle(keymap, key, event);
                    assert(keymap_cursor(keymap) == OptionVibration);
                }
                keymap_free(keymap);
            }
        }
        Keymap* keymap = allocs[i]();
        keymap_set_connected(keymap, false);
        assert(keymap_cursor(keymap) == OptionVibration);
        keymap_set_connected(keymap, true);
        assert(keymap_cursor(keymap) == OptionVibration);
        keymap_time_out(keymap);
        assert(keymap_cursor(keymap) == OptionVibration);
        keymap_free(keymap);
    }
}

static void test_every_option_outlasts_the_options(void) {
    Keymap* keymap = keymap_test_alloc_options();
    keymap_test_tap(keymap, KeymapKeyRight);
    keymap_test_tap(keymap, KeymapKeyDown);
    keymap_test_tap(keymap, KeymapKeyRight);
    keymap_test_tap(keymap, KeymapKeyBack);
    assert(keymap_mode(keymap) == ModeMain);
    assert(keymap_test_tap(keymap, KeymapKeyUp) == ActionArrowUp);
    keymap_set_connected(keymap, false);
    keymap_time_out(keymap);
    assert(keymap_mode(keymap) == ModeStopped);
    keymap_handle(keymap, KeymapKeyUp, KeymapEventPress);
    keymap_set_connected(keymap, true);
    keymap_test_tap(keymap, KeymapKeyBack);
    assert(keymap_mode(keymap) == ModeOptions);
    for(Option option = OptionVibration; option < OptionMAX; option++) {
        assert(!keymap_option(keymap, option));
    }
    keymap_free(keymap);
}

static void test_cursor_outlasts_the_options(void) {
    Keymap* keymap = keymap_test_alloc_options();
    keymap_test_tap(keymap, KeymapKeyDown);
    keymap_test_tap(keymap, KeymapKeyBack);
    assert(keymap_mode(keymap) == ModeMain);
    assert(keymap_cursor(keymap) == OptionBacklight);
    assert(keymap_test_tap(keymap, KeymapKeyDown) == ActionArrowDown);
    assert(keymap_cursor(keymap) == OptionBacklight);
    keymap_set_connected(keymap, false);
    assert(keymap_cursor(keymap) == OptionBacklight);
    keymap_time_out(keymap);
    assert(keymap_mode(keymap) == ModeStopped);
    assert(keymap_cursor(keymap) == OptionBacklight);
    keymap_handle(keymap, KeymapKeyUp, KeymapEventPress);
    keymap_set_connected(keymap, true);
    assert(keymap_cursor(keymap) == OptionBacklight);
    keymap_test_tap(keymap, KeymapKeyBack);
    assert(keymap_mode(keymap) == ModeOptions);
    assert(keymap_cursor(keymap) == OptionBacklight);
    keymap_free(keymap);
}

static void test_link_moves_the_options_between_their_modes(void) {
    Keymap* keymap = keymap_test_alloc_options();
    assert(keymap_set_connected(keymap, false) == ActionNone);
    assert(keymap_mode(keymap) == ModeOptionsNoLink);
    assert(keymap_set_connected(keymap, false) == ActionNone);
    assert(keymap_mode(keymap) == ModeOptionsNoLink);
    assert(keymap_set_connected(keymap, true) == ActionNone);
    assert(keymap_mode(keymap) == ModeOptions);
    assert(keymap_set_connected(keymap, true) == ActionNone);
    assert(keymap_mode(keymap) == ModeOptions);
    keymap_free(keymap);
}

static void test_tap_through_a_drop_on_the_options_does_nothing(void) {
    Keymap* keymap = keymap_test_alloc_options();
    keymap_handle(keymap, KeymapKeyRight, KeymapEventPress);
    keymap_handle(keymap, KeymapKeyDown, KeymapEventPress);
    keymap_set_connected(keymap, false);
    assert(keymap_handle(keymap, KeymapKeyRight, KeymapEventShort) == ActionNone);
    assert(keymap_option(keymap, OptionVibration));
    assert(keymap_handle(keymap, KeymapKeyDown, KeymapEventShort) == ActionNone);
    assert(keymap_cursor(keymap) == OptionVibration);
    keymap_handle(keymap, KeymapKeyBack, KeymapEventPress);
    keymap_set_connected(keymap, true);
    assert(keymap_handle(keymap, KeymapKeyBack, KeymapEventShort) == ActionNone);
    assert(keymap_mode(keymap) == ModeOptions);
    keymap_free(keymap);
}

static void test_wait_ends_the_options_with_give_up(void) {
    Keymap* keymap = keymap_test_alloc_options();
    assert(keymap_wait_ms(keymap) == 0);
    assert(keymap_time_out(keymap) == ActionNone);
    assert(keymap_mode(keymap) == ModeOptions);
    keymap_set_connected(keymap, false);
    assert(keymap_wait_ms(keymap) == 60000);
    assert(keymap_time_out(keymap) == ActionGiveUp);
    assert(keymap_mode(keymap) == ModeStopped);
    keymap_free(keymap);
}

static void test_options_keep_the_wait_but_for_the_link(void) {
    Keymap* keymap = keymap_test_alloc_connected();
    keymap_handle(keymap, KeymapKeyBack, KeymapEventPress);
    keymap_handle(keymap, KeymapKeyBack, KeymapEventShort);
    assert(keymap_wait(keymap) == KeymapWaitKeep);
    keymap_test_tap(keymap, KeymapKeyRight);
    assert(keymap_wait(keymap) == KeymapWaitKeep);
    keymap_set_connected(keymap, false);
    assert(keymap_wait(keymap) == KeymapWaitStart);
    keymap_test_tap(keymap, KeymapKeyLeft);
    assert(keymap_wait(keymap) == KeymapWaitKeep);
    keymap_set_connected(keymap, true);
    assert(keymap_wait(keymap) == KeymapWaitStop);
    keymap_set_connected(keymap, false);
    keymap_handle(keymap, KeymapKeyBack, KeymapEventPress);
    keymap_handle(keymap, KeymapKeyBack, KeymapEventShort);
    assert(keymap_mode(keymap) == ModeNoLink);
    assert(keymap_wait(keymap) == KeymapWaitKeep);
    keymap_free(keymap);

    keymap = keymap_test_alloc_options();
    keymap_handle(keymap, KeymapKeyBack, KeymapEventPress);
    keymap_handle(keymap, KeymapKeyBack, KeymapEventShort);
    assert(keymap_mode(keymap) == ModeMain);
    assert(keymap_wait(keymap) == KeymapWaitKeep);
    keymap_free(keymap);
}

static void test_fresh_keymap_shows_the_legend(void) {
    Keymap* (*const allocs[])(void) = {keymap_alloc, keymap_test_alloc_on_usb};
    for(size_t i = 0; i < sizeof(allocs) / sizeof(allocs[0]); i++) {
        Keymap* keymap = allocs[i]();
        assert(keymap_legend(keymap));
        keymap_set_connected(keymap, true);
        assert(keymap_mode(keymap) == ModeMain);
        assert(keymap_legend(keymap));
        keymap_free(keymap);
    }
}

static void test_press_on_the_main_screen_ends_the_legend(void) {
    Keymap* (*const allocs[])(void) = {keymap_alloc, keymap_test_alloc_on_usb};
    for(size_t i = 0; i < sizeof(allocs) / sizeof(allocs[0]); i++) {
        for(KeymapKey key = KeymapKeyUp; key < KeymapKeyMAX; key++) {
            Keymap* keymap = allocs[i]();
            keymap_set_connected(keymap, true);
            keymap_handle(keymap, key, KeymapEventPress);
            assert(!keymap_legend(keymap));
            keymap_free(keymap);
        }
    }
}

static void test_nothing_else_ends_the_legend(void) {
    Keymap* (*const allocs[])(void) = {
        keymap_alloc,
        keymap_test_alloc_on_usb,
        keymap_test_alloc_stopped,
    };
    for(size_t i = 0; i < sizeof(allocs) / sizeof(allocs[0]); i++) {
        for(KeymapKey key = KeymapKeyUp; key < KeymapKeyMAX; key++) {
            for(KeymapEvent event = KeymapEventPress; event < KeymapEventMAX; event++) {
                Keymap* keymap = allocs[i]();
                keymap_handle(keymap, key, KeymapEventPress);
                keymap_handle(keymap, key, event);
                assert(keymap_legend(keymap));
                keymap_free(keymap);
            }
        }
        Keymap* keymap = allocs[i]();
        keymap_set_connected(keymap, false);
        keymap_time_out(keymap);
        assert(keymap_legend(keymap));
        keymap_free(keymap);
    }

    for(KeymapKey key = KeymapKeyUp; key < KeymapKeyMAX; key++) {
        Keymap* keymap = keymap_alloc();
        keymap_handle(keymap, key, KeymapEventPress);
        keymap_set_connected(keymap, true);
        keymap_handle(keymap, key, KeymapEventShort);
        keymap_handle(keymap, key, KeymapEventRelease);
        keymap_set_connected(keymap, true);
        keymap_time_out(keymap);
        assert(keymap_mode(keymap) == ModeMain);
        assert(keymap_legend(keymap));
        keymap_free(keymap);
    }
}

static void test_legend_stays_away(void) {
    Keymap* keymap = keymap_test_alloc_connected();
    keymap_test_tap(keymap, KeymapKeyUp);
    keymap_handle(keymap, KeymapKeyOk, KeymapEventPress);
    keymap_handle(keymap, KeymapKeyOk, KeymapEventLong);
    keymap_handle(keymap, KeymapKeyOk, KeymapEventRelease);
    assert(keymap_mode(keymap) == ModeMain);
    assert(!keymap_legend(keymap));
    keymap_test_tap(keymap, KeymapKeyBack);
    keymap_test_tap(keymap, KeymapKeyBack);
    assert(keymap_mode(keymap) == ModeMain);
    assert(!keymap_legend(keymap));
    keymap_set_connected(keymap, false);
    keymap_time_out(keymap);
    keymap_handle(keymap, KeymapKeyUp, KeymapEventPress);
    keymap_set_connected(keymap, true);
    assert(keymap_mode(keymap) == ModeMain);
    assert(!keymap_legend(keymap));
    keymap_free(keymap);
}

int main(void) {
    test_run(test_tap_acts);
    test_run(test_tap_on_back_opens_the_options);
    test_run(test_hold_acts);
    test_run(test_press_never_acts);
    test_run(test_repeat_never_acts);
    test_run(test_release_without_voice_does_nothing);
    test_run(test_hold_ok_turns_voice_on);
    test_run(test_release_ok_stops_voice_once);
    test_run(test_tap_ok_leaves_voice_off);
    test_run(test_voice_ignores_every_other_event);
    test_run(test_buttons_act_again_after_voice);
    test_run(test_fresh_keymap_has_no_link);
    test_run(test_fresh_keymap_is_on_bluetooth);
    test_run(test_link_up_ends_no_link);
    test_run(test_link_down_ends_main);
    test_run(test_link_down_cuts_voice);
    test_run(test_link_up_ends_voice_cut);
    test_run(test_repeated_link_up_changes_nothing);
    test_run(test_repeated_link_down_changes_nothing);
    test_run(test_no_link_acts_only_on_held_back_or_right);
    test_run(test_right_held_on_no_link_takes_usb_once);
    test_run(test_right_held_again_switches_back_and_forth);
    test_run(test_link_stays_through_every_mode);
    test_run(test_right_pressed_on_usb_stopped_returns_nothing);
    test_run(test_right_pressed_on_usb_main_returns_nothing);
    test_run(test_right_held_elsewhere_on_usb_keeps_usb);
    test_run(test_right_tapped_on_usb_no_link_does_nothing);
    test_run(test_right_tapped_on_no_link_does_nothing);
    test_run(test_right_long_with_no_press_does_nothing);
    test_run(test_right_pressed_on_stopped_takes_no_usb);
    test_run(test_right_pressed_on_main_takes_no_usb);
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
    test_run(test_opening_starts_the_wait);
    test_run(test_drop_starts_the_wait);
    test_run(test_starting_over_starts_the_wait);
    test_run(test_each_switch_starts_the_wait_afresh);
    test_run(test_link_up_stops_the_wait);
    test_run(test_wait_end_stops_the_wait);
    test_run(test_presses_with_no_link_keep_the_wait);
    test_run(test_ok_released_after_cut_keeps_the_wait);
    test_run(test_repeated_link_down_keeps_the_wait);
    test_run(test_events_with_a_link_keep_the_wait);
    test_run(test_events_once_stopped_keep_the_wait);
    test_run(test_started_over_on_a_cable_sends_nothing);
    test_run(test_tap_through_link_return_does_nothing);
    test_run(test_back_held_through_drop_exits);
    test_run(test_tap_on_back_closes_the_options);
    test_run(test_tap_on_back_opens_nothing_elsewhere);
    test_run(test_back_tap_begun_elsewhere_opens_nothing);
    test_run(test_options_act_only_on_held_back);
    test_run(test_fresh_keymap_has_every_option_on);
    test_run(test_fresh_keymap_has_the_cursor_on_vibration);
    test_run(test_keymap_takes_each_saved_option);
    test_run(test_side_tap_on_the_options_switches_the_option_under_the_cursor);
    test_run(test_up_or_down_tap_on_the_options_moves_the_cursor);
    test_run(test_nothing_else_switches_an_option);
    test_run(test_nothing_else_moves_the_cursor);
    test_run(test_every_option_outlasts_the_options);
    test_run(test_cursor_outlasts_the_options);
    test_run(test_link_moves_the_options_between_their_modes);
    test_run(test_tap_through_a_drop_on_the_options_does_nothing);
    test_run(test_wait_ends_the_options_with_give_up);
    test_run(test_options_keep_the_wait_but_for_the_link);
    test_run(test_fresh_keymap_shows_the_legend);
    test_run(test_press_on_the_main_screen_ends_the_legend);
    test_run(test_nothing_else_ends_the_legend);
    test_run(test_legend_stays_away);
    return 0;
}
