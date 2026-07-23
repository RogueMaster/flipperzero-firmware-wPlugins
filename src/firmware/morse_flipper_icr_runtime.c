/*
 * Purpose: Manage ICR runtime lifetime and first-screen drawing.
 * Owns: ICR stats allocation, scene reset, and ICR canvas rendering.
 * Depends on: morse_flipper_app_i.h and morse_flipper_icr.h.
 * Tests: firmware build.
 */

#include "morse_flipper_app_i.h"
#include "cw.h"

#include <stdlib.h>

#define MORSE_FLIPPER_ICR_1000MS_BUCKET 50U
#define MORSE_FLIPPER_ICR_GRAPH_MAX_H 44U
#define MORSE_FLIPPER_ICR_GRAPH_Q_H 11U
#define MORSE_FLIPPER_ICR_GRAPH_MIN_H 4U
#define MORSE_FLIPPER_ICR_GRAPH_SOLID_H (MORSE_FLIPPER_ICR_GRAPH_Q_H * 3U)
#define MORSE_FLIPPER_ICR_FLASH_STEP_MS 250U
#define MORSE_FLIPPER_ICR_FLASH_MS (MORSE_FLIPPER_ICR_FLASH_STEP_MS * 5U)

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
    uint8_t previous;

    if(app == NULL || app->icr_stats == NULL) return;

    previous = app->icr_target < MORSE_FLIPPER_ICR_CHAR_COUNT ? app->icr_target :
                                                              MORSE_FLIPPER_ICR_NO_CHOICE;
    app->icr_target =
        morse_flipper_icr_pick_target_except(app->icr_stats, &app->icr_rng_state, previous);
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
    app->icr_choice = MORSE_FLIPPER_ICR_NO_CHOICE;
    app->icr_mark_idx = 0U;
    if(app->icr_target < MORSE_FLIPPER_ICR_CHAR_COUNT) {
        app->icr_next_at = now_ms + MORSE_FLIPPER_ICR_FLASH_MS;
        app->icr_guard_until = now_ms + MORSE_FLIPPER_ICR_FLASH_STEP_MS;
    } else {
        app->icr_next_at = now_ms + MORSE_FLIPPER_ICR_WAIT_MS;
        app->icr_guard_until = 0U;
    }
    app->icr_reaction_started_at = 0U;
    app->icr_pending_reaction_ms = 0U;
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

    if(app->icr_phase == MorseFlipperIcrPhaseGraphWait) {
        if(now_ms >= app->icr_next_at) {
            morse_flipper_icr_begin_prompt(app, now_ms);
            return;
        }
        if(app->icr_target < MORSE_FLIPPER_ICR_CHAR_COUNT && app->icr_guard_until != 0U &&
           now_ms >= app->icr_guard_until) {
            do {
                app->icr_guard_until += MORSE_FLIPPER_ICR_FLASH_STEP_MS;
            } while(app->icr_guard_until <= now_ms && app->icr_guard_until < app->icr_next_at);
            morse_flipper_view_dirty(app);
        }
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
        return "";
    case MorseFlipperIcrPhaseResult:
        return app->icr_answer_correct ? "OK" : "Fail";
    default:
        return "";
    }
}

static void morse_flipper_icr_char_text(uint8_t index, char out[2]) {
    out[0] = index < MORSE_FLIPPER_ICR_CHAR_COUNT ? morse_flipper_icr_char_at(index) : '?';
    out[1] = '\0';
}

static uint8_t morse_flipper_icr_scale_bar_height(
    uint8_t bucket,
    uint8_t fast_bucket,
    uint8_t slow_bucket,
    uint8_t tall_h,
    uint8_t short_h) {
    uint16_t span;
    uint16_t pos;

    if(bucket <= fast_bucket) return tall_h;
    if(bucket >= slow_bucket) return short_h;

    span = (uint16_t)(slow_bucket - fast_bucket);
    pos = (uint16_t)(slow_bucket - bucket);
    return (uint8_t)(short_h + ((pos * (uint16_t)(tall_h - short_h)) / span));
}

static uint8_t morse_flipper_icr_bar_height(uint8_t bucket) {
    if(bucket == 0U) return 0U;
    if(bucket <= MORSE_FLIPPER_ICR_INSTANT_BUCKET) {
        return morse_flipper_icr_scale_bar_height(
            bucket,
            1U,
            MORSE_FLIPPER_ICR_INSTANT_BUCKET,
            MORSE_FLIPPER_ICR_GRAPH_MAX_H,
            MORSE_FLIPPER_ICR_GRAPH_SOLID_H + 1U);
    }
    if(bucket <= MORSE_FLIPPER_ICR_1000MS_BUCKET) {
        return morse_flipper_icr_scale_bar_height(
            bucket,
            MORSE_FLIPPER_ICR_INSTANT_BUCKET,
            MORSE_FLIPPER_ICR_1000MS_BUCKET,
            MORSE_FLIPPER_ICR_GRAPH_SOLID_H,
            MORSE_FLIPPER_ICR_GRAPH_Q_H + 1U);
    }
    return morse_flipper_icr_scale_bar_height(
        bucket,
        MORSE_FLIPPER_ICR_1000MS_BUCKET,
        MORSE_FLIPPER_ICR_TIMEOUT_BUCKET,
        MORSE_FLIPPER_ICR_GRAPH_Q_H,
        MORSE_FLIPPER_ICR_GRAPH_MIN_H);
}

static void morse_flipper_icr_draw_checker(
    Canvas* canvas,
    uint8_t x,
    uint8_t top,
    uint8_t bottom) {
    for(uint8_t y = top; y <= bottom; y++) {
        canvas_draw_dot(canvas, x + ((y - top) & 1U), y);
    }
}

static void morse_flipper_icr_draw_bar(Canvas* canvas, uint8_t x, uint8_t base_y, uint8_t bucket) {
    uint8_t h = morse_flipper_icr_bar_height(bucket);
    uint8_t top;

    if(bucket == 0U) {
        canvas_draw_dot(canvas, x, base_y);
        canvas_draw_dot(canvas, x + 1U, base_y);
        return;
    }

    top = (uint8_t)(base_y - h + 1U);
    if(h > MORSE_FLIPPER_ICR_GRAPH_SOLID_H) {
        uint8_t solid_top = (uint8_t)(base_y - MORSE_FLIPPER_ICR_GRAPH_SOLID_H + 1U);

        morse_flipper_icr_draw_checker(canvas, x, top, (uint8_t)(solid_top - 1U));
        canvas_draw_box(canvas, x, solid_top, 2U, MORSE_FLIPPER_ICR_GRAPH_SOLID_H);
        return;
    }

    canvas_draw_box(canvas, x, top, 2U, h);
}

static bool morse_flipper_icr_flash_visible(const MorseFlipperApp* app, uint32_t now_ms) {
    uint32_t remaining;
    uint32_t elapsed;

    if(app == NULL || app->icr_phase != MorseFlipperIcrPhaseGraphWait) return true;
    if(app->icr_target >= MORSE_FLIPPER_ICR_CHAR_COUNT) return true;
    if(now_ms >= app->icr_next_at) return true;

    remaining = app->icr_next_at - now_ms;
    elapsed = remaining >= MORSE_FLIPPER_ICR_FLASH_MS ? 0U :
                                                       MORSE_FLIPPER_ICR_FLASH_MS - remaining;
    return ((elapsed / MORSE_FLIPPER_ICR_FLASH_STEP_MS) & 1U) == 0U;
}

static void morse_flipper_icr_draw_graph(Canvas* canvas, MorseFlipperApp* app) {
    enum {
        X0 = 4U,
        Step = 3U,
        BaseY = 59U,
        DotY = 62U,
    };
    uint32_t now_ms;
    uint8_t target;
    bool flash_visible;

    if(canvas == NULL || app == NULL) return;

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 127, 8, AlignRight, AlignBottom, morse_flipper_icr_phase_label(app));

    if(app->icr_stats == NULL) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "ICR unavailable");
        return;
    }

    now_ms = furi_get_tick();
    target = app->icr_target;
    flash_visible = morse_flipper_icr_flash_visible(app, now_ms);

    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHAR_COUNT; i++) {
        uint8_t x = (uint8_t)(X0 + (i * Step));

        if(app->icr_phase == MorseFlipperIcrPhaseGraphWait && i == target && !flash_visible)
            continue;
        morse_flipper_icr_draw_bar(canvas, x, BaseY, app->icr_stats->avg_ms20[i]);
    }

    if(app->icr_phase == MorseFlipperIcrPhaseGraphWait && target < MORSE_FLIPPER_ICR_CHAR_COUNT &&
       flash_visible) {
        uint8_t x = (uint8_t)(X0 + (target * Step));
        canvas_draw_box(canvas, x, DotY, 2U, 2U);
    }
}

static void morse_flipper_icr_draw_choice(Canvas* canvas, int32_t x, int32_t y, uint8_t choice) {
    char text[2];

    if(canvas == NULL) return;
    morse_flipper_icr_char_text(choice, text);
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
    uint8_t target;

    if(canvas == NULL || app == NULL) return;

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 127, 8, AlignRight, AlignBottom, morse_flipper_icr_phase_label(app));

    if(app->icr_choice != MORSE_FLIPPER_ICR_NO_CHOICE ||
       app->icr_phase != MorseFlipperIcrPhaseResult) {
        morse_flipper_icr_draw_choices(canvas, app);
    }

    canvas_draw_line(canvas, 57, 0, 57, 63);
    if(app->icr_phase != MorseFlipperIcrPhaseResult) return;

    target = app->icr_target;
    morse_flipper_draw_straight_prompt(
        canvas,
        app,
        92,
        36,
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
