/*
 * Purpose: Bridge straight trainer model state into app runtime.
 * Owns: straight trainer resets, playback ticks, answer capture, and scoring.
 * Depends on: morse_flipper_app_i.h and morse_flipper_straight_trainer.h.
 * Tests: tests/test_straight_trainer.c covers the host trainer model.
 */

#include "morse_flipper_app_i.h"

static void morse_flipper_release_straight_keying(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    if(app->straight_key_down && app->straight_mark_started_at != 0U) {
        uint32_t mark_ms = now_ms - app->straight_mark_started_at;

        if(mark_ms > UINT16_MAX) mark_ms = UINT16_MAX;
        morse_flipper_feed_straight_mark(app, (uint16_t)mark_ms, now_ms);
    }

    app->straight_key_down = false;
    app->straight_mark_started_at = 0U;

    if(app->input_source == MorseFlipperInputSourceButtons) app->ok_down = false;
    morse_flipper_straight_filter_reset(&app->straight_filter);
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, false);
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_GPIO, false);
    morse_flipper_update_sidetone(app);
}

static void morse_flipper_silence_straight_keying(MorseFlipperApp* app) {
    if(app == NULL) return;

    app->straight_key_down = false;
    app->straight_mark_started_at = 0U;
    if(app->input_source == MorseFlipperInputSourceButtons) app->ok_down = false;
    morse_flipper_straight_filter_reset(&app->straight_filter);
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, false);
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_GPIO, false);
    morse_flipper_update_sidetone(app);
}

void morse_flipper_reset_straight_state(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    app->straight_playback_active = false;
    app->straight_playback_mark = false;
    app->straight_started = false;
    app->straight_wait_answer = false;
    app->straight_done = false;
    app->straight_key_down = false;
    app->straight_mark_idx = 0U;
    app->straight_next_at = 0U;
    app->straight_wait_started_at = 0U;
    app->straight_answer_started_at = 0U;
    app->straight_last_input_at = 0U;
    app->straight_mark_started_at = 0U;
    app->straight_next_draw_s = 0xFFU;
    app->straight_cutoff_wait_release = false;
    app->straight_trainer.active = false;
    app->straight_trainer.answer[0] = '\0';
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_BTN, false);
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_GPIO, false);
    morse_flipper_update_sidetone(app);
    morse_flipper_clear_button_keying(app, now_ms);
}

void morse_flipper_start_straight_round(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    if(!app->straight_started) morse_flipper_reset_straight_session(app);
    morse_flipper_reset_straight_state(app, now_ms);
    morse_flipper_straight_trainer_start(
        &app->straight_trainer,
        MORSE_FLIPPER_STRAIGHT_CHARSET,
        morse_flipper_current_straight_dit_ms(app));
    app->straight_started = true;
    app->straight_playback_active = true;
    app->straight_playback_mark = false;
    app->straight_mark_idx = 0U;
    app->straight_next_at = now_ms;
    morse_flipper_view_dirty(app);
}

void morse_flipper_reset_straight_session(MorseFlipperApp* app) {
    if(app == NULL) return;
    app->straight_session_total = 0U;
    app->straight_session_good = 0U;
}

void morse_flipper_note_straight_session(MorseFlipperApp* app) {
    if(app == NULL) return;

    app->straight_session_total++;
    if(strcmp(
           morse_flipper_straight_trainer_target_morse(&app->straight_trainer),
           morse_flipper_straight_trainer_answer(&app->straight_trainer)) == 0)
        app->straight_session_good++;
}

bool morse_flipper_straight_countdown_active(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    return app->screen == MorseFlipperScreenStraight && app->straight_started &&
           !app->straight_playback_active && !app->straight_wait_answer && !app->straight_done &&
           app->straight_next_at != 0U;
}

void morse_flipper_start_straight_countdown(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    if(!app->straight_started) morse_flipper_reset_straight_session(app);
    morse_flipper_reset_straight_state(app, now_ms);
    app->straight_started = true;
    app->straight_next_at = now_ms + ((uint32_t)app->straight_next_delay_s * 1000U);
    app->straight_next_draw_s = 0xFFU;
    morse_flipper_view_dirty(app);
}

void morse_flipper_finish_straight_round(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    morse_flipper_release_straight_keying(app, now_ms);
    app->straight_wait_answer = false;
    app->straight_done = true;
    app->straight_cutoff_wait_release = false;
    app->straight_trainer.active = false;
    app->straight_next_at = now_ms + ((uint32_t)app->straight_next_delay_s * 1000U);
    app->straight_next_draw_s = 0xFFU;
    morse_flipper_note_straight_session(app);
    morse_flipper_view_dirty(app);
}

static uint32_t morse_flipper_straight_target_ms(const MorseFlipperApp* app) {
    uint32_t units;
    uint32_t dit_ms;

    if(app == NULL) return MORSE_FLIPPER_DEFAULT_DIT_MS;

    units = morse_flipper_straight_trainer_target_total_units(&app->straight_trainer);
    if(units == 0U) units = 1U;
    dit_ms = morse_flipper_current_straight_dit_ms(app);
    return units * dit_ms;
}

static void morse_flipper_cutoff_straight_answer(MorseFlipperApp* app, uint32_t now_ms) {
    uint32_t mark_ms;

    if(app == NULL || app->straight_cutoff_wait_release) return;

    if(app->straight_key_down && app->straight_mark_started_at != 0U) {
        mark_ms = now_ms - app->straight_mark_started_at;
        if(mark_ms > UINT16_MAX) mark_ms = UINT16_MAX;
        if(mark_ms != 0U) morse_flipper_feed_straight_mark(app, (uint16_t)mark_ms, now_ms);
        app->straight_cutoff_wait_release = true;
        morse_flipper_silence_straight_keying(app);
        morse_flipper_view_dirty(app);
        return;
    }

    morse_flipper_finish_straight_round(app, now_ms);
}

void morse_flipper_tick_straight(MorseFlipperApp* app, uint32_t now_ms) {
    uint32_t dit_ms;
    uint32_t left_ms;
    uint8_t marks;
    uint8_t mark_units;
    uint8_t left_s;

    if(app == NULL) return;

    if(app->straight_wait_answer && !app->straight_done && app->straight_answer_started_at != 0U &&
       !app->straight_cutoff_wait_release) {
        uint32_t limit_ms = morse_flipper_straight_target_ms(app) * 4U;

        if(limit_ms != 0U && now_ms - app->straight_answer_started_at > limit_ms) {
            morse_flipper_cutoff_straight_answer(app, now_ms);
            return;
        }
    }

    if(app->straight_cutoff_wait_release) return;

    if(morse_flipper_straight_countdown_active(app)) {
        if(now_ms >= app->straight_next_at) {
            morse_flipper_start_straight_round(app, now_ms);
        } else {
            left_ms = app->straight_next_at - now_ms;
            left_s = (uint8_t)((left_ms + 999U) / 1000U);
            if(left_s != app->straight_next_draw_s) {
                app->straight_next_draw_s = left_s;
                morse_flipper_view_dirty(app);
            }
        }
        return;
    }

    if(app->straight_playback_active && now_ms >= app->straight_next_at) {
        dit_ms = morse_flipper_current_straight_dit_ms(app);
        marks = morse_flipper_straight_trainer_target_symbol_count(&app->straight_trainer);

        if(!app->straight_playback_mark) {
            if(app->straight_mark_idx >= marks) {
                app->straight_playback_active = false;
                app->straight_wait_answer = true;
                app->straight_wait_started_at = now_ms;
                app->straight_next_at = 0U;
                morse_flipper_update_sidetone(app);
                morse_flipper_view_dirty(app);
                return;
            }

            app->straight_playback_mark = true;
            mark_units = morse_flipper_straight_trainer_target_mark_units(
                &app->straight_trainer, app->straight_mark_idx);
            app->straight_next_at = now_ms + (dit_ms * mark_units);
            morse_flipper_update_sidetone(app);
            morse_flipper_view_dirty(app);
            return;
        }

        app->straight_playback_mark = false;
        app->straight_mark_idx++;
        if(app->straight_mark_idx >= marks) {
            app->straight_playback_active = false;
            app->straight_wait_answer = true;
            app->straight_wait_started_at = now_ms;
            app->straight_next_at = 0U;
        } else {
            app->straight_next_at = now_ms + dit_ms;
        }
        morse_flipper_update_sidetone(app);
        morse_flipper_view_dirty(app);
    }

    if(app->straight_wait_answer &&
       morse_flipper_straight_trainer_answer(&app->straight_trainer)[0] == '\0') {
        uint32_t timeout_ms = (uint32_t)app->straight_answer_timeout_s * 1000U;

        if(app->straight_wait_started_at != 0U &&
           now_ms - app->straight_wait_started_at >= timeout_ms) {
            morse_flipper_finish_straight_round(app, now_ms);
            return;
        }
    }

    if(app->straight_wait_answer &&
       morse_flipper_straight_trainer_answer(&app->straight_trainer)[0] != '\0') {
        uint32_t settle = morse_flipper_straight_answer_settle_ms(app);

        if(settle != 0U && !app->straight_key_down &&
           now_ms - app->straight_last_input_at >= settle) {
            morse_flipper_finish_straight_round(app, now_ms);
            return;
        }
    }

    if(app->straight_done && app->straight_next_at != 0U) {
        if(now_ms >= app->straight_next_at) {
            morse_flipper_start_straight_round(app, now_ms);
        } else {
            left_ms = app->straight_next_at - now_ms;
            left_s = (uint8_t)((left_ms + 999U) / 1000U);
            if(left_s != app->straight_next_draw_s) {
                app->straight_next_draw_s = left_s;
                morse_flipper_view_dirty(app);
            }
        }
    }
}
