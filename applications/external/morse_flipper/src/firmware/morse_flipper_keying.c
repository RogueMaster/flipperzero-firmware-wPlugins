/*
 * Purpose: Convert keying sources into tone, HID/MIDI, GPIO, and trainer input.
 * Owns: audio preview state, source masks, WPM updates, and PC output routing.
 * Depends on: morse_flipper_app_i.h, keyer.h, and Flipper HID/notification APIs.
 * Tests: tests/test_keyer.c and tests/test_vail_modes.c cover core keyer paths.
 */

#include "morse_flipper_app_i.h"

static const MorseFlipperTone* morse_flipper_current_tone(const MorseFlipperApp* app) {
    if(app == NULL) return &morse_flipper_tones[MORSE_FLIPPER_DEFAULT_TONE_IDX];

    if(app->vail_tone_active && app->vail_tone_idx < COUNT_OF(morse_flipper_tones))
        return &morse_flipper_tones[app->vail_tone_idx];

    if(app->tone_idx >= COUNT_OF(morse_flipper_tones))
        return &morse_flipper_tones[MORSE_FLIPPER_DEFAULT_TONE_IDX];

    return &morse_flipper_tones[app->tone_idx];
}

const char* morse_flipper_current_tone_name(const MorseFlipperApp* app) {
    if(app != NULL && app->tone_idx == MORSE_FLIPPER_TONE_OFF_IDX) return "Off";
    return morse_flipper_current_tone(app)->name;
}

static const MorseFlipperTone* morse_flipper_current_audible_tone(const MorseFlipperApp* app) {
    const MorseFlipperTone* tone = morse_flipper_current_tone(app);

    if(tone->hz > 0.0f) return tone;
    return &morse_flipper_tones[MORSE_FLIPPER_DEFAULT_TONE_IDX];
}

float morse_flipper_active_tone_hz(const MorseFlipperApp* app) {
    float hz = morse_flipper_current_audible_tone(app)->hz;

    if(app != NULL && app->screen == MorseFlipperScreenHamRun &&
       !app->ham_keyer.break_in_enabled && app->audio_path == MorseFlipperAudioPathVibration)
        return morse_flipper_tones[MORSE_FLIPPER_DEFAULT_TONE_IDX].hz;

    if(app != NULL && app->vail_tone_active && app->vail_tone_idx < COUNT_OF(morse_flipper_tones))
        hz = morse_flipper_current_audible_tone(app)->hz;

    return hz;
}

static bool morse_flipper_ham_force_buzzer(const MorseFlipperApp* app) {
    return app != NULL && app->screen == MorseFlipperScreenHamRun &&
           !app->ham_keyer.break_in_enabled;
}

static bool morse_flipper_ham_silent_audio(const MorseFlipperApp* app) {
    return app != NULL && app->screen == MorseFlipperScreenHamRun &&
           app->ham_keyer.break_in_enabled;
}

bool morse_flipper_local_buzzer_enabled(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    return app->tone_idx != MORSE_FLIPPER_TONE_OFF_IDX;
}

bool morse_flipper_use_pwm_buzzer(const MorseFlipperApp* app) {
    return morse_flipper_audio_output_is_pwm(app) && app->audio_pwm.running;
}

bool morse_flipper_any_active_notes(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    return app->note_sources[0] != 0U || app->note_sources[1] != 0U || app->note_sources[2] != 0U;
}

static bool morse_flipper_training_input_muted(const MorseFlipperApp* app) {
    return app != NULL && app->screen == MorseFlipperScreenSession && app->trainer_playback_active;
}

static bool morse_flipper_signal_led_level(const MorseFlipperApp* app, bool want_tx_tone) {
    if(app == NULL) return false;

    /* Result tone is red; playback marks are orange. Both deliberately bypass TX state. */
    if(app->session_result_good && app->session_result_until != 0U &&
       furi_get_tick() < app->session_result_until)
        return true;
    if(app->session_result_tone) return true;
    if(app->trainer_playback_mark || app->straight_playback_mark) return true;

    if(app->screen == MorseFlipperScreenRf && app->rf_live_active) {
        return app->radio.tx_level;
    }

    if(app->screen == MorseFlipperScreenHamRun && app->ham_keyer.break_in_enabled) {
        return app->ham.key_level;
    }

    return want_tx_tone;
}

void morse_flipper_tone_stop(MorseFlipperApp* app) {
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }

    app->tone_on = false;
    app->speaker_owned = false;
    app->speaker_busy = false;
    app->speaker_hz = 0.0f;
}

static void morse_flipper_tone_start(MorseFlipperApp* app) {
    float hz;

    if(app->tone_on) return;

    if(!app->speaker_owned) {
        if(furi_hal_speaker_acquire(30U)) {
            app->speaker_owned = true;
        } else {
            app->speaker_busy = true;
            return;
        }
    }

    if(!furi_hal_speaker_is_mine()) {
        app->tone_on = false;
        app->speaker_owned = false;
        app->speaker_busy = true;
        return;
    }

    hz = morse_flipper_active_tone_hz(app);
    furi_hal_speaker_start(hz, MORSE_FLIPPER_VOLUME);
    app->tone_on = true;
    app->speaker_hz = hz;
    app->speaker_busy = false;
}

void morse_flipper_update_sidetone(MorseFlipperApp* app) {
    bool use_pwm;
    bool use_vibro;
    bool force_buzzer = morse_flipper_ham_force_buzzer(app);
    bool want_tx_tone = morse_flipper_any_active_notes(app) || (app->preview_ticks > 0U);
    bool want_aux_tone = app->trainer_playback_mark || app->straight_playback_mark ||
                         app->session_result_tone || app->rf_monitor_tone;
    bool want_signal = want_tx_tone || want_aux_tone;
    bool want_speaker;
    bool want_vibro;

    /*
     * One gate fans out to speaker, PWM, vibro, PTT, and signal LED.
     * Aux tones may sound without keying RF; ham break-in is the awkward exception.
     */
    if(morse_flipper_ham_silent_audio(app)) {
        if(app->audio_pwm.running) morse_flipper_audio_pwm_stop(&app->audio_pwm);
        furi_hal_vibro_on(false);
        if(app->speaker_owned || app->tone_on) morse_flipper_tone_stop(app);
        app->tone_on = false;
        app->speaker_busy = false;
        morse_flipper_sync_ptt(app, furi_get_tick());
        morse_flipper_sync_signal_led(app, morse_flipper_signal_led_level(app, want_tx_tone));
        return;
    }

    use_pwm = morse_flipper_use_pwm_buzzer(app);
    use_vibro = !force_buzzer && app != NULL && app->audio_path == MorseFlipperAudioPathVibration;
    want_speaker = !use_pwm &&
                   (want_aux_tone ||
                    (want_tx_tone && (force_buzzer || morse_flipper_local_buzzer_enabled(app))));
    want_vibro = use_vibro && want_signal;

    if(force_buzzer && app->audio_pwm.running) {
        morse_flipper_audio_pwm_stop(&app->audio_pwm);
    }

    if(use_pwm) {
        furi_hal_vibro_on(false);
        if(app->speaker_owned) {
            morse_flipper_tone_stop(app);
        }

        morse_flipper_audio_pwm_set_gate(&app->audio_pwm, want_signal);
        app->tone_on = want_signal || morse_flipper_audio_pwm_sound_active(&app->audio_pwm);
        app->speaker_busy = false;
        morse_flipper_sync_ptt(app, furi_get_tick());
        morse_flipper_sync_signal_led(app, morse_flipper_signal_led_level(app, want_tx_tone));
        return;
    }

    if(use_vibro) {
        if(app->speaker_owned || app->tone_on) morse_flipper_tone_stop(app);
        furi_hal_vibro_on(want_vibro);
        app->tone_on = want_vibro;
        app->speaker_busy = false;
        morse_flipper_sync_ptt(app, furi_get_tick());
        morse_flipper_sync_signal_led(app, morse_flipper_signal_led_level(app, want_tx_tone));
        return;
    }

    furi_hal_vibro_on(false);
    if(want_speaker) {
        float hz = morse_flipper_active_tone_hz(app);

        morse_flipper_tone_start(app);
        if(app->tone_on && app->speaker_owned && furi_hal_speaker_is_mine() &&
           app->speaker_hz != hz) {
            furi_hal_speaker_start(hz, MORSE_FLIPPER_VOLUME);
            app->speaker_hz = hz;
            app->speaker_busy = false;
        }
    } else {
        morse_flipper_tone_stop(app);
    }

    morse_flipper_sync_ptt(app, furi_get_tick());
    morse_flipper_sync_signal_led(app, morse_flipper_signal_led_level(app, want_tx_tone));
}

uint32_t morse_flipper_note_source_for_paddle(uint8_t paddle) {
    return (paddle == MorseKeyerPaddleDit) ? MORSE_SOURCE_KEYER_DIT : MORSE_SOURCE_KEYER_DAH;
}

uint8_t morse_flipper_note_for_paddle(uint8_t paddle) {
    return (paddle == MorseKeyerPaddleDit) ? 1U : 2U;
}

void morse_flipper_set_note_source(
    MorseFlipperApp* app,
    uint8_t note,
    uint32_t source_mask,
    bool active) {
    uint32_t now_ms;

    if(note >= COUNT_OF(app->note_sources)) return;
    if(active && morse_flipper_training_input_muted(app)) active = false;

    uint32_t before = app->note_sources[note];
    uint32_t after = active ? (before | source_mask) : (before & ~source_mask);

    /* Source masks let GPIO, buttons, macros, and keyer events share one note cleanly. */
    if(before == after) return;

    app->note_sources[note] = after;
    if(app->screen == MorseFlipperScreenRf && app->rf_live_active) {
        bool can_tx = morse_flipper_rf_tx_allowed_khz(morse_flipper_rf_frequency_khz(&app->rf));
        now_ms = furi_get_tick();
        app->rf_tx_tail_until =
            now_ms + ((uint32_t)morse_flipper_current_dit_ms(app) * MORSE_FLIPPER_RF_TX_TAIL_DITS);

        if(morse_flipper_any_active_notes(app) && can_tx && !app->radio.tx_on) {
            morse_flipper_radio_sync_live(
                &app->radio,
                morse_flipper_rf_frequency_hz(&app->rf),
                true,
                true,
                MorseFlipperRadioProfileOokData);
        }
        morse_flipper_radio_set_tx_level(
            &app->radio, morse_flipper_any_active_notes(app) && can_tx);
    }
    morse_flipper_update_sidetone(app);

    if(before == 0U && after != 0U) {
        morse_flipper_send_transport_note(app, note, true);
    } else if(before != 0U && after == 0U) {
        morse_flipper_send_transport_note(app, note, false);
    }
}

void morse_flipper_release_all_notes(MorseFlipperApp* app) {
    uint32_t note_sources[COUNT_OF(app->note_sources)];

    for(size_t note = 0; note < COUNT_OF(app->note_sources); note++) {
        note_sources[note] = app->note_sources[note];
        app->note_sources[note] = 0U;
    }

    for(size_t note = 0; note < COUNT_OF(app->note_sources); note++) {
        if(note_sources[note] != 0U) morse_flipper_send_transport_note(app, (uint8_t)note, false);
    }

    if(app->screen == MorseFlipperScreenRf && app->rf_live_active) {
        uint32_t now_ms = furi_get_tick();
        app->rf_tx_tail_until =
            now_ms + ((uint32_t)morse_flipper_current_dit_ms(app) * MORSE_FLIPPER_RF_TX_TAIL_DITS);
        morse_flipper_radio_set_tx_level(&app->radio, false);
    }
    morse_flipper_update_sidetone(app);
}

void morse_flipper_drain_keyer_events(MorseFlipperApp* app) {
    MorseKeyerEvent event;

    while(morse_keyer_pop_event(&app->keyer, &event)) {
        if((app->screen == MorseFlipperScreenTrainer ||
            app->screen == MorseFlipperScreenSession) &&
           event.type == MorseKeyerEventPress &&
           morse_trainer_phase(&app->trainer) == MorseTrainerPhaseRepeat) {
            morse_trainer_feed_element(
                &app->trainer, event.paddle == MorseKeyerPaddleDit ? '.' : '-');
        }

        if(app->screen == MorseFlipperScreenSession &&
           morse_trainer_phase(&app->trainer) == MorseTrainerPhaseRepeat)
            app->session_last_input_at = furi_get_tick();

        morse_flipper_set_note_source(
            app,
            morse_flipper_note_for_paddle(event.paddle),
            morse_flipper_note_source_for_paddle(event.paddle),
            event.type == MorseKeyerEventPress);
    }
}

void morse_flipper_set_paddle_source(
    MorseFlipperApp* app,
    uint8_t paddle,
    uint32_t source_mask,
    bool active,
    uint32_t now_ms) {
    if(paddle >= MorseKeyerPaddleCount) return;
    if(active && morse_flipper_training_input_muted(app)) active = false;

    uint32_t before = app->paddle_sources[paddle];
    uint32_t after = active ? (before | source_mask) : (before & ~source_mask);

    if(before == after) return;

    app->paddle_sources[paddle] = after;
    morse_keyer_paddle_event(&app->keyer, paddle, after != 0U, now_ms);
    morse_keyer_tick(&app->keyer, now_ms);
    morse_flipper_drain_keyer_events(app);
}

void morse_flipper_refresh_keyer(MorseFlipperApp* app, uint32_t now_ms) {
    morse_keyer_reset(&app->keyer);
    morse_flipper_drain_keyer_events(app);
    morse_keyer_set_mode(&app->keyer, morse_flipper_current_keyer_mode(app));
    morse_keyer_set_dit_duration(&app->keyer, morse_flipper_current_dit_ms(app));

    for(uint8_t paddle = 0; paddle < MorseKeyerPaddleCount; paddle++) {
        if(app->paddle_sources[paddle] != 0U)
            morse_keyer_paddle_event(&app->keyer, paddle, true, now_ms);
    }

    morse_keyer_tick(&app->keyer, now_ms);
    morse_flipper_drain_keyer_events(app);
}

static void morse_flipper_clear_button_paddles(MorseFlipperApp* app, uint32_t now_ms) {
    morse_flipper_set_paddle_source(
        app, MorseKeyerPaddleDit, MORSE_PADDLE_SOURCE_BTN_OK, false, now_ms);
    morse_flipper_set_paddle_source(
        app, MorseKeyerPaddleDah, MORSE_PADDLE_SOURCE_BTN_OK, false, now_ms);
    morse_flipper_set_paddle_source(
        app, MorseKeyerPaddleDit, MORSE_PADDLE_SOURCE_BTN_BACK, false, now_ms);
    morse_flipper_set_paddle_source(
        app, MorseKeyerPaddleDah, MORSE_PADDLE_SOURCE_BTN_BACK, false, now_ms);
}

void morse_flipper_resync_button_paddles(MorseFlipperApp* app, uint32_t now_ms) {
    uint8_t ok_paddle = morse_flipper_ok_button_paddle(app);
    uint8_t back_paddle = morse_flipper_back_button_paddle(app);

    morse_flipper_set_paddle_source(
        app,
        MorseKeyerPaddleDit,
        MORSE_PADDLE_SOURCE_BTN_OK,
        app->ok_down && ok_paddle == MorseKeyerPaddleDit,
        now_ms);
    morse_flipper_set_paddle_source(
        app,
        MorseKeyerPaddleDah,
        MORSE_PADDLE_SOURCE_BTN_OK,
        app->ok_down && ok_paddle == MorseKeyerPaddleDah,
        now_ms);
    morse_flipper_set_paddle_source(
        app,
        MorseKeyerPaddleDit,
        MORSE_PADDLE_SOURCE_BTN_BACK,
        app->back_down && back_paddle == MorseKeyerPaddleDit,
        now_ms);
    morse_flipper_set_paddle_source(
        app,
        MorseKeyerPaddleDah,
        MORSE_PADDLE_SOURCE_BTN_BACK,
        app->back_down && back_paddle == MorseKeyerPaddleDah,
        now_ms);
}

void morse_flipper_clear_button_keying(MorseFlipperApp* app, uint32_t now_ms) {
    app->left_down = false;
    app->ok_down = false;
    app->back_down = false;
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, false);
    morse_flipper_clear_button_paddles(app, now_ms);
}
