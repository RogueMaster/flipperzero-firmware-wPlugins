/*
 * Purpose: Run LCWO-style listen/repeat training sessions.
 * Owns: session state resets, answer text, scoring, and prompt progression.
 * Depends on: morse_flipper_app_i.h, trainer.h, and CW token helpers.
 * Tests: tests/test_trainer.c covers the host trainer core.
 */

#include "morse_flipper_app_i.h"
#include "morse_flipper_cw_token.h"

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
    app->session_result_until = 0U;
    app->session_next_group_at = 0U;
    app->session_complete_at = 0U;
    app->session_wait_draw_s = 0xFFU;
    app->session_end_flash_phase = 0U;
}

void morse_flipper_reset_session_state(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    app->trainer_playback_active = false;
    app->trainer_playback_mark = false;

    morse_flipper_drop_live_keying_for_playback(app, now_ms);
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

    morse_flipper_reset_session_state(app, now_ms);
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
            morse_flipper_begin_group_playback(app, now_ms);
        } else {
            morse_flipper_view_dirty(app);
        }
        return;
    }

    if(morse_trainer_session_completed(&app->trainer) ||
       (morse_trainer_session_aborted(&app->trainer) &&
        morse_trainer_phase(&app->trainer) == MorseTrainerPhaseDone)) {
        if(app->session_complete_at == 0U) app->session_complete_at = now_ms;
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
        morse_flipper_queue_session_feedback(app, now_ms);
        return;
    }
    app->session_answer_complete_at = 0U;

    dt = (uint32_t)app->trainer_answer_timeout_s * 1000U;
    if(dt == 0U) dt = (uint32_t)MORSE_FLIPPER_TRAINER_TIMEOUT_DEFAULT_S * 1000U;
    if(now_ms - app->session_last_input_at < dt) return;

    morse_trainer_score_repeat_text(&app->trainer, ans);
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

bool morse_flipper_session_end_flash(const MorseFlipperApp* app) {
    return app != NULL && morse_flipper_session_final_percent(app) > 95U;
}

static const char* morse_flipper_session_end_blurb(const MorseFlipperApp* app) {
    if(app == NULL) return "Keep practicing";
    if(morse_trainer_lesson(&app->trainer) >= 40U) return "Congratulations!";
    if(morse_flipper_session_final_percent(app) >= 90U) return "Try the next lesson";
    return "Keep practicing";
}

void morse_flipper_draw_session_end(Canvas* canvas, const MorseFlipperApp* app) {
    char digits[4];
    bool flash_on = false;
    uint8_t x;
    uint8_t score = morse_flipper_session_final_percent(app);

    if(canvas == NULL || app == NULL) return;

    if(morse_flipper_session_end_flash(app) && app->session_end_flash_phase != 0U) {
        canvas_draw_box(canvas, 0, 0, 128, 64);
        canvas_set_color(canvas, ColorWhite);
        flash_on = true;
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignCenter, "Final score");
    snprintf(digits, sizeof(digits), "%u", (unsigned)score);
    canvas_set_font(canvas, FontBigNumbers);
    x = (uint8_t)(64U - (canvas_string_width(canvas, digits) / 2U));
    canvas_draw_str(canvas, x, 39, digits);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 64, 54, AlignCenter, AlignCenter, morse_flipper_session_end_blurb(app));

    if(flash_on) canvas_set_color(canvas, ColorBlack);
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
    const char* group = morse_trainer_last_group(&app->trainer);
    uint8_t top_y;
    uint8_t bot_y;
    uint8_t text_h;
    uint8_t wrong_box_h;
    size_t ans_len;
    uint8_t prompt_count;
    uint8_t answer_count;
    Font row_font;
    uint8_t i;
    bool idle = morse_flipper_session_idle_view(app);
    bool rep = morse_flipper_session_repeat_active(app);
    bool done;

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
        char top = '_';
        char bot = '_';
        bool have = i < ans_len;
        bool ok = false;
        bool bad = false;

        if(group[i] != '\0') q = morse_flipper_upper_char(group[i]);
        if(have) a = morse_flipper_upper_char(answers[i]);
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
