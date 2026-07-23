/*
 * Purpose: Manage ICR runtime lifetime and first-screen drawing.
 * Owns: ICR stats allocation, scene reset, and ICR canvas rendering.
 * Depends on: morse_flipper_app_i.h and morse_flipper_icr.h.
 * Tests: firmware build.
 */

#include "morse_flipper_app_i.h"
#include "cw.h"

#include <stdlib.h>

bool morse_flipper_ensure_icr_stats_loaded(MorseFlipperApp* app) {
    if(app == NULL) return false;
    if(app->icr_stats != NULL) return true;

    app->icr_stats = malloc(sizeof(MorseFlipperIcrStats));
    if(app->icr_stats == NULL) return false;
    if(!morse_flipper_icr_stats_load(app->icr_stats)) {
        free(app->icr_stats);
        app->icr_stats = NULL;
        return false;
    }

    app->icr_stats_dirty = false;
    return true;
}

void morse_flipper_release_icr_stats(MorseFlipperApp* app, bool save) {
    if(app == NULL || app->icr_stats == NULL) return;

    if(save && app->icr_stats_dirty) {
        morse_flipper_icr_stats_save(app->icr_stats);
    }
    free(app->icr_stats);
    app->icr_stats = NULL;
    app->icr_stats_dirty = false;
}

void morse_flipper_reset_icr_runtime(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    app->icr_playback_mark = false;
    app->icr_phase = MorseFlipperIcrPhaseGraphWait;
    app->icr_target = MORSE_FLIPPER_ICR_NO_CHOICE;
    app->icr_choice = MORSE_FLIPPER_ICR_NO_CHOICE;
    app->icr_mark_idx = 0U;
    app->icr_next_at = now_ms + MORSE_FLIPPER_ICR_WAIT_MS;
    app->icr_reaction_started_at = 0U;
    app->icr_pending_reaction_ms = 0U;
    app->icr_guard_until = 0U;
    app->icr_result_until = 0U;
    app->icr_answer_correct = false;
    app->session_result_tone = false;
    app->session_result_good = false;
    app->session_result_until = 0U;
    morse_flipper_update_sidetone(app);
    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHOICE_COUNT; i++) {
        app->icr_choices[i] = MORSE_FLIPPER_ICR_NO_CHOICE;
    }
}

void morse_flipper_enter_icr(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    morse_flipper_ensure_icr_stats_loaded(app);
    if(app->icr_rng_state == 0U) app->icr_rng_state = now_ms ^ 0x49435231UL;
}

void morse_flipper_leave_icr(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;
    UNUSED(now_ms);

    app->icr_playback_mark = false;
    app->session_result_tone = false;
    app->session_result_good = false;
    app->session_result_until = 0U;
    morse_flipper_release_icr_stats(app, true);
    morse_flipper_update_sidetone(app);
}

static uint16_t morse_flipper_icr_dit_ms(void) {
    return morse_flipper_wpm_to_dit_ms(MORSE_FLIPPER_ICR_WPM);
}

static void morse_flipper_icr_begin_prompt(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL || app->icr_stats == NULL) return;

    app->icr_target = morse_flipper_icr_pick_target(app->icr_stats, &app->icr_rng_state);
    app->icr_choice = MORSE_FLIPPER_ICR_NO_CHOICE;
    app->icr_mark_idx = 0U;
    app->icr_playback_mark = false;
    app->icr_pending_reaction_ms = 0U;
    app->icr_reaction_started_at = 0U;
    app->icr_phase = MorseFlipperIcrPhasePlayback;
    app->icr_next_at = now_ms;
    morse_flipper_view_dirty(app);
}

static void morse_flipper_icr_begin_wait(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    app->icr_playback_mark = false;
    app->icr_phase = MorseFlipperIcrPhaseGraphWait;
    app->icr_target = MORSE_FLIPPER_ICR_NO_CHOICE;
    app->icr_choice = MORSE_FLIPPER_ICR_NO_CHOICE;
    app->icr_mark_idx = 0U;
    app->icr_next_at = now_ms + MORSE_FLIPPER_ICR_WAIT_MS;
    app->icr_reaction_started_at = 0U;
    app->icr_pending_reaction_ms = 0U;
    app->icr_guard_until = 0U;
    app->icr_result_until = 0U;
    app->icr_answer_correct = false;
    morse_flipper_update_sidetone(app);
    morse_flipper_view_dirty(app);
}

static void morse_flipper_icr_finish_playback(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    app->icr_playback_mark = false;
    app->icr_phase = MorseFlipperIcrPhaseRecognition;
    app->icr_reaction_started_at = now_ms;
    app->icr_next_at = now_ms + MORSE_FLIPPER_ICR_TIMEOUT_MS;
    morse_flipper_update_sidetone(app);
    morse_flipper_view_dirty(app);
}

static void morse_flipper_icr_correct_feedback(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    app->session_result_tone = false;
    app->session_result_good = true;
    app->session_result_until = now_ms + MORSE_FLIPPER_SESSION_RESULT_MS;
    morse_flipper_update_sidetone(app);
}

static void morse_flipper_icr_clear_feedback(MorseFlipperApp* app) {
    if(app == NULL) return;

    if(!app->session_result_tone && !app->session_result_good && app->session_result_until == 0U)
        return;
    app->session_result_tone = false;
    app->session_result_good = false;
    app->session_result_until = 0U;
    morse_flipper_update_sidetone(app);
}

static void morse_flipper_icr_tick_feedback(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL || app->session_result_until == 0U) return;
    if(now_ms < app->session_result_until) return;

    morse_flipper_icr_clear_feedback(app);
}

static void morse_flipper_icr_finish_timeout(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL || app->icr_stats == NULL) return;
    if(app->icr_target >= MORSE_FLIPPER_ICR_CHAR_COUNT) return;

    morse_flipper_icr_note_answer(
        app->icr_stats, app->icr_target, MORSE_FLIPPER_ICR_NO_CHOICE, MORSE_FLIPPER_ICR_TIMEOUT_MS);
    app->icr_stats_dirty = true;
    app->icr_choice = MORSE_FLIPPER_ICR_NO_CHOICE;
    app->icr_answer_correct = false;
    app->icr_phase = MorseFlipperIcrPhaseResult;
    app->icr_result_until = now_ms + MORSE_FLIPPER_ICR_RESULT_MS;
    app->session_result_good = false;
    morse_flipper_feedback_timeout(app);
    morse_flipper_view_dirty(app);
}

static void morse_flipper_icr_open_answer_guard(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL || app->icr_stats == NULL) return;
    if(app->icr_target >= MORSE_FLIPPER_ICR_CHAR_COUNT) return;

    morse_flipper_icr_build_choices(
        app->icr_stats, app->icr_target, &app->icr_rng_state, app->icr_choices);
    app->icr_phase = MorseFlipperIcrPhaseAnswerGuard;
    app->icr_guard_until = now_ms + MORSE_FLIPPER_ICR_GUARD_MS;
    app->icr_next_at = 0U;
    morse_flipper_view_dirty(app);
}

static uint8_t morse_flipper_icr_choice_from_key(InputKey key) {
    switch(key) {
    case InputKeyUp:
        return 0U;
    case InputKeyDown:
        return 1U;
    case InputKeyLeft:
        return 2U;
    case InputKeyRight:
        return 3U;
    case InputKeyOk:
        return 4U;
    default:
        return MORSE_FLIPPER_ICR_NO_CHOICE;
    }
}

static void morse_flipper_icr_answer(MorseFlipperApp* app, uint8_t choice_pos, uint32_t now_ms) {
    uint8_t choice;

    if(app == NULL || app->icr_stats == NULL) return;
    if(app->icr_target >= MORSE_FLIPPER_ICR_CHAR_COUNT) return;
    if(choice_pos >= MORSE_FLIPPER_ICR_CHOICE_COUNT) return;

    choice = app->icr_choices[choice_pos];
    if(choice >= MORSE_FLIPPER_ICR_CHAR_COUNT) return;

    app->icr_choice = choice;
    app->icr_answer_correct = choice == app->icr_target;
    morse_flipper_icr_note_answer(
        app->icr_stats, app->icr_target, choice, app->icr_pending_reaction_ms);
    app->icr_stats_dirty = true;
    app->icr_phase = MorseFlipperIcrPhaseResult;
    app->icr_result_until = now_ms + MORSE_FLIPPER_ICR_RESULT_MS;
    if(app->icr_answer_correct) {
        morse_flipper_icr_correct_feedback(app, now_ms);
    } else {
        app->session_result_good = false;
        morse_flipper_feedback_fail(app);
    }
    morse_flipper_view_dirty(app);
}

static void morse_flipper_icr_tick_playback(MorseFlipperApp* app, uint32_t now_ms) {
    uint8_t code;
    uint8_t marks;
    uint16_t dit_ms;

    if(app == NULL || app->icr_target >= MORSE_FLIPPER_ICR_CHAR_COUNT) return;

    code = cw(morse_flipper_icr_char_at(app->icr_target));
    marks = cw_symbol_count(code);
    if(marks == 0U) {
        app->icr_next_at = now_ms + MORSE_FLIPPER_ICR_WAIT_MS;
        app->icr_phase = MorseFlipperIcrPhaseGraphWait;
        morse_flipper_view_dirty(app);
        return;
    }

    dit_ms = morse_flipper_icr_dit_ms();
    if(app->icr_playback_mark) {
        app->icr_playback_mark = false;
        if(app->icr_mark_idx + 1U < marks) {
            app->icr_mark_idx++;
            app->icr_next_at = now_ms + dit_ms;
        } else {
            morse_flipper_icr_finish_playback(app, now_ms);
            return;
        }
    } else {
        app->icr_playback_mark = true;
        app->icr_next_at = now_ms + (dit_ms * cw_symbol_units(code, app->icr_mark_idx));
    }

    morse_flipper_update_sidetone(app);
    morse_flipper_view_dirty(app);
}

void morse_flipper_tick_icr(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL || app->screen != MorseFlipperScreenIcr) return;

    morse_flipper_icr_tick_feedback(app, now_ms);
    if(app->icr_stats == NULL) {
        morse_flipper_ensure_icr_stats_loaded(app);
        if(app->icr_stats == NULL) return;
    }

    if(app->icr_phase == MorseFlipperIcrPhaseGraphWait && now_ms >= app->icr_next_at) {
        morse_flipper_icr_begin_prompt(app, now_ms);
        return;
    }

    if(app->icr_phase == MorseFlipperIcrPhasePlayback && now_ms >= app->icr_next_at) {
        morse_flipper_icr_tick_playback(app, now_ms);
        return;
    }

    if(app->icr_phase == MorseFlipperIcrPhaseRecognition && now_ms >= app->icr_next_at) {
        morse_flipper_icr_finish_timeout(app, now_ms);
        return;
    }

    if(app->icr_phase == MorseFlipperIcrPhaseAnswerGuard && now_ms >= app->icr_guard_until) {
        app->icr_phase = MorseFlipperIcrPhaseAnswer;
        morse_flipper_view_dirty(app);
        return;
    }

    if(app->icr_phase == MorseFlipperIcrPhaseResult && now_ms >= app->icr_result_until) {
        morse_flipper_icr_clear_feedback(app);
        morse_flipper_icr_begin_wait(app, now_ms);
    }
}

bool morse_flipper_icr_input(MorseFlipperApp* app, const InputEvent* event, uint32_t now_ms) {
    uint8_t choice_pos;

    if(app == NULL || event == NULL || app->screen != MorseFlipperScreenIcr) return false;

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        morse_flipper_scene_back(app);
        return true;
    }

    if(app->icr_phase == MorseFlipperIcrPhaseRecognition && event->key == InputKeyOk &&
       event->type == InputTypePress) {
        if(app->icr_reaction_started_at != 0U) {
            app->icr_pending_reaction_ms = now_ms - app->icr_reaction_started_at;
        } else {
            app->icr_pending_reaction_ms = 0U;
        }
        app->icr_phase = MorseFlipperIcrPhaseRecognizedHold;
        morse_flipper_view_dirty(app);
        return true;
    }

    if(app->icr_phase == MorseFlipperIcrPhaseRecognizedHold && event->key == InputKeyOk &&
       event->type == InputTypeRelease) {
        morse_flipper_icr_open_answer_guard(app, now_ms);
        return true;
    }

    if(app->icr_phase == MorseFlipperIcrPhaseAnswer && event->type == InputTypeRelease) {
        choice_pos = morse_flipper_icr_choice_from_key(event->key);
        if(choice_pos != MORSE_FLIPPER_ICR_NO_CHOICE) {
            morse_flipper_icr_answer(app, choice_pos, now_ms);
        }
        return true;
    }

    return true;
}

static const char* morse_flipper_icr_phase_label(const MorseFlipperApp* app) {
    if(app == NULL) return "";

    switch(app->icr_phase) {
    case MorseFlipperIcrPhaseGraphWait:
        return "Wait";
    case MorseFlipperIcrPhasePlayback:
        return "Listen";
    case MorseFlipperIcrPhaseRecognition:
        return "React";
    case MorseFlipperIcrPhaseRecognizedHold:
        return "Hold";
    case MorseFlipperIcrPhaseAnswerGuard:
    case MorseFlipperIcrPhaseAnswer:
        return "Pick";
    case MorseFlipperIcrPhaseResult:
        return app->icr_choice == MORSE_FLIPPER_ICR_NO_CHOICE ?
                   "Time" :
                   (app->icr_answer_correct ? "OK" : "No");
    default:
        return "";
    }
}

static void morse_flipper_icr_char_text(uint8_t index, char out[2]) {
    out[0] = index < MORSE_FLIPPER_ICR_CHAR_COUNT ? morse_flipper_icr_char_at(index) : '?';
    out[1] = '\0';
}

static uint8_t morse_flipper_icr_bar_height(uint8_t bucket) {
    if(bucket == 0U) return 4U;
    if(bucket <= MORSE_FLIPPER_ICR_INSTANT_BUCKET) return 36U;
    if(bucket <= MORSE_FLIPPER_ICR_RECOGNIZED_BUCKET) return 25U;
    if(bucket < MORSE_FLIPPER_ICR_TIMEOUT_BUCKET) return 15U;
    return 8U;
}

static void morse_flipper_icr_draw_bar(Canvas* canvas, uint8_t x, uint8_t base_y, uint8_t bucket) {
    uint8_t h = morse_flipper_icr_bar_height(bucket);
    uint8_t top = (uint8_t)(base_y - h + 1U);

    if(bucket == 0U) {
        canvas_draw_dot(canvas, x, base_y);
        canvas_draw_dot(canvas, x + 1U, base_y);
    } else if(bucket <= MORSE_FLIPPER_ICR_INSTANT_BUCKET) {
        canvas_draw_box(canvas, x, top, 2U, h);
    } else if(bucket <= MORSE_FLIPPER_ICR_RECOGNIZED_BUCKET) {
        for(uint8_t y = top; y <= base_y; y = (uint8_t)(y + 2U)) {
            canvas_draw_box(canvas, x, y, 2U, 1U);
        }
    } else if(bucket < MORSE_FLIPPER_ICR_TIMEOUT_BUCKET) {
        canvas_draw_line(canvas, x, top, x, base_y);
        canvas_draw_line(canvas, x + 1U, top, x + 1U, base_y);
        canvas_draw_line(canvas, x, base_y, x + 1U, base_y);
    } else {
        canvas_draw_box(canvas, x, top, 2U, h);
        if(top > 2U) canvas_draw_line(canvas, x, top - 2U, x + 1U, top - 2U);
    }
}

static void morse_flipper_icr_draw_graph(Canvas* canvas, MorseFlipperApp* app) {
    enum {
        X0 = 4U,
        Step = 3U,
        BaseY = 55U,
    };
    uint8_t target;

    if(canvas == NULL || app == NULL) return;

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 1, 8, "ICR");
    canvas_draw_str_aligned(
        canvas, 127, 8, AlignRight, AlignBottom, morse_flipper_icr_phase_label(app));

    if(app->icr_stats == NULL) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "ICR unavailable");
        return;
    }

    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHAR_COUNT; i++) {
        uint8_t x = (uint8_t)(X0 + (i * Step));
        morse_flipper_icr_draw_bar(canvas, x, BaseY, app->icr_stats->avg_ms20[i]);
    }

    target = app->icr_target;
    if(target < MORSE_FLIPPER_ICR_CHAR_COUNT) {
        uint8_t x = (uint8_t)(X0 + (target * Step));
        canvas_draw_box(canvas, x, 58U, 2U, 2U);
    }
}

static void morse_flipper_icr_draw_choice(Canvas* canvas, int32_t x, int32_t y, uint8_t choice) {
    char text[2];

    if(canvas == NULL) return;
    morse_flipper_icr_char_text(choice, text);
    canvas_draw_frame(canvas, x - 7, y - 6, 15U, 13U);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, x, y + 1, AlignCenter, AlignCenter, text);
}

static void morse_flipper_icr_draw_choices(Canvas* canvas, MorseFlipperApp* app) {
    if(canvas == NULL || app == NULL) return;

    morse_flipper_icr_draw_choice(canvas, 27, 12, app->icr_choices[0]);
    morse_flipper_icr_draw_choice(canvas, 27, 51, app->icr_choices[1]);
    morse_flipper_icr_draw_choice(canvas, 10, 32, app->icr_choices[2]);
    morse_flipper_icr_draw_choice(canvas, 44, 32, app->icr_choices[3]);
    morse_flipper_icr_draw_choice(canvas, 27, 32, app->icr_choices[4]);
}

static void morse_flipper_icr_draw_answer(Canvas* canvas, MorseFlipperApp* app) {
    const char* title;
    uint8_t target;

    if(canvas == NULL || app == NULL) return;

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 1, 8, "ICR");
    canvas_draw_str_aligned(
        canvas, 127, 8, AlignRight, AlignBottom, morse_flipper_icr_phase_label(app));

    if(app->icr_choice != MORSE_FLIPPER_ICR_NO_CHOICE ||
       app->icr_phase != MorseFlipperIcrPhaseResult) {
        morse_flipper_icr_draw_choices(canvas, app);
    }

    canvas_draw_line(canvas, 57, 0, 57, 63);
    if(app->icr_phase != MorseFlipperIcrPhaseResult) return;

    target = app->icr_target;
    title = app->icr_choice == MORSE_FLIPPER_ICR_NO_CHOICE ?
                "Time" :
                (app->icr_answer_correct ? "OK" : "No");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 92, 16, AlignCenter, AlignCenter, title);
    morse_flipper_draw_straight_prompt(
        canvas,
        92,
        39,
        target < MORSE_FLIPPER_ICR_CHAR_COUNT ? morse_flipper_icr_char_at(target) : '?');
}

void morse_flipper_draw_icr(Canvas* canvas, MorseFlipperApp* app) {
    if(app == NULL) return;

    if(app->icr_phase == MorseFlipperIcrPhaseAnswerGuard ||
       app->icr_phase == MorseFlipperIcrPhaseAnswer ||
       app->icr_phase == MorseFlipperIcrPhaseResult) {
        morse_flipper_icr_draw_answer(canvas, app);
        return;
    }

    morse_flipper_icr_draw_graph(canvas, app);
}
