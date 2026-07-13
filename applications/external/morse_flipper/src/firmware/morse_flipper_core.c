/*
 * Purpose: Provide central constants and shared app utility functions.
 * Owns: tone tables, USB mode names, notifications, and cross-feature helpers.
 * Depends on: morse_flipper_app_i.h, cw.h, and Flipper notification services.
 * Tests: firmware build plus feature-specific host tests.
 */

#include "morse_flipper_app_i.h"
#include "cw.h"

const char* const morse_flipper_usb_mode_names[] = {
    "None",
    "Keyboard",
    "Mouse",
    "MIDI",
};

const uint8_t morse_flipper_input_values[] = {
    MorseFlipperInputSourceButtons,
    MorseFlipperInputSourceStraight,
    MorseFlipperInputSourcePaddle,
};

const char* const morse_flipper_input_names[] = {
    "buttons",
    "straight",
    "paddle",
};

const char* const morse_flipper_audio_path_names[] = {
    "Buzzer",
    "P2 (HD)",
    "Vibration",
};

const uint8_t morse_flipper_keyer_values[] = {
    MorseKeyerModeStraight,
    MorseKeyerModeBug,
    MorseKeyerModePlainIambic,
    MorseKeyerModeIambicA,
    MorseKeyerModeIambicB,
    MorseKeyerModeUltimatic,
    MorseKeyerModeKeyahead,
};

const MorseFlipperTone morse_flipper_tones[] = {
    {"G2", 98.00f, 43U},   {"A2", 110.00f, 45U},  {"B2", 123.47f, 47U},  {"C3", 130.81f, 48U},
    {"D3", 146.83f, 50U},  {"E3", 164.81f, 52U},  {"F3", 174.61f, 53U},  {"G3", 196.00f, 55U},
    {"A3", 220.00f, 57U},  {"B3", 246.94f, 59U},  {"C4", 261.63f, 60U},  {"D4", 293.66f, 62U},
    {"E4", 329.63f, 64U},  {"F4", 349.23f, 65U},  {"G4", 392.00f, 67U},  {"A4", 440.00f, 69U},
    {"B4", 493.88f, 71U},  {"C5", 523.25f, 72U},  {"D5", 587.33f, 74U},  {"E5", 659.25f, 76U},
    {"F5", 698.46f, 77U},  {"G5", 783.99f, 79U},  {"A5", 880.00f, 81U},  {"B5", 987.77f, 83U},
    {"C6", 1046.50f, 84U}, {"D6", 1174.66f, 86U}, {"E6", 1318.51f, 88U}, {"F6", 1396.91f, 89U},
    {"G6", 1567.98f, 91U}, {"A6", 1760.00f, 93U}, {"B6", 1975.53f, 95U},
};

uint8_t morse_flipper_backlight_mode(const MorseFlipperApp* app) {
    if(app == NULL) return MorseFlipperBacklightAuto;

    if(app->screen == MorseFlipperScreenRun || app->screen == MorseFlipperScreenTrace ||
       app->screen == MorseFlipperScreenHamRun || app->screen == MorseFlipperScreenStraight ||
       app->screen == MorseFlipperScreenTxGroups ||
       app->screen == MorseFlipperScreenTxGroupsResult ||
       app->screen == MorseFlipperScreenTxGroupsFinal)
        return MorseFlipperBacklightHold;

    if(app->screen == MorseFlipperScreenRf || app->screen == MorseFlipperScreenRfRx)
        return app->rf_live_active ? MorseFlipperBacklightHold : MorseFlipperBacklightAuto;

    if(app->screen == MorseFlipperScreenSession || app->screen == MorseFlipperScreenSessionEnd)
        return app->session_started ? MorseFlipperBacklightHold : MorseFlipperBacklightAuto;

    return MorseFlipperBacklightAuto;
}

void morse_flipper_sync_backlight(MorseFlipperApp* app, uint32_t now_ms) {
    uint8_t want;

    if(app == NULL || app->notifications == NULL) return;
    UNUSED(now_ms);

    want = morse_flipper_backlight_mode(app);
    if(want != app->backlight_mode) {
        if(app->backlight_mode != MorseFlipperBacklightAuto)
            notification_message(app->notifications, &sequence_display_backlight_enforce_auto);

        app->backlight_mode = want;

        if(want != MorseFlipperBacklightAuto)
            notification_message(app->notifications, &sequence_display_backlight_enforce_on);
    }
}

void morse_flipper_sync_signal_led(MorseFlipperApp* app, bool on) {
    bool red;
    bool green;

    if(app == NULL) return;

    red = on && app->session_result_tone;
    green = on && app->session_result_good && app->session_result_until != 0U &&
            furi_get_tick() < app->session_result_until;
    if(app->signal_led_on == on && app->signal_led_red == red && app->signal_led_green == green)
        return;

    app->signal_led_on = on;
    app->signal_led_red = red;
    app->signal_led_green = green;
    if(on) {
        furi_hal_light_set(LightBlue, 0U);
        furi_hal_light_set(LightGreen, green ? 255U : (red ? 0U : 96U));
        furi_hal_light_set(LightRed, green ? 0U : 255U);
    } else {
        furi_hal_light_set(LightRed | LightGreen | LightBlue, 0U);
    }
}

static void morse_flipper_feedback_do(MorseFlipperApp* app) {
    if(app == NULL) return;
    app->session_result_tone = true;
    app->session_result_until = furi_get_tick() + MORSE_FLIPPER_SESSION_RESULT_MS;
    morse_flipper_update_sidetone(app);
}

void morse_flipper_feedback_pass(MorseFlipperApp* app) {
    if(app == NULL) return;
    app->session_result_tone = false;
    app->session_result_until = 0U;
    morse_flipper_update_sidetone(app);
}

void morse_flipper_feedback_fail(MorseFlipperApp* app) {
    morse_flipper_feedback_do(app);
}

void morse_flipper_feedback_timeout(MorseFlipperApp* app) {
    morse_flipper_feedback_do(app);
}

uint8_t morse_flipper_input_value_index(uint8_t source) {
    for(uint8_t i = 0U; i < COUNT_OF(morse_flipper_input_values); i++) {
        if(morse_flipper_input_values[i] == source) {
            return i;
        }
    }

    return 0U;
}

void morse_flipper_set_local_wpm(MorseFlipperApp* app, uint8_t wpm) {
    if(app == NULL) return;

    if(wpm < 10U) wpm = 10U;
    if(wpm > 30U) wpm = 30U;

    app->trainer.local_dit_ms = morse_flipper_wpm_to_dit_ms(wpm);
    morse_flipper_clamp_trainer_settings(app);
    morse_flipper_cw_decoder_init(&app->rf_decoder, morse_flipper_current_dit_ms(app));
    morse_flipper_cw_decoder_init(&app->tx_decoder, morse_flipper_current_dit_ms(app));
    morse_flipper_cw_decoder_init(&app->gpio_decoder, morse_flipper_current_dit_ms(app));
    morse_flipper_refresh_keyer(app, furi_get_tick());
}

void morse_flipper_set_run_wpm(MorseFlipperApp* app, uint8_t wpm) {
    if(app == NULL) return;

    if(wpm < 10U) wpm = 10U;
    if(wpm > 30U) wpm = 30U;

    app->run_dit_ms = morse_flipper_wpm_to_dit_ms(wpm);
    morse_flipper_cw_decoder_init(&app->tx_decoder, morse_flipper_current_dit_ms(app));
    app->rf_tx_edge_at = 0U;
    app->rf_tx_gap_flushed = true;
    app->rf_tx_level = false;
    morse_flipper_refresh_keyer(app, furi_get_tick());
    morse_flipper_view_dirty(app);
}

void morse_flipper_clear_run_wpm(MorseFlipperApp* app, uint32_t now_ms) {
    uint16_t dit_ms;

    if(app == NULL || app->run_dit_ms == 0U) return;

    app->run_dit_ms = 0U;
    dit_ms = morse_flipper_current_dit_ms(app);
    morse_flipper_cw_decoder_init(&app->rf_decoder, dit_ms);
    morse_flipper_cw_decoder_init(&app->tx_decoder, dit_ms);
    morse_flipper_cw_decoder_init(&app->gpio_decoder, dit_ms);
    morse_flipper_refresh_keyer(app, now_ms);
}

uint8_t morse_flipper_straight_wpm(const MorseFlipperApp* app) {
    uint16_t dit;
    uint8_t wpm;

    if(app == NULL) return 0U;
    dit = app->straight_dit_ms ? app->straight_dit_ms : MORSE_FLIPPER_DEFAULT_DIT_MS;
    wpm = (uint8_t)((1200U + (dit / 2U)) / dit);
    if(wpm < 10U) wpm = 10U;
    if(wpm > 30U) wpm = 30U;
    return wpm;
}

void morse_flipper_set_straight_wpm(MorseFlipperApp* app, uint8_t wpm) {
    if(app == NULL) return;

    if(wpm < 10U) wpm = 10U;
    if(wpm > 30U) wpm = 30U;
    app->straight_dit_ms = morse_flipper_wpm_to_dit_ms(wpm);
    morse_flipper_clamp_straight_settings(app);
}

static uint16_t morse_flipper_trainer_farnsworth_unit_ms(const MorseFlipperApp* app) {
    uint32_t w;
    uint32_t farn;
    uint32_t dit;
    uint32_t total;
    uint32_t spare;

    if(app == NULL) return MORSE_FLIPPER_DEFAULT_DIT_MS;

    w = morse_flipper_local_wpm(app);
    farn = app->trainer_farnsworth_wpm;
    dit = app->trainer.local_dit_ms ? app->trainer.local_dit_ms : MORSE_FLIPPER_DEFAULT_DIT_MS;
    if(farn == 0U || farn >= w) return (uint16_t)dit;

    total = 60000U / farn;
    if(total <= 31U * dit) return (uint16_t)dit;

    spare = total - (31U * dit);
    return (uint16_t)((spare + 9U) / 19U);
}

static uint16_t morse_flipper_trainer_char_gap_ms(const MorseFlipperApp* app) {
    uint16_t dit_ms;

    if(app == NULL) return MORSE_FLIPPER_DEFAULT_DIT_MS * 3U;

    dit_ms = app->trainer.local_dit_ms ? app->trainer.local_dit_ms : MORSE_FLIPPER_DEFAULT_DIT_MS;
    if(app->screen != MorseFlipperScreenSession) return (uint16_t)(dit_ms * 3U);
    return (uint16_t)(morse_flipper_trainer_farnsworth_unit_ms(app) * 3U);
}

uint8_t morse_flipper_keyer_value_index(uint8_t mode) {
    for(uint8_t i = 0U; i < COUNT_OF(morse_flipper_keyer_values); i++) {
        if(morse_flipper_keyer_values[i] == mode) {
            return i;
        }
    }

    return 0U;
}

uint16_t morse_flipper_wpm_to_dit_ms(uint8_t wpm) {
    if(wpm < 1U) {
        return MORSE_FLIPPER_DEFAULT_DIT_MS;
    }

    return (1200U + (wpm / 2U)) / wpm;
}

uint8_t morse_flipper_current_keyer_mode(const MorseFlipperApp* app) {
    if(app != NULL && app->screen == MorseFlipperScreenStraight) {
        return MorseKeyerModeStraight;
    }

    if(app->vail_mode_active) {
        return app->vail_keyer_mode;
    }

    return app->keyer_mode;
}

uint16_t morse_flipper_current_dit_ms(const MorseFlipperApp* app) {
    if(app != NULL &&
       (app->screen == MorseFlipperScreenRun || app->screen == MorseFlipperScreenHamRun) &&
       app->run_dit_ms != 0U) {
        return app->run_dit_ms;
    }

    if(app->vail_speed_active) {
        return app->vail_dit_ms;
    }

    return app->trainer.local_dit_ms ? app->trainer.local_dit_ms : MORSE_FLIPPER_DEFAULT_DIT_MS;
}

uint16_t morse_flipper_current_straight_dit_ms(const MorseFlipperApp* app) {
    if(app == NULL) return MORSE_FLIPPER_DEFAULT_DIT_MS;
    return app->straight_dit_ms ? app->straight_dit_ms : MORSE_FLIPPER_DEFAULT_DIT_MS;
}

uint8_t morse_flipper_current_wpm(const MorseFlipperApp* app) {
    uint16_t dit_ms = morse_flipper_current_dit_ms(app);

    if(dit_ms == 0U) {
        return 0U;
    }

    return (uint8_t)((1200U + (dit_ms / 2U)) / dit_ms);
}

bool morse_flipper_straight_like_mode(const MorseFlipperApp* app) {
    uint8_t mode = morse_flipper_current_keyer_mode(app);

    return mode == MorseKeyerModePassthrough || mode == MorseKeyerModeStraight;
}

void morse_flipper_reset_run_state(MorseFlipperApp* app) {
    if(app == NULL) return;

    morse_flipper_run_history_reset(&app->run_history);
    morse_flipper_audio_pwm_set_gate(&app->audio_pwm, false);
    morse_flipper_straight_filter_reset(&app->straight_filter);
    app->rf_tx_text[0] = '\0';
    morse_flipper_cw_decoder_init(&app->tx_decoder, morse_flipper_current_dit_ms(app));
    app->rf_tx_edge_at = 0U;
    app->rf_tx_gap_flushed = true;
    app->rf_tx_level = false;
}

bool morse_flipper_session_running_view(const MorseFlipperApp* app) {
    if(app == NULL || app->screen != MorseFlipperScreenSession) return false;
    return !morse_flipper_session_idle_view(app);
}

void morse_flipper_toggle_source(MorseFlipperApp* app) {
    uint32_t now_ms = furi_get_tick();

    morse_flipper_drop_live_keying_for_playback(app, now_ms);

    if(app->input_source == MorseFlipperInputSourceStraight) {
        app->input_source = MorseFlipperInputSourcePaddle;
    } else if(app->input_source == MorseFlipperInputSourcePaddle) {
        app->input_source = MorseFlipperInputSourceButtons;
    } else {
        app->input_source = MorseFlipperInputSourceStraight;
    }

    morse_flipper_save_config(app);
    morse_flipper_refresh_keyer(app, now_ms);
    morse_flipper_poll(app);
    morse_flipper_view_dirty(app);
}

bool morse_flipper_training_playback_active(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    if(app->screen == MorseFlipperScreenStraight) return app->straight_playback_active;
    return app->screen == MorseFlipperScreenSession && app->trainer_playback_active;
}

bool morse_flipper_straight_answer_down(const MorseFlipperApp* app) {
    if(app == NULL) return false;

    if(app->input_source == MorseFlipperInputSourceButtons) return false;
    if(morse_flipper_gpio_probe_blocks_start(app)) return false;
    if(app->input_source == MorseFlipperInputSourcePaddle)
        return morse_flipper_gpio_probe_use_straight(app) ?
                   !furi_hal_gpio_read(morse_flipper_dit_pin) :
                   (morse_flipper_logical_dit_down(app) || morse_flipper_logical_dah_down(app));
    return morse_flipper_straight_down();
}

void morse_flipper_tick_trainer_playback(MorseFlipperApp* app, uint32_t now_ms) {
    const char* group;
    uint16_t cw_code;
    uint32_t dit_ms;
    uint8_t marks;

    if(app == NULL || !app->trainer_playback_active || now_ms < app->trainer_next_at) {
        return;
    }

    group = morse_trainer_last_group(&app->trainer);
    if(group[app->trainer_char_idx] == '\0') {
        app->trainer_playback_active = false;
        app->trainer_playback_mark = false;
        app->trainer_next_at = 0U;
        if(app->screen == MorseFlipperScreenSession) app->session_start_holdoff = true;
        morse_flipper_view_dirty(app);
        return;
    }

    cw_code = cw(group[app->trainer_char_idx]);
    marks = cw_symbol_count(cw_code);
    dit_ms = morse_flipper_current_dit_ms(app);

    if(marks == 0U) {
        app->trainer_char_idx++;
        app->trainer_mark_idx = 0U;
        app->trainer_next_at = now_ms + morse_flipper_trainer_char_gap_ms(app);
        morse_flipper_view_dirty(app);
        return;
    }

    if(app->trainer_playback_mark) {
        app->trainer_playback_mark = false;
        if(app->trainer_mark_idx + 1U < marks) {
            app->trainer_mark_idx++;
            app->trainer_next_at = now_ms + dit_ms;
        } else if(group[app->trainer_char_idx + 1U] != '\0') {
            app->trainer_char_idx++;
            app->trainer_mark_idx = 0U;
            app->trainer_next_at = now_ms + morse_flipper_trainer_char_gap_ms(app);
        } else {
            app->trainer_playback_active = false;
            app->trainer_next_at = 0U;
            morse_trainer_finish_listen(&app->trainer);
            if(app->screen == MorseFlipperScreenSession) {
                app->session_start_holdoff = true;
                app->session_last_input_at = now_ms;
            }
        }
        morse_flipper_update_sidetone(app);
        morse_flipper_view_dirty(app);
        return;
    }

    if(app->trainer_mark_idx >= marks) {
        app->trainer_playback_active = false;
        app->trainer_next_at = 0U;
        if(app->screen == MorseFlipperScreenSession) app->session_start_holdoff = true;
        morse_flipper_update_sidetone(app);
        return;
    }

    app->trainer_playback_mark = true;
    app->trainer_next_at = now_ms + (dit_ms * cw_symbol_units(cw_code, app->trainer_mark_idx));
    morse_flipper_update_sidetone(app);
    morse_flipper_view_dirty(app);
}

void morse_flipper_cycle_trainer_value(MorseFlipperApp* app, int dir) {
    int next;

    if(app == NULL) {
        return;
    }

    switch(app->trainer_row) {
    case 0:
        next = (int)morse_trainer_lesson(&app->trainer) + dir;
        morse_trainer_set_lesson(&app->trainer, (uint8_t)next);
        break;
    case 1:
        next = (int)morse_trainer_group_size(&app->trainer) + dir;
        morse_trainer_set_group_size(&app->trainer, (uint8_t)next);
        break;
    case 2:
        next = (int)morse_trainer_session_groups(&app->trainer) + dir;
        morse_trainer_set_session_groups(&app->trainer, (uint8_t)next);
        break;
    default:
        morse_flipper_ensure_custom_sets_loaded(app);
        next = (int)morse_flipper_effective_trainer_custom_set_idx(app) + dir;
        if(next < 0) {
            next = (int)app->custom_sets.count;
        } else if(next > (int)app->custom_sets.count) {
            next = 0;
        }
        app->trainer.custom_set_idx = (uint8_t)next;
        morse_flipper_apply_trainer_charset_choice(app);
        break;
    }

    morse_flipper_view_dirty(app);
}

void morse_flipper_leave_live_screen(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    if(app->screen == MorseFlipperScreenSession) {
        morse_flipper_leave_session(app, now_ms);
        return;
    }

    morse_flipper_clear_button_keying(app, now_ms);
    morse_flipper_scene_back(app);
}

void morse_flipper_ensure_custom_sets_loaded(MorseFlipperApp* app) {
    uint8_t selected;

    if(app == NULL || app->custom_sets_loaded) {
        return;
    }

    selected = app->trainer.custom_set_idx;
    app->custom_sets_loaded = morse_trainer_load_custom_sets(&app->custom_sets);
    app->trainer.custom_set_idx = selected;
    morse_flipper_apply_trainer_charset_choice(app);
}

uint8_t morse_flipper_effective_trainer_custom_set_idx(const MorseFlipperApp* app) {
    if(app == NULL || app->trainer.custom_set_idx == 0U) return 0U;
    if(app->custom_sets.count == 0U || app->trainer.custom_set_idx > app->custom_sets.count)
        return 0U;
    return app->trainer.custom_set_idx;
}

void morse_flipper_apply_trainer_charset_choice(MorseFlipperApp* app) {
    uint8_t idx;

    if(app == NULL) {
        return;
    }

    idx = morse_flipper_effective_trainer_custom_set_idx(app);
    if(idx == 0U) {
        app->trainer.custom_name[0] = '\0';
        app->trainer.charset_override[0] = '\0';
        return;
    }

    strncpy(
        app->trainer.custom_name,
        app->custom_sets.sets[idx - 1U].name,
        sizeof(app->trainer.custom_name) - 1U);
    app->trainer.custom_name[sizeof(app->trainer.custom_name) - 1U] = '\0';
    strncpy(
        app->trainer.charset_override,
        app->custom_sets.sets[idx - 1U].chars,
        sizeof(app->trainer.charset_override) - 1U);
    app->trainer.charset_override[sizeof(app->trainer.charset_override) - 1U] = '\0';
}

void morse_flipper_cycle_mode(MorseFlipperApp* app) {
    uint32_t now_ms = furi_get_tick();

    morse_flipper_drop_live_keying_for_playback(app, now_ms);
    app->keyer_mode = morse_keyer_next_ui_mode(app->keyer_mode);
    morse_flipper_save_config(app);
    morse_flipper_refresh_keyer(app, now_ms);
    morse_flipper_view_dirty(app);
}

void morse_flipper_toggle_handedness(MorseFlipperApp* app) {
    uint32_t now_ms = furi_get_tick();

    if(app->handedness == MorseFlipperHandednessNormal) {
        app->handedness = MorseFlipperHandednessSwapped;
    } else {
        app->handedness = MorseFlipperHandednessNormal;
    }

    morse_flipper_save_config(app);
    morse_flipper_resync_button_paddles(app, now_ms);
    morse_flipper_refresh_keyer(app, now_ms);
    morse_flipper_poll(app);
    morse_flipper_view_dirty(app);
}
