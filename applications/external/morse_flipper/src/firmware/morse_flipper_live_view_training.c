/*
 * Purpose: Draw LCWO, straight trainer, and TX Groups training screens.
 * Owns: training score lines, answer prompts, and large glyph layouts.
 * Depends on: morse_flipper_app_i.h, trainer state, and prompt font data.
 * Tests: trainer host tests cover data; rendering is hardware-only.
 */

#include "fonts/morse_flipper_terminus24.h"
#include "morse_flipper_app_i.h"

static void morse_flipper_txg_score_line(const MorseFlipperApp* app, char* out, size_t out_sz) {
    unsigned pct;

    if(out == NULL || out_sz == 0U) return;
    pct = app != NULL && app->txg_session_total != 0U ?
              ((unsigned)app->txg_session_good * 100U) / app->txg_session_total :
              0U;
    snprintf(
        out,
        out_sz,
        "%u/%u  %u%%",
        app ? (unsigned)app->txg_session_good : 0U,
        app ? (unsigned)app->txg_session_total : 0U,
        pct);
}

static void morse_flipper_txg_score_pct(const MorseFlipperApp* app, char* out, size_t out_sz) {
    unsigned pct;

    if(out == NULL || out_sz == 0U) return;
    pct = app != NULL && app->txg_session_total != 0U ?
              ((unsigned)app->txg_session_good * 100U) / app->txg_session_total :
              0U;
    if(pct > 100U) pct = 100U;
    snprintf(out, out_sz, "%u%%", pct);
}

static void morse_flipper_draw_txg_big_slots(Canvas* canvas, int32_t cy, const char* text) {
    const int32_t gap = 3;
    const int32_t cell = (int32_t)MORSE_FLIPPER_TERMINUS24_WIDTH;
    const int32_t total = (cell * 5) + (gap * 4);
    int32_t cx = ((128 - total) / 2) + (cell / 2);

    if(canvas == NULL || text == NULL) return;

    for(uint8_t i = 0U; i < MORSE_FLIPPER_TX_GROUP_LEN; i++) {
        if(text[i] == '\0') break;
        morse_flipper_draw_straight_prompt(canvas, cx + ((cell + gap) * (int32_t)i), cy, text[i]);
    }
}

static void
    morse_flipper_txg_answer_with_preview(const MorseFlipperApp* app, char* out, size_t out_sz) {
    uint8_t n = 0U;
    char preview;

    if(out == NULL || out_sz == 0U) return;
    out[0] = '\0';
    if(app == NULL) return;

    while(n < MORSE_FLIPPER_TX_GROUP_LEN && n + 1U < out_sz && app->tx_group.answer[n] != '\0') {
        out[n] = app->tx_group.answer[n];
        n++;
    }
    out[n] = '\0';

    if(n >= MORSE_FLIPPER_TX_GROUP_LEN || n + 1U >= out_sz) return;
    if(app->screen != MorseFlipperScreenTxGroups || !app->txg_wait_answer) return;

    preview =
        (char)morse_flipper_live_upper_char(morse_flipper_cw_decoder_preview(&app->tx_decoder));
    if(preview == 0 || preview == ' ' || preview == '|') return;

    out[n++] = preview;
    out[n] = '\0';
}

static void morse_flipper_draw_tx_groups_practice(Canvas* canvas, MorseFlipperApp* app) {
    char answer[MORSE_FLIPPER_TX_GROUP_LEN + 1U];
    char score[8];
    uint8_t x;

    if(canvas == NULL || app == NULL) return;

    morse_flipper_draw_txg_big_slots(canvas, 18, app->tx_group.target);
    morse_flipper_draw_tx_history_divider(canvas, morse_flipper_live_left_hint(app));
    morse_flipper_txg_answer_with_preview(app, answer, sizeof(answer));
    morse_flipper_draw_txg_big_slots(canvas, 49, answer);

    canvas_set_font(canvas, FontSecondary);
    morse_flipper_txg_score_pct(app, score, sizeof(score));
    x = (uint8_t)(126U - canvas_string_width(canvas, score));
    canvas_draw_str(canvas, x, 64, score);
}

static void
    morse_flipper_draw_txg_label(Canvas* canvas, int32_t x, int32_t y, const char* s, bool bad) {
    uint16_t w;

    if(!bad) {
        canvas_draw_str(canvas, x, y, s);
        return;
    }

    w = canvas_string_width(canvas, s);
    canvas_draw_box(canvas, x - 1, y - 7, w + 2, 8);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, x, y, s);
    canvas_set_color(canvas, ColorBlack);
}

static void morse_flipper_draw_txg_metric(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    const char* label,
    const char* value,
    bool bad) {
    morse_flipper_draw_txg_label(canvas, x, y, label, bad);
    canvas_draw_str(canvas, x + 28, y, value);
}

static uint8_t morse_flipper_txg_countdown_s(const MorseFlipperApp* app) {
    uint32_t now;
    uint32_t left;

    if(app == NULL || app->txg_result_until == 0U) return 0U;
    now = furi_get_tick();
    if(now >= app->txg_result_until) return 0U;
    left = app->txg_result_until - now;
    return (uint8_t)((left + 999U) / 1000U);
}

static void morse_flipper_draw_tx_groups_result(Canvas* canvas, MorseFlipperApp* app) {
    char a[12];
    char b[12];
    char c[4];
    const MorseFlipperTxGroupResult* r;

    if(canvas == NULL || app == NULL) return;
    r = &app->tx_group.result;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 7, AlignCenter, AlignCenter, r->passed ? "OK" : "Fail");
    canvas_set_font(canvas, FontKeyboard);

    snprintf(a, sizeof(a), "%u/5", (unsigned)r->correct);
    morse_flipper_draw_txg_metric(canvas, 1, 20, "Corr", a, !r->correct_pass);
    snprintf(a, sizeof(a), "%u%%", (unsigned)r->speed_pct);
    morse_flipper_draw_txg_metric(canvas, 1, 29, "Time", a, !r->speed_pass);
    snprintf(a, sizeof(a), "%u%%", (unsigned)r->letter_gap_pct);
    morse_flipper_draw_txg_metric(canvas, 1, 38, "LGap", a, !r->letter_gap_pass);

    if(app->tx_group.sk) {
        snprintf(
            a,
            sizeof(a),
            "%u.%02u",
            (unsigned)(r->ratio_x100 / 100U),
            (unsigned)(r->ratio_x100 % 100U));
        morse_flipper_draw_txg_metric(canvas, 64, 20, "Rtio", a, !r->ratio_pass);
        snprintf(a, sizeof(a), "%u%%", (unsigned)r->accuracy_pct);
        morse_flipper_draw_txg_metric(canvas, 64, 29, "Acc", a, !r->accuracy_pass);
        snprintf(a, sizeof(a), "%u%%", (unsigned)r->dit_gap_pct);
        morse_flipper_draw_txg_metric(canvas, 64, 38, "DGap", a, !r->dit_gap_pass);
        snprintf(a, sizeof(a), "%u%%", (unsigned)r->variance_pct);
        morse_flipper_draw_txg_metric(canvas, 64, 47, "Var", a, !r->variance_pass);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 56, (r->fault && r->fault[0]) ? r->fault : "");
    snprintf(c, sizeof(c), "%u", (unsigned)morse_flipper_txg_countdown_s(app));
    canvas_draw_str(canvas, 2, 64, c);
    morse_flipper_txg_score_line(app, b, sizeof(b));
    canvas_draw_str(canvas, 126 - canvas_string_width(canvas, b), 64, b);
    if(app->input_source == MorseFlipperInputSourceButtons && !app->txg_sk)
        morse_flipper_draw_left_exit_hint(canvas);
}

static uint16_t morse_flipper_txg_avg_u16(uint32_t sum, uint16_t n) {
    if(n == 0U) return 0U;
    return (uint16_t)((sum + (n / 2U)) / n);
}

static void morse_flipper_draw_tx_groups_final(Canvas* canvas, MorseFlipperApp* app) {
    char v[24];
    uint16_t avg;
    uint8_t y = 20U;

    if(canvas == NULL || app == NULL) return;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignCenter, "Final score");
    canvas_set_font(canvas, FontKeyboard);

    snprintf(
        v, sizeof(v), "%u/%u", (unsigned)app->txg_session_good, (unsigned)app->txg_session_total);
    morse_flipper_draw_txg_metric(canvas, 1, y, "Pass", v, false);
    y += 9U;
    snprintf(
        v,
        sizeof(v),
        "%u%%",
        (unsigned)morse_flipper_txg_avg_u16(app->txg_sum_speed, app->txg_session_total));
    morse_flipper_draw_txg_metric(canvas, 1, y, "Time", v, false);
    y += 9U;
    snprintf(
        v,
        sizeof(v),
        "%u%%",
        (unsigned)morse_flipper_txg_avg_u16(app->txg_sum_lgap, app->txg_session_total));
    morse_flipper_draw_txg_metric(canvas, 1, y, "LGap", v, false);

    if(app->txg_session_sk != 0U) {
        avg = morse_flipper_txg_avg_u16(app->txg_sum_ratio, app->txg_session_sk);
        snprintf(v, sizeof(v), "%u.%02u", (unsigned)(avg / 100U), (unsigned)(avg % 100U));
        morse_flipper_draw_txg_metric(canvas, 64, 20, "Rtio", v, false);
        snprintf(
            v,
            sizeof(v),
            "%u%%",
            (unsigned)morse_flipper_txg_avg_u16(app->txg_sum_accuracy, app->txg_session_sk));
        morse_flipper_draw_txg_metric(canvas, 64, 29, "Acc", v, false);
        snprintf(
            v,
            sizeof(v),
            "%u%%",
            (unsigned)morse_flipper_txg_avg_u16(app->txg_sum_dgap, app->txg_session_sk));
        morse_flipper_draw_txg_metric(canvas, 64, 38, "DGap", v, false);
        snprintf(
            v,
            sizeof(v),
            "%u%%",
            (unsigned)morse_flipper_txg_avg_u16(app->txg_sum_variance, app->txg_session_sk));
        morse_flipper_draw_txg_metric(canvas, 64, 47, "Var", v, false);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 64, "Back exit");
    morse_flipper_txg_score_line(app, v, sizeof(v));
    canvas_draw_str(canvas, 126 - canvas_string_width(canvas, v), 64, v);
    if(app->input_source == MorseFlipperInputSourceButtons && !app->txg_sk)
        morse_flipper_draw_left_exit_hint(canvas);
}

static uint16_t morse_flipper_straight_symbol_ms(char elem, uint16_t dit_ms) {
    return (uint16_t)((elem == '-' ? 3U : 1U) * (uint32_t)dit_ms);
}

static uint32_t morse_flipper_straight_strip_total_ms(
    const char* code,
    const uint16_t* marks_ms,
    const uint16_t* spaces_ms,
    uint16_t dit_ms,
    bool use_ms) {
    uint32_t total;
    size_t i;

    if(code == NULL || code[0] == '\0') return 0U;
    if(dit_ms == 0U) dit_ms = MORSE_FLIPPER_DEFAULT_DIT_MS;

    total = (uint32_t)dit_ms * 2U;
    for(i = 0U; code[i] != '\0'; i++) {
        if(use_ms && marks_ms != NULL) {
            total += marks_ms[i];
        } else {
            total += morse_flipper_straight_symbol_ms(code[i], dit_ms);
        }

        if(code[i + 1U] == '\0') continue;

        if(use_ms && spaces_ms != NULL) {
            total += spaces_ms[i];
        } else {
            total += dit_ms;
        }
    }

    return total;
}

static uint8_t
    morse_flipper_straight_segment_px(uint32_t ms, uint32_t ref_ms, uint8_t w, bool visible) {
    uint32_t px;

    if(ref_ms == 0U) ref_ms = 1U;
    px = ((ms * (uint32_t)w) + (ref_ms / 2U)) / ref_ms;
    if(visible && ms != 0U && px == 0U) px = 1U;
    if(px > w) px = w;
    return (uint8_t)px;
}

static void morse_flipper_draw_straight_low_segment(
    Canvas* canvas,
    int32_t* pos,
    int32_t end_x,
    uint8_t low_y,
    uint8_t px) {
    int32_t next;

    if(canvas == NULL || pos == NULL || px == 0U || *pos >= end_x) return;

    next = *pos + px;
    if(next > end_x) next = end_x;
    canvas_draw_line(canvas, *pos, low_y, next, low_y);
    *pos = next;
}

static void morse_flipper_draw_straight_mark_segment(
    Canvas* canvas,
    int32_t* pos,
    int32_t end_x,
    uint8_t low_y,
    uint8_t hi_y,
    uint8_t px) {
    int32_t next;

    if(canvas == NULL || pos == NULL || px == 0U || *pos >= end_x) return;

    next = *pos + px;
    if(next > end_x) next = end_x;
    canvas_draw_line(canvas, *pos, low_y, *pos, hi_y);
    canvas_draw_line(canvas, *pos, hi_y, next, hi_y);
    *pos = next;
    canvas_draw_line(canvas, *pos, hi_y, *pos, low_y);
}

static void morse_flipper_draw_straight_strip(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t w,
    const char* code,
    const uint16_t* marks_ms,
    const uint16_t* spaces_ms,
    uint16_t dit_ms,
    uint32_t ref_ms,
    bool use_ms) {
    uint8_t low_y = y + 6U;
    uint8_t hi_y = y + 1U;
    int32_t pos;
    int32_t end_x;
    size_t i;

    if(canvas == NULL || code == NULL || code[0] == '\0') return;
    if(dit_ms == 0U) dit_ms = MORSE_FLIPPER_DEFAULT_DIT_MS;
    if(ref_ms == 0U)
        ref_ms = morse_flipper_straight_strip_total_ms(code, marks_ms, spaces_ms, dit_ms, use_ms);

    pos = x;
    end_x = (int32_t)x + w;

    morse_flipper_draw_straight_low_segment(
        canvas, &pos, end_x, low_y, morse_flipper_straight_segment_px(dit_ms, ref_ms, w, true));

    for(i = 0U; code[i] != '\0'; i++) {
        uint32_t ms;
        uint8_t px;

        if(use_ms && marks_ms != NULL) {
            ms = marks_ms[i];
        } else {
            ms = morse_flipper_straight_symbol_ms(code[i], dit_ms);
        }

        px = morse_flipper_straight_segment_px(ms, ref_ms, w, true);
        morse_flipper_draw_straight_mark_segment(canvas, &pos, end_x, low_y, hi_y, px);

        if(code[i + 1U] == '\0') break;

        if(use_ms && spaces_ms != NULL) {
            ms = spaces_ms[i];
        } else {
            ms = dit_ms;
        }

        px = morse_flipper_straight_segment_px(ms, ref_ms, w, ms != 0U);
        morse_flipper_draw_straight_low_segment(canvas, &pos, end_x, low_y, px);
    }

    morse_flipper_draw_straight_low_segment(
        canvas, &pos, end_x, low_y, morse_flipper_straight_segment_px(dit_ms, ref_ms, w, true));
}

static void morse_flipper_draw_straight_countdown(Canvas* canvas, const MorseFlipperApp* app) {
    char wait_txt[12];
    uint32_t now;
    uint32_t left_ms;

    if(canvas == NULL || app == NULL || app->straight_next_at == 0U) return;
    now = furi_get_tick();
    if(now >= app->straight_next_at) return;

    left_ms = app->straight_next_at - now;
    snprintf(wait_txt, sizeof(wait_txt), "%u", (unsigned)((left_ms + 999U) / 1000U));
    canvas_draw_str(canvas, 2, 64, wait_txt);
}

static void
    morse_flipper_straight_ratio_text(const MorseFlipperApp* app, char* out, size_t out_sz) {
    uint16_t ratio;

    if(out == NULL || out_sz == 0U) return;

    ratio = app != NULL ? morse_flipper_straight_trainer_ratio_x100(&app->straight_trainer) : 0U;
    if(ratio >= 600U) {
        snprintf(out, out_sz, "6+");
        return;
    }

    snprintf(out, out_sz, "%u.%02u", (unsigned)(ratio / 100U), (unsigned)(ratio % 100U));
}

static void morse_flipper_draw_straight_metrics(Canvas* canvas, const MorseFlipperApp* app) {
    char s_txt[8];
    char di_txt[8];
    char da_txt[8];
    char ra_txt[8];
    char cnt[24];
    unsigned pct = 0U;
    bool answer_empty;
    bool count_fail;
    uint8_t x;

    if(canvas == NULL || app == NULL) return;

    answer_empty = morse_flipper_straight_trainer_answer(&app->straight_trainer)[0] == '\0';
    count_fail = app->straight_done &&
                 (answer_empty ||
                  !morse_flipper_straight_trainer_symbol_count_match(&app->straight_trainer));

    if(app->straight_done && !count_fail) {
        snprintf(
            s_txt,
            sizeof(s_txt),
            "%s",
            morse_flipper_straight_trainer_worst_space_score(&app->straight_trainer) >= 90U ?
                "OK" :
                "");
        if(s_txt[0] == '\0')
            snprintf(
                s_txt,
                sizeof(s_txt),
                "%u",
                (unsigned)morse_flipper_straight_trainer_worst_space_score(
                    &app->straight_trainer));
        snprintf(
            di_txt,
            sizeof(di_txt),
            "%s",
            morse_flipper_straight_trainer_worst_dit_score(&app->straight_trainer) >= 90U ? "OK" :
                                                                                            "");
        if(di_txt[0] == '\0')
            snprintf(
                di_txt,
                sizeof(di_txt),
                "%u",
                (unsigned)morse_flipper_straight_trainer_worst_dit_score(&app->straight_trainer));
        snprintf(
            da_txt,
            sizeof(da_txt),
            "%s",
            morse_flipper_straight_trainer_worst_dah_score(&app->straight_trainer) >= 90U ? "OK" :
                                                                                            "");
        if(da_txt[0] == '\0')
            snprintf(
                da_txt,
                sizeof(da_txt),
                "%u",
                (unsigned)morse_flipper_straight_trainer_worst_dah_score(&app->straight_trainer));
        morse_flipper_straight_ratio_text(app, ra_txt, sizeof(ra_txt));
    }

    if(app->straight_session_total != 0U)
        pct = ((unsigned)app->straight_session_good * 100U) / app->straight_session_total;
    snprintf(
        cnt,
        sizeof(cnt),
        "%u/%u %u%%",
        (unsigned)app->straight_session_good,
        (unsigned)app->straight_session_total,
        pct);

    if(app->straight_done && count_fail) {
        canvas_set_font(canvas, FontPrimary);
        x = (uint8_t)((128U - canvas_string_width(canvas, "FAIL")) / 2U);
        canvas_draw_str(canvas, x, 46, "FAIL");
    } else if(app->straight_done) {
        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str(canvas, 2, 46, "S");
        canvas_draw_str(canvas, 10, 46, s_txt);
        canvas_draw_str(canvas, 28, 46, "di");
        canvas_draw_str(canvas, 42, 46, di_txt);
        canvas_draw_str(canvas, 62, 46, "da");
        canvas_draw_str(canvas, 76, 46, da_txt);
        canvas_draw_str(canvas, 96, 46, "r");
        canvas_draw_str(canvas, 102, 46, ra_txt);
    }

    canvas_set_font(canvas, FontKeyboard);
    if(app->straight_done) morse_flipper_draw_straight_countdown(canvas, app);
    x = (uint8_t)(126U - canvas_string_width(canvas, cnt));
    canvas_draw_str(canvas, x, 64, cnt);
}

void morse_flipper_draw_trainer_setup(Canvas* canvas, MorseFlipperApp* app) {
    char tone_line[32];
    char trainer_line[32];
    char trainer_line2[32];
    char trainer_line3[32];

    if(canvas == NULL || app == NULL) return;

    canvas_set_font(canvas, FontSecondary);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 8, 12, "Koch Setup");
    canvas_set_font(canvas, FontSecondary);
    snprintf(
        trainer_line,
        sizeof(trainer_line),
        "%c lesson %u/%u",
        app->trainer_row == 0U ? '>' : ' ',
        (unsigned)morse_trainer_lesson(&app->trainer),
        (unsigned)morse_trainer_lesson_count());
    snprintf(
        trainer_line2,
        sizeof(trainer_line2),
        "%c size %u",
        app->trainer_row == 1U ? '>' : ' ',
        (unsigned)morse_trainer_group_size(&app->trainer));
    snprintf(
        trainer_line3,
        sizeof(trainer_line3),
        "%c groups %u",
        app->trainer_row == 2U ? '>' : ' ',
        (unsigned)morse_trainer_session_groups(&app->trainer));
    snprintf(
        tone_line,
        sizeof(tone_line),
        "%c chars %s",
        app->trainer_row == 3U ? '>' : ' ',
        app->trainer.custom_set_idx == 0U ? "lesson" : app->trainer.custom_name);
    canvas_draw_str(canvas, 8, 24, trainer_line);
    canvas_draw_str(canvas, 8, 34, trainer_line2);
    canvas_draw_str(canvas, 8, 44, trainer_line3);
    canvas_draw_str(canvas, 8, 54, tone_line);
    canvas_draw_str(canvas, 8, 64, "OK sess  Bk back");
}

void morse_flipper_draw_straight_screen(Canvas* canvas, MorseFlipperApp* app) {
    uint16_t dit_ms;
    uint32_t target_ms;
    uint32_t answer_ms;
    uint32_t ref_ms;

    if(canvas == NULL || app == NULL) return;

    if(morse_flipper_gpio_probe_notice_active(app) || morse_flipper_gpio_probe_blocks_start(app)) {
        morse_flipper_draw_gpio_probe_overlay(canvas, app);
        return;
    }

    if(!app->straight_started || morse_flipper_straight_countdown_active(app)) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "Straight trainer");
        canvas_set_font(canvas, FontSecondary);
        if(morse_flipper_straight_countdown_active(app)) {
            canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "Starting");
            morse_flipper_draw_straight_countdown(canvas, app);
        } else if(app->input_source == MorseFlipperInputSourceButtons) {
            canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "Press OK to start");
        } else {
            canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Press OK to start");
            canvas_draw_str_aligned(
                canvas, 64, 44, AlignCenter, AlignCenter, "Press your key to start");
        }
        return;
    }

    morse_flipper_draw_straight_prompt(
        canvas, 19, 18, morse_flipper_straight_trainer_target_char(&app->straight_trainer));

    dit_ms = morse_flipper_current_straight_dit_ms(app);

    if(app->straight_done &&
       morse_flipper_straight_trainer_answer(&app->straight_trainer)[0] != '\0') {
        target_ms = morse_flipper_straight_strip_total_ms(
            morse_flipper_straight_trainer_target_morse(&app->straight_trainer),
            app->straight_trainer.target_marks_ms,
            NULL,
            dit_ms,
            true);
        answer_ms = morse_flipper_straight_strip_total_ms(
            morse_flipper_straight_trainer_answer(&app->straight_trainer),
            app->straight_trainer.answer_marks_ms,
            app->straight_trainer.answer_spaces_ms,
            dit_ms,
            true);
        ref_ms = target_ms > answer_ms ? target_ms : answer_ms;

        morse_flipper_draw_straight_strip(
            canvas,
            39,
            6,
            85,
            morse_flipper_straight_trainer_target_morse(&app->straight_trainer),
            app->straight_trainer.target_marks_ms,
            NULL,
            dit_ms,
            ref_ms,
            true);
        morse_flipper_draw_straight_strip(
            canvas,
            39,
            20,
            85,
            morse_flipper_straight_trainer_answer(&app->straight_trainer),
            app->straight_trainer.answer_marks_ms,
            app->straight_trainer.answer_spaces_ms,
            dit_ms,
            ref_ms,
            true);
    }

    morse_flipper_draw_straight_metrics(canvas, app);
}

void morse_flipper_draw_tx_groups_screen(Canvas* canvas, MorseFlipperApp* app) {
    if(canvas == NULL || app == NULL) return;

    canvas_set_font(canvas, FontSecondary);
    if(app->screen == MorseFlipperScreenTxGroups && !app->txg_started) {
        if(morse_flipper_gpio_probe_notice_active(app) ||
           morse_flipper_gpio_probe_blocks_start(app)) {
            morse_flipper_draw_gpio_probe_overlay(canvas, app);
            return;
        }
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "TX Groups of 5");
        canvas_set_font(canvas, FontSecondary);
        if(app->input_source == MorseFlipperInputSourceButtons) {
            canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "Press OK to start");
        } else {
            canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Press OK to start");
            canvas_draw_str_aligned(
                canvas, 64, 44, AlignCenter, AlignCenter, "Press your key to start");
        }
    } else if(app->screen == MorseFlipperScreenTxGroups) {
        morse_flipper_draw_tx_groups_practice(canvas, app);
    } else if(app->screen == MorseFlipperScreenTxGroupsResult) {
        morse_flipper_draw_tx_groups_result(canvas, app);
    } else {
        morse_flipper_draw_tx_groups_final(canvas, app);
    }
}
