/*
 * Purpose: Run LCWO-style listen/repeat training sessions.
 * Owns: session state resets, answer text, scoring, and prompt progression.
 * Depends on: morse_flipper_app_i.h, trainer.h, and CW token helpers.
 * Tests: tests/test_trainer.c covers the host trainer core.
 */

#include "morse_flipper_app_i.h"
#include "morse_flipper_cw_token.h"

#define MORSE_FLIPPER_SESSION_DELETE_FLASH_STEP_MS 132U
#define MORSE_FLIPPER_SESSION_DELETE_FLASH_DONE_MS 400U
#define MORSE_FLIPPER_SESSION_DELETE_FLASH_IDLE    0xFFU

static void morse_flipper_session_answer_text(
    const MorseFlipperApp* app,
    char* out,
    size_t out_sz,
    uint8_t max_chars);
static uint8_t morse_flipper_session_answer_count(const char* answer);
static void morse_flipper_session_answer_committed_text(
    const MorseFlipperApp* app,
    char* out,
    size_t out_sz,
    uint8_t max_chars);

static void morse_flipper_note_session_progress_group(MorseFlipperApp* app);

static bool morse_flipper_session_answer_is_straight(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    if(app->input_source == MorseFlipperInputSourceStraight) return true;
    if(app->input_source == MorseFlipperInputSourcePaddle &&
       morse_flipper_gpio_probe_use_straight(app))
        return true;
    if(app->input_source == MorseFlipperInputSourceButtons &&
       morse_flipper_straight_like_mode(app))
        return true;
    return false;
}

static void morse_flipper_session_reset_answer_decoder(MorseFlipperApp* app) {
    if(app == NULL) return;

    if(morse_flipper_session_answer_is_straight(app)) {
        morse_flipper_cw_decoder_init(
            &app->tx_decoder, morse_flipper_current_straight_dit_ms(app));
    } else {
        morse_flipper_cw_decoder_init_fixed(&app->tx_decoder, morse_flipper_current_dit_ms(app));
    }
}

void morse_flipper_reset_session_runtime(MorseFlipperApp* app) {
    if(app == NULL) return;

    app->session_started = false;
    app->session_round_pending = false;
    app->session_result_hold = false;
    app->session_result_tone = false;
    app->session_result_good = false;
    app->session_start_holdoff = false;
    app->session_last_input_at = 0U;
    app->session_answer_complete_at = 0U;
    app->session_answer_flash_started_ms = 0U;
    app->session_result_until = 0U;
    app->session_next_group_at = 0U;
    app->session_complete_at = 0U;
    app->session_wait_draw_s = 0xFFU;
    app->session_answer_flash_phase = MORSE_FLIPPER_SESSION_DELETE_FLASH_IDLE;
    app->session_deleted_text[0] = '\0';
}

void morse_flipper_reset_session_state(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    app->trainer_playback_active = false;
    app->trainer_playback_mark = false;

    morse_flipper_drop_live_keying_for_playback(app, now_ms);
    morse_flipper_release_session_progress(app, false);
    morse_flipper_reset_session_runtime(app);
    morse_trainer_reset_session(&app->trainer);
    app->rf_tx_text[0] = '\0';
    app->rf_tx_edge_at = 0U;
    app->rf_tx_gap_flushed = true;
    app->rf_tx_level = false;
    morse_flipper_session_reset_answer_decoder(app);

    morse_flipper_refresh_keyer(app, now_ms);
    morse_flipper_update_sidetone(app);
}

bool morse_flipper_session_repeat_active(const MorseFlipperApp* app) {
    return app && app->screen == MorseFlipperScreenSession && app->session_started &&
           !app->trainer_playback_active && app->session_next_group_at == 0U &&
           morse_trainer_phase(&app->trainer) == MorseTrainerPhaseRepeat;
}

static bool morse_flipper_session_decoder_has_answer(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    if(morse_flipper_cw_decoder_output(&app->tx_decoder)[0] != '\0') return true;
    return morse_flipper_cw_decoder_preview(&app->tx_decoder) != 0U;
}

static bool morse_flipper_session_pop_committed_answer(MorseFlipperApp* app) {
    size_t len;

    if(app == NULL) return false;

    len = strlen(app->rf_tx_text);
    while(len > 0U) {
        len--;
        if(app->rf_tx_text[len] != ' ' && app->rf_tx_text[len] != '|') {
            app->rf_tx_text[len] = '\0';
            return true;
        }
    }

    if(app->rf_tx_text[0] == '\0') return false;
    app->rf_tx_text[0] = '\0';
    return true;
}

static uint8_t
    morse_flipper_session_answer_flash_phase(const MorseFlipperApp* app, uint32_t now_ms) {
    uint32_t elapsed;

    if(app == NULL || app->session_answer_flash_started_ms == 0U) {
        return MORSE_FLIPPER_SESSION_DELETE_FLASH_IDLE;
    }

    elapsed = now_ms - app->session_answer_flash_started_ms;
    if(elapsed >= MORSE_FLIPPER_SESSION_DELETE_FLASH_DONE_MS) {
        return MORSE_FLIPPER_SESSION_DELETE_FLASH_IDLE;
    }

    return (uint8_t)(elapsed / MORSE_FLIPPER_SESSION_DELETE_FLASH_STEP_MS);
}

static bool
    morse_flipper_session_answer_flash_visible(const MorseFlipperApp* app, uint32_t now_ms) {
    uint8_t phase = morse_flipper_session_answer_flash_phase(app, now_ms);
    return phase == 0U || phase == 2U;
}

void morse_flipper_session_cancel_answer_flash(MorseFlipperApp* app) {
    if(app == NULL || app->session_answer_flash_started_ms == 0U) return;

    app->session_answer_flash_started_ms = 0U;
    app->session_answer_flash_phase = MORSE_FLIPPER_SESSION_DELETE_FLASH_IDLE;
    app->session_deleted_text[0] = '\0';
    morse_flipper_view_dirty(app);
}

static void morse_flipper_session_start_answer_flash(
    MorseFlipperApp* app,
    const char* deleted_text,
    uint32_t now_ms) {
    if(app == NULL || deleted_text == NULL || deleted_text[0] == '\0') return;

    strlcpy(app->session_deleted_text, deleted_text, sizeof(app->session_deleted_text));
    app->session_answer_flash_started_ms = now_ms != 0U ? now_ms : 1U;
    app->session_answer_flash_phase = 0U;
}

static void morse_flipper_session_tick_answer_flash(MorseFlipperApp* app, uint32_t now_ms) {
    uint8_t phase;

    if(app == NULL || app->session_answer_flash_started_ms == 0U) return;

    phase = morse_flipper_session_answer_flash_phase(app, now_ms);
    if(phase == MORSE_FLIPPER_SESSION_DELETE_FLASH_IDLE) {
        app->session_answer_flash_started_ms = 0U;
        app->session_answer_flash_phase = MORSE_FLIPPER_SESSION_DELETE_FLASH_IDLE;
        app->session_deleted_text[0] = '\0';
        morse_flipper_view_dirty(app);
    } else if(phase != app->session_answer_flash_phase) {
        app->session_answer_flash_phase = phase;
        morse_flipper_view_dirty(app);
    }
}

static void morse_flipper_session_note_answer_edit(
    MorseFlipperApp* app,
    const char* deleted_text,
    uint32_t now_ms) {
    app->session_last_input_at = now_ms;
    app->session_answer_complete_at = 0U;
    app->rf_tx_gap_flushed = true;
    app->rf_tx_edge_at = now_ms;
    morse_flipper_session_start_answer_flash(app, deleted_text, now_ms);
    morse_flipper_view_dirty(app);
}

bool morse_flipper_session_backspace_answer(MorseFlipperApp* app, uint32_t now_ms) {
    char deleted_text[MORSE_TRAINER_GROUP_CAP];
    bool changed = false;

    if(!morse_flipper_session_repeat_active(app)) return false;

    morse_flipper_session_answer_text(
        app, deleted_text, sizeof(deleted_text), MORSE_TRAINER_GROUP_CAP - 1U);
    if(deleted_text[0] == '\0') return false;

    if(morse_flipper_session_decoder_has_answer(app)) {
        morse_flipper_cw_decoder_reset(&app->tx_decoder);
        changed = true;
    } else {
        changed = morse_flipper_session_pop_committed_answer(app);
    }

    if(changed) morse_flipper_session_note_answer_edit(app, deleted_text, now_ms);
    return changed;
}

bool morse_flipper_session_clear_answer(MorseFlipperApp* app, uint32_t now_ms) {
    char deleted_text[MORSE_TRAINER_GROUP_CAP];
    bool changed;

    if(!morse_flipper_session_repeat_active(app)) return false;

    morse_flipper_session_answer_text(
        app, deleted_text, sizeof(deleted_text), MORSE_TRAINER_GROUP_CAP - 1U);
    changed = deleted_text[0] != '\0';
    app->rf_tx_text[0] = '\0';
    morse_flipper_cw_decoder_reset(&app->tx_decoder);

    if(changed) morse_flipper_session_note_answer_edit(app, deleted_text, now_ms);
    return changed;
}

bool morse_flipper_session_idle_view(const MorseFlipperApp* app) {
    MorseTrainerPhase ph;

    if(!app || app->screen != MorseFlipperScreenSession) return false;

    ph = morse_trainer_phase(&app->trainer);
    return !app->session_started ||
           (!app->trainer_playback_active && !app->session_round_pending &&
            !app->session_result_hold && !app->session_result_tone &&
            app->session_next_group_at == 0U && !morse_trainer_session_active(&app->trainer) &&
            ph != MorseTrainerPhaseRepeat);
}

static bool morse_flipper_session_live_keying(const MorseFlipperApp* app) {
    return app && (app->paddle_sources[MorseKeyerPaddleDit] != 0U ||
                   app->paddle_sources[MorseKeyerPaddleDah] != 0U || app->note_sources[0] != 0U ||
                   app->note_sources[1] != 0U || app->note_sources[2] != 0U);
}

bool morse_flipper_session_wait_key_down(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    if(morse_flipper_gpio_probe_notice_active(app)) return false;
    if(morse_flipper_gpio_probe_blocks_start(app)) return false;

    if(app->input_source == MorseFlipperInputSourceButtons) {
        if(morse_flipper_straight_like_mode(app)) return app->ok_down;
        return app->ok_down || app->back_down;
    }

    if(app->input_source == MorseFlipperInputSourceStraight) return morse_flipper_straight_down();
    if(morse_flipper_gpio_probe_use_straight(app))
        return !furi_hal_gpio_read(morse_flipper_dit_pin);

    return morse_flipper_logical_dit_down(app) || morse_flipper_logical_dah_down(app);
}

static bool morse_flipper_session_can_hurry(const MorseFlipperApp* app) {
    return app != NULL && app->session_result_hold && app->session_next_group_at != 0U;
}

bool morse_flipper_session_hurry(MorseFlipperApp* app, uint32_t now_ms) {
    if(!morse_flipper_session_can_hurry(app)) return false;
    if(app->session_next_group_at <= now_ms + 1000U) return true;

    app->session_next_group_at = now_ms + 1000U;
    app->session_wait_draw_s = 0xFFU;
    morse_flipper_view_dirty(app);
    return true;
}

static void morse_flipper_queue_session_feedback(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL || !app->session_round_pending) return;

    /* Result hold is both UI pause and error tone window; next prompt waits behind it. */
    app->session_round_pending = false;
    app->session_result_hold = true;
    app->session_result_good = !morse_trainer_last_failed(&app->trainer);
    app->session_result_tone = !app->session_result_good;
    app->session_result_until = now_ms + MORSE_FLIPPER_SESSION_RESULT_MS;
    app->session_answer_complete_at = 0U;
    app->session_next_group_at = morse_trainer_session_has_next(&app->trainer) ?
                                     (now_ms + ((uint32_t)app->trainer_group_pause_s * 1000U)) :
                                     0U;
    app->session_wait_draw_s = 0xFFU;
    morse_flipper_update_sidetone(app);
    morse_flipper_view_dirty(app);
}

void morse_flipper_drop_live_keying_for_playback(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    morse_flipper_clear_button_keying(app, now_ms);
    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_STRAIGHT_GPIO, false);
    morse_flipper_set_paddle_source(
        app, MorseKeyerPaddleDit, MORSE_PADDLE_SOURCE_GPIO_DIT, false, now_ms);
    morse_flipper_set_paddle_source(
        app, MorseKeyerPaddleDah, MORSE_PADDLE_SOURCE_GPIO_DAH, false, now_ms);
}

void morse_flipper_begin_group_playback(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) {
        return;
    }

    /* Playback owns the keying path while it runs; live input comes back afterwards. */
    morse_flipper_drop_live_keying_for_playback(app, now_ms);
    app->trainer_playback_active = true;
    app->trainer_playback_mark = false;
    app->trainer_char_idx = 0U;
    app->trainer_mark_idx = 0U;
    app->trainer_next_at = now_ms;
    app->rf_tx_text[0] = '\0';
    app->rf_tx_edge_at = 0U;
    app->rf_tx_gap_flushed = true;
    app->rf_tx_level = false;
    morse_flipper_session_reset_answer_decoder(app);
    if(app->screen == MorseFlipperScreenSession) {
        app->session_round_pending = true;
        app->session_result_hold = false;
        app->session_result_tone = false;
        app->session_result_good = false;
        app->session_last_input_at = now_ms;
        app->session_result_until = 0U;
        app->session_next_group_at = 0U;
        app->session_wait_draw_s = 0xFFU;
    }
    morse_flipper_view_dirty(app);
}

void morse_flipper_start_session(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    if(app->trainer.custom_set_idx != 0U && app->trainer.charset_override[0] == '\0') {
        morse_flipper_ensure_custom_sets_loaded(app);
        morse_flipper_unload_custom_sets(app);
    }

    morse_flipper_reset_session_state(app, now_ms);
    app->session_progress_recorded = false;
    app->session_progress_dirty = false;
    morse_flipper_ensure_session_progress_loaded(app);
    morse_trainer_start_session(&app->trainer);
    app->session_started = true;
    app->session_round_pending = false;
    app->session_result_hold = true;
    app->session_result_tone = false;
    app->session_result_good = false;
    app->session_result_until = 0U;
    app->session_answer_complete_at = 0U;
    app->session_next_group_at = now_ms + ((uint32_t)app->trainer_group_pause_s * 1000U);
    app->session_wait_draw_s = 0xFFU;
    mf_tlm_session(app);
    mf_tlm_group(app);
    morse_flipper_view_dirty(app);
}

void morse_flipper_tick_session(MorseFlipperApp* app, uint32_t now_ms) {
    uint32_t dt;
    uint32_t left_ms;
    uint8_t left_s;
    char ans[MORSE_TRAINER_GROUP_CAP];
    uint8_t group_size;
    uint8_t answer_count;

    if(app == NULL || app->screen != MorseFlipperScreenSession || !app->session_started) return;

    /*
     * Session flow, in order: expire error tone, count down to the next group,
     * launch/listen/playback, delay final screen, then score or timeout the answer.
     */
    if(app->session_result_tone && now_ms >= app->session_result_until) {
        app->session_result_tone = false;
        morse_flipper_update_sidetone(app);
    }

    morse_flipper_session_tick_answer_flash(app, now_ms);

    if(app->session_result_hold && app->session_next_group_at > now_ms) {
        if(morse_flipper_session_wait_key_down(app)) {
            morse_flipper_session_hurry(app, now_ms);
        }

        left_ms = app->session_next_group_at - now_ms;
        left_s = (uint8_t)((left_ms + 999U) / 1000U);
        if(left_s == 0U) left_s = 1U;
        if(left_s != app->session_wait_draw_s) {
            app->session_wait_draw_s = left_s;
            morse_flipper_view_dirty(app);
        }
    }

    if(app->session_next_group_at != 0U && now_ms >= app->session_next_group_at) {
        app->session_next_group_at = 0U;
        app->session_wait_draw_s = 0xFFU;
        if(!app->session_round_pending && !app->trainer_playback_active &&
           morse_trainer_phase(&app->trainer) == MorseTrainerPhaseListen) {
            app->session_result_hold = false;
            morse_flipper_begin_group_playback(app, now_ms);
        } else if(morse_trainer_next_session_group(&app->trainer)) {
            mf_tlm_group(app);
            morse_flipper_begin_group_playback(app, now_ms);
        } else {
            morse_flipper_view_dirty(app);
        }
        return;
    }

    if(morse_trainer_session_completed(&app->trainer) ||
       (morse_trainer_session_aborted(&app->trainer) &&
        morse_trainer_phase(&app->trainer) == MorseTrainerPhaseDone)) {
        if(app->session_complete_at == 0U) {
            app->session_complete_at = now_ms;
            mf_tlm_done(app);
        }
        if(now_ms - app->session_complete_at >= 1000U) {
            morse_flipper_scene_open(app, MorseFlipperSceneSessionEnd);
            return;
        }
    } else {
        app->session_complete_at = 0U;
    }

    if(!app->session_round_pending || app->trainer_playback_active) return;

    if(morse_trainer_phase(&app->trainer) == MorseTrainerPhaseDone) {
        morse_flipper_queue_session_feedback(app, now_ms);
        return;
    }

    if(morse_flipper_session_live_keying(app)) {
        app->session_last_input_at = now_ms;
        app->session_answer_complete_at = 0U;
        return;
    }

    if(!morse_flipper_session_repeat_active(app)) return;

    group_size = morse_trainer_group_size(&app->trainer);
    morse_flipper_session_answer_text(app, ans, sizeof(ans), MORSE_TRAINER_GROUP_CAP - 1U);
    answer_count = morse_flipper_session_answer_count(ans);

    if(answer_count >= group_size) {
        if(app->session_answer_complete_at == 0U) {
            app->session_answer_complete_at = now_ms;
            return;
        }

        if(now_ms - app->session_answer_complete_at < MORSE_FLIPPER_SESSION_ANSWER_GRACE_MS)
            return;

        morse_trainer_score_repeat_text(&app->trainer, ans);
        mf_tlm_answer(app, ans, !morse_trainer_last_failed(&app->trainer));
        morse_flipper_note_session_progress_group(app);
        morse_flipper_queue_session_feedback(app, now_ms);
        return;
    }
    app->session_answer_complete_at = 0U;

    dt = (uint32_t)app->trainer_answer_timeout_s * 1000U;
    if(dt == 0U) dt = (uint32_t)MORSE_FLIPPER_TRAINER_TIMEOUT_DEFAULT_S * 1000U;
    if(now_ms - app->session_last_input_at < dt) return;

    morse_trainer_score_repeat_text(&app->trainer, ans);
    mf_tlm_answer(app, ans, !morse_trainer_last_failed(&app->trainer));
    morse_flipper_note_session_progress_group(app);
    morse_flipper_queue_session_feedback(app, now_ms);
}

void morse_flipper_leave_session(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;
    morse_flipper_reset_session_state(app, now_ms);
    morse_flipper_scene_back(app);
}

static uint8_t morse_flipper_session_final_percent(const MorseFlipperApp* app) {
    if(app == NULL) return 100U;
    return morse_trainer_session_letter_percent(&app->trainer);
}

static void morse_flipper_note_session_progress_group(MorseFlipperApp* app) {
    if(app == NULL || !app->session_started) return;
    if(morse_flipper_effective_trainer_custom_set_idx(app) != 0U) return;
    if(!morse_flipper_ensure_session_progress_loaded(app)) return;
    if(app->session_progress == NULL) return;

    morse_flipper_progress_note_weak_group(
        app->session_progress,
        morse_trainer_charset(&app->trainer),
        morse_trainer_last_group(&app->trainer),
        morse_trainer_reveal(&app->trainer));
    app->session_progress_dirty = true;
}

void morse_flipper_record_session_progress(MorseFlipperApp* app) {
    DateTime dt = {0};
    uint16_t practice_day = MORSE_FLIPPER_PROGRESS_DAY_NONE;
    bool date_valid = false;
    uint8_t percent;

    if(app == NULL) return;
    if(app->session_progress_recorded) return;
    if(!morse_trainer_session_aborted(&app->trainer) &&
       !morse_trainer_session_completed(&app->trainer))
        return;

    if(morse_trainer_session_aborted(&app->trainer)) {
        app->session_progress_recorded = true;
        morse_flipper_release_session_progress(app, false);
        return;
    }

    app->session_progress_recorded = true;
    if(!morse_flipper_ensure_session_progress_loaded(app) || app->session_progress == NULL) {
        app->session_progress_dirty = false;
        return;
    }

    furi_hal_rtc_get_datetime(&dt);
    date_valid = morse_flipper_progress_date_to_day(dt.year, dt.month, dt.day, &practice_day);
    percent = morse_flipper_session_final_percent(app);

    if(morse_flipper_effective_trainer_custom_set_idx(app) != 0U) {
        morse_flipper_progress_note_custom_attempt(
            app->session_progress, date_valid, practice_day);
        morse_flipper_release_session_progress(app, true);
        return;
    }

    morse_flipper_progress_note_standard_attempt(
        app->session_progress,
        date_valid,
        practice_day,
        morse_trainer_lesson(&app->trainer),
        percent,
        morse_trainer_session_total(&app->trainer));
    if(morse_flipper_progress_save(app->session_progress) && date_valid) {
        morse_flipper_progress_append_history(
            dt.year,
            dt.month,
            dt.day,
            dt.hour,
            dt.minute,
            morse_trainer_lesson(&app->trainer),
            percent);
    }
    morse_flipper_release_session_progress(app, false);
}

static const char* morse_flipper_session_end_blurb(const MorseFlipperApp* app) {
    if(app == NULL) return "Keep practicing";
    if(morse_trainer_lesson(&app->trainer) >= 40U) return "Congratulations!";
    if(morse_flipper_session_final_percent(app) >= 90U) return "Try the next lesson";
    return "Keep practicing";
}

static uint8_t
    morse_flipper_score_wheel_seed(const MorseFlipperApp* app, uint8_t score, uint8_t digit_idx) {
    uint32_t seed;

    seed = app == NULL ? 0U : app->star_anim_started_at;
    seed ^= (uint32_t)score * 37U;
    seed ^= (uint32_t)(digit_idx + 1U) * 173U;
    seed ^= seed >> 11;
    seed ^= seed << 7;
    seed ^= seed >> 13;
    return (uint8_t)seed;
}

static bool morse_flipper_score_digit_settles_first(
    const MorseFlipperApp* app,
    uint8_t score,
    uint8_t digit_idx) {
    bool left_first = (morse_flipper_score_wheel_seed(app, score, 2U) & 1U) == 0U;
    return digit_idx == 0U ? left_first : !left_first;
}

static uint16_t morse_flipper_score_wheel_random_ms(
    const MorseFlipperApp* app,
    uint8_t score,
    uint8_t digit_idx) {
    if(morse_flipper_score_digit_settles_first(app, score, digit_idx)) {
        return MORSE_FLIPPER_SCORE_WHEEL_RANDOM_MS - MORSE_FLIPPER_SCORE_WHEEL_RANDOM_JITTER_MS;
    }

    return MORSE_FLIPPER_SCORE_WHEEL_RANDOM_MS + MORSE_FLIPPER_SCORE_WHEEL_RANDOM_JITTER_MS;
}

static uint16_t morse_flipper_score_wheel_settle_ms(
    const MorseFlipperApp* app,
    uint8_t score,
    uint8_t digit_idx) {
    if(morse_flipper_score_digit_settles_first(app, score, digit_idx)) {
        return MORSE_FLIPPER_SCORE_WHEEL_SETTLE_MS - MORSE_FLIPPER_SCORE_WHEEL_SETTLE_JITTER_MS;
    }

    return MORSE_FLIPPER_SCORE_WHEEL_SETTLE_MS + MORSE_FLIPPER_SCORE_WHEEL_SETTLE_JITTER_MS;
}

static uint16_t morse_flipper_score_digit_wheel_ms(
    const MorseFlipperApp* app,
    uint8_t score,
    uint8_t digit_idx) {
    return (uint16_t)(morse_flipper_score_wheel_random_ms(app, score, digit_idx) +
                      morse_flipper_score_wheel_settle_ms(app, score, digit_idx));
}

static uint16_t morse_flipper_session_score_wheel_ms(const MorseFlipperApp* app, uint8_t score) {
    uint16_t left_ms;
    uint16_t right_ms;

    if(score >= 100U) return 0U;

    left_ms = morse_flipper_score_digit_wheel_ms(app, score, 0U);
    right_ms = morse_flipper_score_digit_wheel_ms(app, score, 1U);
    return left_ms > right_ms ? left_ms : right_ms;
}

static int8_t
    morse_flipper_score_wheel_dir(const MorseFlipperApp* app, uint8_t score, uint8_t digit_idx) {
    int8_t left_dir = (morse_flipper_score_wheel_seed(app, score, 0U) & 1U) != 0U ? 1 : -1;
    return digit_idx == 0U ? left_dir : (int8_t)-left_dir;
}

static int32_t morse_flipper_lerp_i32(int32_t from, int32_t to, uint32_t elapsed, uint16_t ms) {
    if(ms == 0U || elapsed >= ms) return to;
    return from + (((to - from) * (int32_t)elapsed) / (int32_t)ms);
}

static int32_t morse_flipper_floor_div_256(int32_t v) {
    if(v >= 0) return v / 256;
    return -(((-v) + 255) / 256);
}

static int32_t morse_flipper_ceil_div_256(int32_t v) {
    if(v >= 0) return (v + 255) / 256;
    return -((-v) / 256);
}

static uint8_t morse_flipper_mod10_i32(int32_t v) {
    int32_t r = v % 10;
    if(r < 0) r += 10;
    return (uint8_t)r;
}

static int32_t morse_flipper_score_wheel_pos_x256(
    const MorseFlipperApp* app,
    uint8_t score,
    uint8_t digit_idx,
    uint8_t target_digit,
    uint32_t elapsed) {
    int8_t dir = morse_flipper_score_wheel_dir(app, score, digit_idx);
    uint16_t random_ms = morse_flipper_score_wheel_random_ms(app, score, digit_idx);
    uint16_t settle_ms = morse_flipper_score_wheel_settle_ms(app, score, digit_idx);
    uint8_t seed = morse_flipper_score_wheel_seed(app, score, digit_idx);
    uint8_t random_steps = digit_idx == 0U ? (uint8_t)(2U + ((seed >> 1) & 1U)) :
                                             (uint8_t)(4U + ((seed >> 1) & 1U));
    uint8_t settle_steps = 1U;
    int32_t target = (int32_t)target_digit * 256;
    int32_t settle_from = target - ((int32_t)dir * (int32_t)settle_steps * 256);
    int32_t random_from = target - ((int32_t)dir * (int32_t)(random_steps + settle_steps) * 256);

    if(elapsed < random_ms) {
        return morse_flipper_lerp_i32(random_from, settle_from, elapsed, random_ms);
    }

    return morse_flipper_lerp_i32(settle_from, target, elapsed - random_ms, settle_ms);
}

static uint8_t morse_flipper_score_digit_slot_w(Canvas* canvas) {
    uint8_t max_w = 0U;

    for(uint8_t i = 0U; i < 10U; i++) {
        char s[2] = {(char)('0' + i), '\0'};
        uint8_t w = (uint8_t)canvas_string_width(canvas, s);
        if(w > max_w) max_w = w;
    }

    return (uint8_t)(max_w + 1U);
}

static void morse_flipper_draw_score_digit_at(
    Canvas* canvas,
    int16_t slot_cx,
    int16_t baseline_y,
    uint8_t digit) {
    char s[2] = {(char)('0' + digit), '\0'};
    int16_t x = slot_cx - (int16_t)(canvas_string_width(canvas, s) / 2U);

    canvas_draw_str(canvas, x, baseline_y, s);
}

static void morse_flipper_draw_score_wheel_digit(
    Canvas* canvas,
    const MorseFlipperApp* app,
    uint8_t score,
    uint8_t digit_idx,
    uint8_t target_digit,
    int16_t slot_cx,
    int16_t baseline_y,
    uint8_t pitch,
    uint32_t elapsed) {
    int32_t pos = morse_flipper_score_wheel_pos_x256(app, score, digit_idx, target_digit, elapsed);
    int8_t dir = morse_flipper_score_wheel_dir(app, score, digit_idx);

    if(dir > 0) {
        int32_t whole = morse_flipper_floor_div_256(pos);
        int32_t frac = pos - (whole * 256);
        int16_t off = (int16_t) - ((frac * pitch) / 256);

        morse_flipper_draw_score_digit_at(
            canvas, slot_cx, baseline_y + off, morse_flipper_mod10_i32(whole));
        morse_flipper_draw_score_digit_at(
            canvas, slot_cx, baseline_y + off + pitch, morse_flipper_mod10_i32(whole + 1));
    } else {
        int32_t whole = morse_flipper_ceil_div_256(pos);
        int32_t frac = (whole * 256) - pos;
        int16_t off = (int16_t)((frac * pitch) / 256);

        morse_flipper_draw_score_digit_at(
            canvas, slot_cx, baseline_y + off, morse_flipper_mod10_i32(whole));
        morse_flipper_draw_score_digit_at(
            canvas, slot_cx, baseline_y + off - pitch, morse_flipper_mod10_i32(whole - 1));
    }
}

static void
    morse_flipper_mask_score_window(Canvas* canvas, int16_t x, int16_t y, uint8_t w, uint8_t h) {
    uint8_t canvas_h = (uint8_t)canvas_height(canvas);

    canvas_set_color(canvas, ColorWhite);
    if(y > 0) canvas_draw_box(canvas, x, 0, w, (uint8_t)y);
    if((int16_t)(y + h) < canvas_h) {
        canvas_draw_box(canvas, x, y + h, w, (uint8_t)(canvas_h - (y + h)));
    }
    canvas_set_color(canvas, ColorBlack);
}

void morse_flipper_draw_session_end(Canvas* canvas, const MorseFlipperApp* app) {
    enum {
        ScoreCenterBaselineY = 42U,
        ScoreBaselineY = 32U,
        StarY = 43U,
        StarStartX = 42U,
        StarGapX = 22U,
        ScoreDigitGapX = 0U,
    };
    char digits[4];
    uint8_t x;
    uint8_t i;
    uint8_t slot_w;
    uint8_t score_w;
    uint8_t font_h;
    uint8_t font_desc;
    int16_t window_top;
    int16_t score_x;
    uint8_t stars;
    uint8_t score = morse_flipper_session_final_percent(app);
    int16_t score_y = ScoreBaselineY;
    uint32_t elapsed = 0U;
    uint32_t now_ms = furi_get_tick();
    uint32_t star_started_at;
    uint16_t score_wheel_ms;
    uint16_t anim_duration;
    bool stars_visible;
    bool show_message;
    bool blink_stars;
    bool blink_on = true;
    const CanvasFontParameters* font_params;

    if(canvas == NULL || app == NULL) return;

    if(app->star_anim_started_at != 0U) elapsed = now_ms - app->star_anim_started_at;

    canvas_set_font(canvas, FontBigNumbers);
    font_params = canvas_get_font_params(canvas, FontBigNumbers);
    font_h = font_params == NULL ? 18U : font_params->height;
    font_desc = font_params == NULL ? 0U : font_params->descender;

    stars = morse_flipper_progress_stars(score);
    score_wheel_ms = morse_flipper_session_score_wheel_ms(app, score);
    star_started_at = app->star_anim_started_at == 0U ?
                          0U :
                          app->star_anim_started_at + score_wheel_ms +
                              MORSE_FLIPPER_SCORE_SETTLED_PAUSE_MS + MORSE_FLIPPER_SCORE_SLIDE_MS;
    anim_duration = morse_flipper_star_anim_duration(stars);
    if(app->star_anim_started_at != 0U) {
        if(elapsed < score_wheel_ms + MORSE_FLIPPER_SCORE_SETTLED_PAUSE_MS) {
            score_y = ScoreCenterBaselineY;
        } else if(
            elapsed <
            score_wheel_ms + MORSE_FLIPPER_SCORE_SETTLED_PAUSE_MS + MORSE_FLIPPER_SCORE_SLIDE_MS) {
            uint32_t slide_elapsed =
                elapsed - score_wheel_ms - MORSE_FLIPPER_SCORE_SETTLED_PAUSE_MS;
            uint8_t slide_delta = ScoreCenterBaselineY - ScoreBaselineY;
            score_y = ScoreCenterBaselineY -
                      (int16_t)((slide_elapsed * slide_delta) / MORSE_FLIPPER_SCORE_SLIDE_MS);
        }
    }

    if(score >= 100U) {
        snprintf(digits, sizeof(digits), "%u", (unsigned)score);
        x = (uint8_t)(64U - (canvas_string_width(canvas, digits) / 2U));
        canvas_draw_str(canvas, x, score_y, digits);
    } else {
        slot_w = morse_flipper_score_digit_slot_w(canvas);
        score_w = (uint8_t)((slot_w * 2U) + ScoreDigitGapX);
        score_x = (int16_t)(64U - (score_w / 2U));
        window_top = score_y - (int16_t)(font_h - font_desc);

        if(app->star_anim_started_at != 0U && elapsed < score_wheel_ms) {
            morse_flipper_draw_score_wheel_digit(
                canvas,
                app,
                score,
                0U,
                (uint8_t)(score / 10U),
                (int16_t)(score_x + (slot_w / 2U)),
                score_y,
                (uint8_t)(font_h + 2U),
                elapsed);
            morse_flipper_draw_score_wheel_digit(
                canvas,
                app,
                score,
                1U,
                (uint8_t)(score % 10U),
                (int16_t)(score_x + slot_w + ScoreDigitGapX + (slot_w / 2U)),
                score_y,
                (uint8_t)(font_h + 2U),
                elapsed);
        } else {
            morse_flipper_draw_score_digit_at(
                canvas, (int16_t)(score_x + (slot_w / 2U)), score_y, (uint8_t)(score / 10U));
            morse_flipper_draw_score_digit_at(
                canvas,
                (int16_t)(score_x + slot_w + ScoreDigitGapX + (slot_w / 2U)),
                score_y,
                (uint8_t)(score % 10U));
        }
        morse_flipper_mask_score_window(
            canvas, score_x, window_top, score_w, (uint8_t)(font_h + 1U));
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignCenter, "Final score");

    stars_visible = app->star_anim_started_at == 0U ||
                    elapsed >= score_wheel_ms + MORSE_FLIPPER_SCORE_SETTLED_PAUSE_MS +
                                   MORSE_FLIPPER_SCORE_SLIDE_MS;
    show_message = app->star_anim_started_at == 0U ||
                   now_ms - app->star_anim_started_at >=
                       score_wheel_ms + MORSE_FLIPPER_SCORE_SETTLED_PAUSE_MS +
                           MORSE_FLIPPER_SCORE_SLIDE_MS + anim_duration;
    blink_stars = stars_visible && score >= 99U && stars != 0U && star_started_at != 0U &&
                  now_ms - star_started_at >= anim_duration;
    if(blink_stars) {
        uint32_t blink_ms = now_ms - star_started_at - anim_duration;
        blink_on = ((blink_ms / MORSE_FLIPPER_STAR_BLINK_HALF_MS) & 1U) == 0U;
    }

    for(i = 0U; i < 3U; i++) {
        uint8_t cols = 0U;
        if(!stars_visible) continue;
        if(star_started_at != 0U) {
            cols = blink_stars && !blink_on ?
                       0U :
                       morse_flipper_star_anim_cols(star_started_at, now_ms, i, stars);
        } else {
            cols = morse_flipper_star_anim_cols(star_started_at, now_ms, i, stars);
        }
        morse_flipper_draw_star_glyph_large_cols(
            canvas, (uint8_t)(StarStartX + (i * StarGapX)), StarY, cols);
    }

    if(show_message) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, 57, AlignCenter, AlignCenter, morse_flipper_session_end_blurb(app));
    }
}
static char morse_flipper_upper_char(char ch) {
    if(ch >= 'a' && ch <= 'z') return (char)(ch - ('a' - 'A'));
    return ch;
}

static void
    morse_flipper_session_draw_cell(Canvas* canvas, uint8_t center, uint8_t y, uint8_t ch) {
    char s[2];
    uint8_t w;
    uint8_t x;

    if(canvas == NULL || ch == 0U) return;

    if(morse_flipper_cw_token_is_private(ch)) ch = '#';

    s[0] = (char)ch;
    s[1] = '\0';
    w = canvas_string_width(canvas, s);
    x = (uint8_t)(center - (w / 2U));
    canvas_draw_str(canvas, x, y, s);
}

static void morse_flipper_session_draw_inverted_cell(
    Canvas* canvas,
    uint8_t center,
    uint8_t y,
    uint8_t box_y,
    uint8_t box_h,
    uint8_t ch) {
    char s[2];
    char w_txt[2] = {'W', '\0'};
    uint8_t box_w;
    uint8_t w;
    uint8_t box_x;
    uint8_t x;

    if(canvas == NULL || ch == 0U) return;

    if(morse_flipper_cw_token_is_private(ch)) ch = '#';

    s[0] = (char)ch;
    s[1] = '\0';
    box_w = (uint8_t)(canvas_string_width(canvas, w_txt) + 2U);
    w = canvas_string_width(canvas, s);
    box_x = (uint8_t)(center - (box_w / 2U));
    x = (uint8_t)(center - (w / 2U));
    canvas_draw_box(canvas, box_x, box_y, box_w, box_h);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, x, y, s);
    canvas_set_color(canvas, ColorBlack);
}

static uint8_t morse_flipper_session_slot_centers(uint8_t size, uint8_t* out) {
    static const uint8_t slot_pos[9][9] = {
        {64},
        {52, 76},
        {40, 64, 88},
        {31, 53, 75, 97},
        {24, 44, 64, 84, 104},
        {19, 37, 55, 73, 91, 109},
        {16, 32, 48, 64, 80, 96, 112},
        {13, 27, 41, 55, 73, 87, 101, 115},
        {11, 24, 37, 50, 64, 78, 91, 104, 117},
    };
    uint8_t i;

    if(size < 1U) size = 1U;
    if(size > 9U) size = 9U;
    if(out == NULL) return size;

    for(i = 0U; i < size; i++) {
        out[i] = slot_pos[size - 1U][i];
    }

    return size;
}

static void morse_flipper_session_answer_text(
    const MorseFlipperApp* app,
    char* out,
    size_t out_sz,
    uint8_t max_chars) {
    char preview = 0;
    size_t wi = 0U;
    size_t i;

    if(out == NULL || out_sz == 0U) return;
    out[0] = '\0';
    if(app == NULL || max_chars == 0U) return;

    if(morse_trainer_phase(&app->trainer) == MorseTrainerPhaseDone &&
       morse_trainer_reveal(&app->trainer)[0] != '\0') {
        uint8_t wi = 0U;
        uint8_t i = 0U;

        while(morse_trainer_reveal(&app->trainer)[i] != '\0' && wi + 1U < out_sz &&
              wi < max_chars) {
            out[wi++] = morse_trainer_reveal(&app->trainer)[i++];
        }
        out[wi] = '\0';
        return;
    }

    for(i = 0U; app->rf_tx_text[i] != '\0' && wi + 1U < out_sz && wi < max_chars; i++) {
        char ch = morse_flipper_upper_char(app->rf_tx_text[i]);

        if(ch == ' ' || ch == '|') continue;
        out[wi++] = ch;
    }

    preview = morse_flipper_upper_char(morse_flipper_cw_decoder_preview(&app->tx_decoder));
    if(preview != 0 && preview != ' ' && preview != '|' && wi + 1U < out_sz && wi < max_chars) {
        out[wi++] = preview;
    }

    out[wi] = '\0';
}

static uint8_t morse_flipper_session_prompt_count(const MorseFlipperApp* app) {
    uint8_t size;
    uint8_t shown;

    if(app == NULL) return 0U;

    size = morse_trainer_group_size(&app->trainer);
    if(size < 1U) size = 1U;
    if(size > 9U) size = 9U;

    if(morse_flipper_session_idle_view(app)) return 0U;
    if(!app->trainer_playback_active) return size;

    shown = app->trainer_char_idx;
    if(shown < size) shown++;
    if(shown > size) shown = size;
    return shown;
}

static uint8_t morse_flipper_session_answer_count(const char* answer) {
    uint8_t n = 0U;
    uint8_t i;

    if(answer == NULL) return 0U;

    for(i = 0U; answer[i] != '\0'; i++) {
        if(answer[i] == ' ' || answer[i] == '|') continue;
        n++;
    }

    return n;
}

static void morse_flipper_session_answer_committed_text(
    const MorseFlipperApp* app,
    char* out,
    size_t out_sz,
    uint8_t max_chars) {
    size_t wi = 0U;
    uint8_t i = 0U;

    if(out == NULL || out_sz == 0U) return;
    out[0] = '\0';
    if(app == NULL || max_chars == 0U) return;

    for(i = 0U; app->rf_tx_text[i] != '\0' && wi + 1U < out_sz && wi < max_chars; i++) {
        char ch = morse_flipper_upper_char(app->rf_tx_text[i]);

        if(ch == ' ' || ch == '|') continue;
        out[wi++] = ch;
    }

    out[wi] = '\0';
}

static uint8_t morse_flipper_session_text_hits(const char* want, const char* got) {
    uint8_t hit = 0U;
    uint8_t i = 0U;

    if(want == NULL || got == NULL) return 0U;

    while(want[i] != '\0' && got[i] != '\0') {
        if(morse_flipper_upper_char(want[i]) == morse_flipper_upper_char(got[i])) hit++;
        i++;
    }

    return hit;
}

static void morse_flipper_session_title(const MorseFlipperApp* app, char* out, size_t out_sz) {
    const char* chars;
    size_t i;
    size_t wi = 0U;
    size_t len;
    char lesson_label[32];

    if(out == NULL || out_sz < 2U) return;
    out[0] = '\0';
    if(app == NULL) return;
    if(morse_trainer_session_completed(&app->trainer)) {
        snprintf(out, out_sz, "Training complete");
        return;
    }

    if(morse_flipper_effective_trainer_custom_set_idx(app) == 0U) {
        morse_trainer_lesson_label(
            morse_trainer_lesson(&app->trainer), lesson_label, sizeof(lesson_label));
        snprintf(out, out_sz, "Lesson %s", lesson_label);
        return;
    }

    chars = morse_trainer_charset(&app->trainer);
    len = strlen(chars);
    if(len > 12U) {
        snprintf(
            out, out_sz, "%s", app->trainer.custom_name[0] ? app->trainer.custom_name : chars);
        return;
    }

    for(i = 0U; chars[i] != '\0' && wi + 2U < out_sz; i++) {
        if(i != 0U && wi + 2U < out_sz) out[wi++] = ' ';
        out[wi++] = morse_flipper_upper_char(chars[i]);
    }
    out[wi] = '\0';
}

void morse_flipper_draw_session_rows(Canvas* canvas, const MorseFlipperApp* app) {
    uint8_t size = morse_trainer_group_size(&app->trainer);
    uint8_t centers[9];
    char answers[MORSE_TRAINER_GROUP_CAP];
    char deleted[MORSE_TRAINER_GROUP_CAP];
    const char* group = morse_trainer_last_group(&app->trainer);
    uint8_t top_y;
    uint8_t bot_y;
    uint8_t text_h;
    uint8_t wrong_box_h;
    size_t ans_len;
    size_t deleted_len = 0U;
    uint8_t prompt_count;
    uint8_t answer_count;
    Font row_font;
    uint8_t i;
    bool idle = morse_flipper_session_idle_view(app);
    bool rep = morse_flipper_session_repeat_active(app);
    bool flash_deleted = false;
    bool done;
    uint32_t now_ms;

    if(size < 1U) size = 1U;
    if(size > 9U) size = 9U;

    if(size <= 6U) {
        row_font = FontPrimary;
        top_y = 15U;
        bot_y = 29U;
        text_h = 11U;
    } else if(size <= 8U) {
        row_font = FontSecondary;
        top_y = 16U;
        bot_y = 28U;
        text_h = 8U;
    } else {
        row_font = FontKeyboard;
        top_y = 16U;
        bot_y = 27U;
        text_h = 9U;
    }
    wrong_box_h = (uint8_t)(text_h + 2U);
    morse_flipper_session_slot_centers(size, centers);
    morse_flipper_session_answer_text(app, answers, sizeof(answers), size);
    deleted[0] = '\0';
    now_ms = furi_get_tick();
    if(morse_flipper_session_answer_flash_visible(app, now_ms)) {
        strlcpy(deleted, app->session_deleted_text, sizeof(deleted));
        deleted_len = strlen(deleted);
        flash_deleted = true;
    }
    ans_len = strlen(answers);
    prompt_count = morse_flipper_session_prompt_count(app);
    answer_count = morse_flipper_session_answer_count(answers);
    done = !idle && app->session_started && !app->trainer_playback_active &&
           morse_trainer_phase(&app->trainer) == MorseTrainerPhaseDone;

    if(idle) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "Listening");
        canvas_set_font(canvas, FontSecondary);
        if(app->input_source == MorseFlipperInputSourceButtons) {
            canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "Press OK to start");
        } else {
            canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Press OK to start");
            canvas_draw_str_aligned(
                canvas, 64, 44, AlignCenter, AlignCenter, "Press your key to start");
        }
        return;
    }

    canvas_set_font(canvas, row_font);

    for(i = 0U; i < size; i++) {
        char q = '\0';
        char a = '\0';
        char flash = '\0';
        char top = '_';
        char bot = '_';
        bool have = i < ans_len;
        bool ok = false;
        bool bad = false;

        if(group[i] != '\0') q = morse_flipper_upper_char(group[i]);
        if(have) a = morse_flipper_upper_char(answers[i]);
        if(flash_deleted && i >= ans_len && i < deleted_len)
            flash = morse_flipper_upper_char(deleted[i]);
        if(q != '\0' && a != '\0' && q == a) ok = true;
        if(done && q != '\0' && i < answer_count && !ok) bad = true;

        if(i >= prompt_count) continue;

        if(app->trainer_playback_active || idle) {
            morse_flipper_session_draw_cell(canvas, centers[i], top_y, (uint8_t)top);
            continue;
        }

        if(rep || done) {
            if(done && q != '\0') {
                top = q;
            }
            if(have && a != '\0') {
                bot = a;
            } else if(flash != '\0') {
                bot = flash;
            }

            morse_flipper_session_draw_cell(canvas, centers[i], top_y, (uint8_t)top);

            if(bad) {
                morse_flipper_session_draw_inverted_cell(
                    canvas,
                    centers[i],
                    bot_y,
                    (uint8_t)(bot_y - text_h + 1U),
                    wrong_box_h,
                    (uint8_t)bot);
            } else {
                morse_flipper_session_draw_cell(canvas, centers[i], bot_y, (uint8_t)bot);
            }
            continue;
        }

        morse_flipper_session_draw_cell(canvas, centers[i], top_y, (uint8_t)top);
    }
}

void morse_flipper_draw_session_bottom(Canvas* canvas, const MorseFlipperApp* app) {
    char lesson_line[48];
    char progress_line[16];
    char pct_line[16];
    char wait_line[16];
    char live_answer[MORSE_TRAINER_GROUP_CAP];
    uint8_t size = morse_trainer_group_size(&app->trainer);
    uint8_t asked = 0U;
    uint8_t total = morse_trainer_session_total(&app->trainer);
    uint8_t fill = 0U;
    uint16_t letter_hits;
    uint16_t letter_total;
    uint8_t letter_pct;
    uint8_t live_count = 0U;
    uint8_t x;
    Font title_font = FontSecondary;
    bool wait = false;
    uint32_t now_ms;
    uint32_t left_ms;
    uint16_t secs;

    morse_flipper_session_title(app, lesson_line, sizeof(lesson_line));
    now_ms = furi_get_tick();
    wait = app->session_result_hold && app->session_next_group_at > now_ms;
    if(morse_flipper_session_idle_view(app)) return;

    if(app->session_started) {
        asked = morse_trainer_session_index(&app->trainer);
        if(asked == 1U && !app->trainer_playback_active && !app->session_round_pending &&
           morse_trainer_phase(&app->trainer) == MorseTrainerPhaseListen)
            asked = 0U;
    }

    if(total != 0U && asked != 0U) fill = (uint8_t)(((uint16_t)asked * 100U) / total);
    letter_hits = morse_trainer_session_letter_hits(&app->trainer);
    letter_total = morse_trainer_session_letter_total(&app->trainer);
    if(app->session_round_pending && !app->trainer_playback_active &&
       morse_trainer_phase(&app->trainer) == MorseTrainerPhaseRepeat) {
        morse_flipper_session_answer_committed_text(app, live_answer, sizeof(live_answer), size);
        live_count = morse_flipper_session_answer_count(live_answer);
        letter_hits =
            (uint16_t)(letter_hits + morse_flipper_session_text_hits(
                                         morse_trainer_last_group(&app->trainer), live_answer));
        letter_total = (uint16_t)(letter_total + live_count);
    }
    if(letter_total != 0U && letter_hits > letter_total) letter_hits = letter_total;
    letter_pct = letter_total == 0U ? 100U :
                                      (uint8_t)(((uint32_t)letter_hits * 100U) / letter_total);

    snprintf(progress_line, sizeof(progress_line), "%u/%u", (unsigned)asked, (unsigned)total);
    snprintf(pct_line, sizeof(pct_line), "%u%%", (unsigned)letter_pct);

    canvas_set_font(canvas, FontSecondary);
    if(canvas_string_width(canvas, lesson_line) > 128U) {
        title_font = FontKeyboard;
        canvas_set_font(canvas, title_font);
    }

    x = (uint8_t)((128 - canvas_string_width(canvas, lesson_line)) / 2);
    canvas_draw_str(canvas, x, 42, lesson_line);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 51, pct_line);
    if(wait) {
        left_ms = app->session_next_group_at - now_ms;
        secs = (uint16_t)((left_ms + 999U) / 1000U);
        if(secs == 0U) secs = 1U;
        snprintf(wait_line, sizeof(wait_line), "%u", (unsigned)secs);
        x = (uint8_t)((128 - canvas_string_width(canvas, wait_line)) / 2);
        canvas_draw_str(canvas, x, 51, wait_line);
    }
    x = (uint8_t)(128 - canvas_string_width(canvas, progress_line));
    canvas_draw_str(canvas, x, 51, progress_line);

    canvas_draw_frame(canvas, 13, 57, 102, 5);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_dot(canvas, 13, 57);
    canvas_draw_dot(canvas, 114, 57);
    canvas_draw_dot(canvas, 13, 61);
    canvas_draw_dot(canvas, 114, 61);
    canvas_set_color(canvas, ColorBlack);
    if(fill != 0U) canvas_draw_box(canvas, 14, 58, fill, 3);
}
