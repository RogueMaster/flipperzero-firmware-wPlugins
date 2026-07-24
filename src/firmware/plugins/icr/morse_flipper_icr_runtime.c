/*
 * Purpose: Run the ICR training state machine inside the embedded FAL.
 * Owns: ICR timing, choices, persistence state, and ICR-specific drawing.
 * Depends on: the public ICR API, Canvas, storage, and host-safe CW helpers.
 */

#include "morse_flipper_icr.h"
#include "morse_flipper_icr_api.h"
#include "../../cw.h"

#include <stdlib.h>

#define MORSE_FLIPPER_ICR_WAIT_MS       1000U
#define MORSE_FLIPPER_ICR_TIMEOUT_MS    5000U
#define MORSE_FLIPPER_ICR_GUARD_MS      100U
#define MORSE_FLIPPER_ICR_RESULT_MS     1000U
#define MORSE_FLIPPER_ICR_FLASH_STEP_MS 250U
#define MORSE_FLIPPER_ICR_FLASH_MS (MORSE_FLIPPER_ICR_FLASH_STEP_MS * 5U)
#define MORSE_FLIPPER_ICR_DIT_MS 48U /* 1200 / fixed 25 WPM */

#define MORSE_FLIPPER_ICR_1000MS_BUCKET 50U
#define MORSE_FLIPPER_ICR_GRAPH_MAX_H   44U
#define MORSE_FLIPPER_ICR_GRAPH_Q_H     11U
#define MORSE_FLIPPER_ICR_GRAPH_MIN_H   4U
#define MORSE_FLIPPER_ICR_GRAPH_SOLID_H (MORSE_FLIPPER_ICR_GRAPH_Q_H * 3U)

typedef enum {
    MorseFlipperIcrPhaseGraphWait,
    MorseFlipperIcrPhasePlayback,
    MorseFlipperIcrPhaseRecognition,
    MorseFlipperIcrPhaseRecognizedHold,
    MorseFlipperIcrPhaseAnswerGuard,
    MorseFlipperIcrPhaseAnswer,
    MorseFlipperIcrPhaseResult,
} MorseFlipperIcrPhase;

typedef struct {
    MorseFlipperIcrStats stats;
    bool dirty;
    MorseFlipperIcrPhase phase;
    uint8_t target;
    uint8_t choice;
    uint8_t choices[MORSE_FLIPPER_ICR_CHOICE_COUNT];
    uint8_t mark_idx;
    uint32_t rng_state;
    uint32_t next_at;
    uint32_t reaction_started_at;
    uint32_t pending_reaction_ms;
    uint32_t guard_until;
    uint32_t result_until;
    bool answer_correct;
    bool playback_mark;
    MorseFlipperIcrFeedback feedback;
} MorseFlipperIcrState;

static MorseFlipperIcrResult morse_flipper_icr_result(
    const MorseFlipperIcrState* state,
    bool redraw) {
    return (MorseFlipperIcrResult){
        .handled = true,
        .redraw = redraw,
        .playback_active = state->phase == MorseFlipperIcrPhasePlayback,
        .playback_mark = state->playback_mark,
        .prompt_visible = state->phase == MorseFlipperIcrPhaseResult,
        .prompt_char = morse_flipper_icr_char_at(state->target),
        .feedback = state->feedback,
    };
}

static void morse_flipper_icr_begin_wait(MorseFlipperIcrState* state, uint32_t now_ms) {
    bool has_previous_target = state->target < MORSE_FLIPPER_ICR_CHAR_COUNT;

    state->playback_mark = false;
    state->phase = MorseFlipperIcrPhaseGraphWait;
    state->choice = MORSE_FLIPPER_ICR_NO_CHOICE;
    state->mark_idx = 0U;
    state->next_at = now_ms +
                     (has_previous_target ? MORSE_FLIPPER_ICR_FLASH_MS : MORSE_FLIPPER_ICR_WAIT_MS);
    state->guard_until = has_previous_target ? now_ms + MORSE_FLIPPER_ICR_FLASH_STEP_MS : 0U;
    state->reaction_started_at = 0U;
    state->pending_reaction_ms = 0U;
    state->result_until = 0U;
    state->answer_correct = false;
}

static void morse_flipper_icr_begin_prompt(MorseFlipperIcrState* state, uint32_t now_ms) {
    uint8_t previous = state->target < MORSE_FLIPPER_ICR_CHAR_COUNT ? state->target :
                                                                      MORSE_FLIPPER_ICR_NO_CHOICE;

    state->target =
        morse_flipper_icr_pick_target_except(&state->stats, &state->rng_state, previous);
    state->choice = MORSE_FLIPPER_ICR_NO_CHOICE;
    state->mark_idx = 0U;
    state->playback_mark = false;
    state->pending_reaction_ms = 0U;
    state->reaction_started_at = 0U;
    state->phase = MorseFlipperIcrPhasePlayback;
    state->next_at = now_ms;
}

static void morse_flipper_icr_finish_playback(MorseFlipperIcrState* state, uint32_t now_ms) {
    state->playback_mark = false;
    state->phase = MorseFlipperIcrPhaseRecognition;
    state->reaction_started_at = now_ms;
    state->next_at = now_ms + MORSE_FLIPPER_ICR_TIMEOUT_MS;
}

static void morse_flipper_icr_finish_timeout(MorseFlipperIcrState* state, uint32_t now_ms) {
    morse_flipper_icr_note_answer(
        &state->stats,
        state->target,
        MORSE_FLIPPER_ICR_NO_CHOICE,
        MORSE_FLIPPER_ICR_TIMEOUT_MS);
    state->dirty = true;
    state->choice = MORSE_FLIPPER_ICR_NO_CHOICE;
    state->answer_correct = false;
    state->phase = MorseFlipperIcrPhaseResult;
    state->result_until = now_ms + MORSE_FLIPPER_ICR_RESULT_MS;
    state->feedback = MorseFlipperIcrFeedbackTimeout;
}

static void morse_flipper_icr_open_answer_guard(MorseFlipperIcrState* state, uint32_t now_ms) {
    morse_flipper_icr_build_choices(
        &state->stats, state->target, &state->rng_state, state->choices);
    state->phase = MorseFlipperIcrPhaseAnswerGuard;
    state->guard_until = now_ms + MORSE_FLIPPER_ICR_GUARD_MS;
    state->next_at = 0U;
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

static void morse_flipper_icr_answer(
    MorseFlipperIcrState* state,
    uint8_t choice_pos,
    uint32_t now_ms) {
    uint8_t choice;

    if(choice_pos >= MORSE_FLIPPER_ICR_CHOICE_COUNT) return;
    choice = state->choices[choice_pos];
    if(choice >= MORSE_FLIPPER_ICR_CHAR_COUNT) return;

    state->choice = choice;
    state->answer_correct = choice == state->target;
    morse_flipper_icr_note_answer(
        &state->stats, state->target, choice, state->pending_reaction_ms);
    state->dirty = true;
    state->phase = MorseFlipperIcrPhaseResult;
    state->result_until = now_ms + MORSE_FLIPPER_ICR_RESULT_MS;
    state->feedback = state->answer_correct ? MorseFlipperIcrFeedbackGood :
                                               MorseFlipperIcrFeedbackFail;
}

static void morse_flipper_icr_tick_playback(MorseFlipperIcrState* state, uint32_t now_ms) {
    uint8_t code = cw(morse_flipper_icr_char_at(state->target));
    uint8_t marks = cw_symbol_count(code);

    if(marks == 0U) {
        morse_flipper_icr_begin_wait(state, now_ms);
        return;
    }

    if(state->playback_mark) {
        state->playback_mark = false;
        if(state->mark_idx + 1U < marks) {
            state->mark_idx++;
            state->next_at = now_ms + MORSE_FLIPPER_ICR_DIT_MS;
        } else {
            morse_flipper_icr_finish_playback(state, now_ms);
        }
    } else {
        state->playback_mark = true;
        state->next_at = now_ms +
                         (MORSE_FLIPPER_ICR_DIT_MS * cw_symbol_units(code, state->mark_idx));
    }
}

void* morse_flipper_icr_runtime_alloc(void) {
    return calloc(1U, sizeof(MorseFlipperIcrState));
}

void morse_flipper_icr_runtime_free(void* value) {
    free(value);
}

bool morse_flipper_icr_runtime_enter(
    void* value,
    const MorseFlipperIcrEnterArgs* args,
    MorseFlipperIcrResult* initial) {
    MorseFlipperIcrState* state = value;

    if(state == NULL || args == NULL || initial == NULL || !morse_flipper_icr_stats_load(&state->stats))
        return false;

    state->rng_state = args->rng_seed != 0U ? args->rng_seed : (args->now_ms ^ 0x49435231UL);
    state->target = MORSE_FLIPPER_ICR_NO_CHOICE;
    state->feedback = MorseFlipperIcrFeedbackClear;
    morse_flipper_icr_begin_wait(state, args->now_ms);
    *initial = morse_flipper_icr_result(state, true);
    return true;
}

void morse_flipper_icr_runtime_leave(void* value) {
    MorseFlipperIcrState* state = value;

    if(state != NULL && state->dirty) morse_flipper_icr_stats_save(&state->stats);
}

MorseFlipperIcrResult morse_flipper_icr_runtime_input(
    void* value,
    const InputEvent* event,
    uint32_t now_ms) {
    MorseFlipperIcrState* state = value;

    if(state == NULL || event == NULL) return (MorseFlipperIcrResult){0};
    state->feedback = MorseFlipperIcrFeedbackNone;

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        MorseFlipperIcrResult result = morse_flipper_icr_result(state, false);
        result.request_back = true;
        return result;
    }

    if(state->phase == MorseFlipperIcrPhaseRecognition && event->key == InputKeyOk &&
       event->type == InputTypePress) {
        state->pending_reaction_ms = state->reaction_started_at != 0U ?
                                         now_ms - state->reaction_started_at :
                                         0U;
        state->phase = MorseFlipperIcrPhaseRecognizedHold;
        return morse_flipper_icr_result(state, true);
    }

    if(state->phase == MorseFlipperIcrPhaseRecognizedHold && event->key == InputKeyOk &&
       event->type == InputTypeRelease) {
        morse_flipper_icr_open_answer_guard(state, now_ms);
        return morse_flipper_icr_result(state, true);
    }

    if(state->phase == MorseFlipperIcrPhaseAnswer && event->type == InputTypeRelease) {
        morse_flipper_icr_answer(state, morse_flipper_icr_choice_from_key(event->key), now_ms);
        return morse_flipper_icr_result(state, true);
    }

    return morse_flipper_icr_result(state, false);
}

MorseFlipperIcrResult morse_flipper_icr_runtime_tick(void* value, uint32_t now_ms) {
    MorseFlipperIcrState* state = value;

    if(state == NULL) return (MorseFlipperIcrResult){0};
    state->feedback = MorseFlipperIcrFeedbackNone;

    if(state->phase == MorseFlipperIcrPhaseGraphWait) {
        if(now_ms >= state->next_at) {
            morse_flipper_icr_begin_prompt(state, now_ms);
            return morse_flipper_icr_result(state, true);
        }
        if(state->target < MORSE_FLIPPER_ICR_CHAR_COUNT && state->guard_until != 0U &&
           now_ms >= state->guard_until) {
            do {
                state->guard_until += MORSE_FLIPPER_ICR_FLASH_STEP_MS;
            } while(state->guard_until <= now_ms && state->guard_until < state->next_at);
            return morse_flipper_icr_result(state, true);
        }
    } else if(state->phase == MorseFlipperIcrPhasePlayback && now_ms >= state->next_at) {
        morse_flipper_icr_tick_playback(state, now_ms);
        return morse_flipper_icr_result(state, true);
    } else if(state->phase == MorseFlipperIcrPhaseRecognition && now_ms >= state->next_at) {
        morse_flipper_icr_finish_timeout(state, now_ms);
        return morse_flipper_icr_result(state, true);
    } else if(state->phase == MorseFlipperIcrPhaseAnswerGuard && now_ms >= state->guard_until) {
        state->phase = MorseFlipperIcrPhaseAnswer;
        return morse_flipper_icr_result(state, true);
    } else if(state->phase == MorseFlipperIcrPhaseResult && now_ms >= state->result_until) {
        state->feedback = MorseFlipperIcrFeedbackClear;
        morse_flipper_icr_begin_wait(state, now_ms);
        return morse_flipper_icr_result(state, true);
    }

    return morse_flipper_icr_result(state, false);
}

static const char* morse_flipper_icr_phase_label(const MorseFlipperIcrState* state) {
    switch(state->phase) {
    case MorseFlipperIcrPhaseGraphWait:
        return "Wait";
    case MorseFlipperIcrPhasePlayback:
        return "Listen";
    case MorseFlipperIcrPhaseRecognition:
        return "React";
    case MorseFlipperIcrPhaseRecognizedHold:
        return "Hold";
    case MorseFlipperIcrPhaseResult:
        return state->answer_correct ? "OK" : "Fail";
    default:
        return "";
    }
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

static void morse_flipper_icr_draw_bar(
    Canvas* canvas,
    uint8_t x,
    uint8_t base_y,
    uint8_t bucket) {
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

static bool morse_flipper_icr_flash_visible(const MorseFlipperIcrState* state, uint32_t now_ms) {
    uint32_t remaining;
    uint32_t elapsed;

    if(state->phase != MorseFlipperIcrPhaseGraphWait) return true;
    if(state->target >= MORSE_FLIPPER_ICR_CHAR_COUNT || now_ms >= state->next_at) return true;

    remaining = state->next_at - now_ms;
    elapsed = remaining >= MORSE_FLIPPER_ICR_FLASH_MS ?
                  0U :
                  MORSE_FLIPPER_ICR_FLASH_MS - remaining;
    return ((elapsed / MORSE_FLIPPER_ICR_FLASH_STEP_MS) & 1U) == 0U;
}

static void morse_flipper_icr_draw_graph(
    Canvas* canvas,
    MorseFlipperIcrState* state,
    uint32_t now_ms) {
    bool flash_visible = morse_flipper_icr_flash_visible(state, now_ms);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 127, 8, AlignRight, AlignBottom, morse_flipper_icr_phase_label(state));
    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHAR_COUNT; i++) {
        uint8_t x = (uint8_t)(4U + (i * 3U));

        if(state->phase == MorseFlipperIcrPhaseGraphWait && i == state->target && !flash_visible)
            continue;
        morse_flipper_icr_draw_bar(canvas, x, 59U, state->stats.avg_ms20[i]);
    }
    if(state->phase == MorseFlipperIcrPhaseGraphWait &&
       state->target < MORSE_FLIPPER_ICR_CHAR_COUNT && flash_visible) {
        canvas_draw_box(canvas, (uint8_t)(4U + (state->target * 3U)), 62U, 2U, 2U);
    }
}

static void morse_flipper_icr_draw_choice(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    uint8_t choice) {
    char text[2] = {
        choice < MORSE_FLIPPER_ICR_CHAR_COUNT ? morse_flipper_icr_char_at(choice) : '?',
        '\0',
    };

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, x, y + 1, AlignCenter, AlignCenter, text);
}

static void morse_flipper_icr_draw_choices(Canvas* canvas, const MorseFlipperIcrState* state) {
    morse_flipper_icr_draw_choice(canvas, 27, 12, state->choices[0]);
    morse_flipper_icr_draw_choice(canvas, 27, 51, state->choices[1]);
    morse_flipper_icr_draw_choice(canvas, 10, 32, state->choices[2]);
    morse_flipper_icr_draw_choice(canvas, 44, 32, state->choices[3]);
    morse_flipper_icr_draw_choice(canvas, 27, 32, state->choices[4]);
}

MorseFlipperIcrDrawResult morse_flipper_icr_runtime_draw(
    void* value,
    Canvas* canvas,
    uint32_t now_ms) {
    MorseFlipperIcrState* state = value;

    if(state == NULL || canvas == NULL) return (MorseFlipperIcrDrawResult){0};
    if(state->phase < MorseFlipperIcrPhaseAnswerGuard) {
        morse_flipper_icr_draw_graph(canvas, state, now_ms);
        return (MorseFlipperIcrDrawResult){0};
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 127, 8, AlignRight, AlignBottom, morse_flipper_icr_phase_label(state));
    if(state->choice != MORSE_FLIPPER_ICR_NO_CHOICE ||
       state->phase != MorseFlipperIcrPhaseResult)
        morse_flipper_icr_draw_choices(canvas, state);

    canvas_draw_line(canvas, 57, 0, 57, 63);
    if(state->phase != MorseFlipperIcrPhaseResult) return (MorseFlipperIcrDrawResult){0};

    return (MorseFlipperIcrDrawResult){
        .draw_prompt = true,
        .prompt_char = morse_flipper_icr_char_at(state->target),
        .prompt_cx = 92,
        .prompt_cy = 36,
    };
}
