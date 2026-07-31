/*
 * Purpose: Run the ICR training state machine inside the embedded FAL.
 * Owns: ICR timing, choices, persistence state, and ICR-specific drawing.
 * Depends on: the public ICR API, Canvas, storage, and host-safe CW helpers.
 */

#include "morse_flipper_icr.h"
#include "morse_flipper_icr_api.h"
#include "morse_flipper_icr_choice_bitmap.h"
#include "../../cw.h"
#include "../../morse_flipper_time.h"

#include <dialogs/dialogs.h>
#include <gui/elements.h>

#define MORSE_FLIPPER_CW_TOKEN_SK    0x80U
#define MORSE_FLIPPER_CW_TOKEN_BK    0x81U
#define MORSE_FLIPPER_CW_TOKEN_CT_KA 0x82U
#define MORSE_FLIPPER_CW_TOKEN_VE_SN 0x83U
#define MORSE_FLIPPER_CW_TOKEN_AA    0x84U
#define MORSE_FLIPPER_CW_TOKEN_SOS   0x85U
#include "../../../../tools/terminus24_source.h"

#include <stdlib.h>

#define MORSE_FLIPPER_ICR_WAIT_MS        1000U
#define MORSE_FLIPPER_ICR_TIMEOUT_MS     5000U
#define MORSE_FLIPPER_ICR_GUARD_MS       100U
#define MORSE_FLIPPER_ICR_RESULT_MS      1000U
#define MORSE_FLIPPER_ICR_RELEASE_POP_MS 200U
#define MORSE_FLIPPER_ICR_FLASH_STEP_MS  250U
#define MORSE_FLIPPER_ICR_FLASH_MS       (MORSE_FLIPPER_ICR_FLASH_STEP_MS * 5U)
#define MORSE_FLIPPER_ICR_DIT_MS         48U /* 1200 / fixed 25 WPM */

#define MORSE_FLIPPER_ICR_1000MS_BUCKET      50U
#define MORSE_FLIPPER_ICR_GRAPH_MAX_H        44U
#define MORSE_FLIPPER_ICR_GRAPH_Q_H          11U
#define MORSE_FLIPPER_ICR_GRAPH_MIN_H        4U
#define MORSE_FLIPPER_ICR_GRAPH_RECOGNIZED_H (MORSE_FLIPPER_ICR_GRAPH_Q_H * 3U)

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
    uint8_t pressed_choice;
    uint8_t choices[MORSE_FLIPPER_ICR_CHOICE_COUNT];
    uint8_t mark_idx;
    uint32_t rng_state;
    uint32_t next_at;
    uint32_t reaction_started_at;
    uint32_t pending_reaction_ms;
    uint32_t guard_until;
    uint32_t result_until;
    bool answer_correct;
    bool answer_key_held;
    bool playback_mark;
    MorseFlipperIcrFeedback feedback;
} MorseFlipperIcrState;

static MorseFlipperIcrResult
    morse_flipper_icr_result(const MorseFlipperIcrState* state, bool redraw) {
    return (MorseFlipperIcrResult){
        .handled = true,
        .redraw = redraw,
        .phase = state->phase,
        .playback_active = state->phase == MorseFlipperIcrPhasePlayback,
        .playback_mark = state->playback_mark,
        .feedback = state->feedback,
    };
}

static void morse_flipper_icr_begin_wait(MorseFlipperIcrState* state, uint32_t now_ms) {
    bool has_previous_target = state->target < MORSE_FLIPPER_ICR_CHAR_COUNT;

    state->playback_mark = false;
    state->phase = MorseFlipperIcrPhaseGraphWait;
    state->choice = MORSE_FLIPPER_ICR_NO_CHOICE;
    state->pressed_choice = MORSE_FLIPPER_ICR_NO_CHOICE;
    state->mark_idx = 0U;
    state->next_at =
        now_ms + (has_previous_target ? MORSE_FLIPPER_ICR_FLASH_MS : MORSE_FLIPPER_ICR_WAIT_MS);
    state->guard_until = has_previous_target ? now_ms + MORSE_FLIPPER_ICR_FLASH_STEP_MS : 0U;
    state->reaction_started_at = 0U;
    state->pending_reaction_ms = 0U;
    state->result_until = 0U;
    state->answer_correct = false;
    state->answer_key_held = false;
}

static void morse_flipper_icr_begin_prompt(MorseFlipperIcrState* state, uint32_t now_ms) {
    uint8_t previous = state->target < MORSE_FLIPPER_ICR_CHAR_COUNT ? state->target :
                                                                      MORSE_FLIPPER_ICR_NO_CHOICE;

    state->target =
        morse_flipper_icr_pick_target_except(&state->stats, &state->rng_state, previous);
    state->choice = MORSE_FLIPPER_ICR_NO_CHOICE;
    state->pressed_choice = MORSE_FLIPPER_ICR_NO_CHOICE;
    state->answer_key_held = false;
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
        &state->stats, state->target, MORSE_FLIPPER_ICR_NO_CHOICE, MORSE_FLIPPER_ICR_TIMEOUT_MS);
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
    state->pressed_choice = MORSE_FLIPPER_ICR_NO_CHOICE;
    state->answer_key_held = false;
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

static void
    morse_flipper_icr_answer(MorseFlipperIcrState* state, uint8_t choice_pos, uint32_t now_ms) {
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
        state->next_at =
            now_ms + (MORSE_FLIPPER_ICR_DIT_MS * cw_symbol_units(code, state->mark_idx));
    }
}

void* morse_flipper_icr_runtime_alloc(void) {
    return calloc(1U, sizeof(MorseFlipperIcrState));
}

void morse_flipper_icr_runtime_free(void* value) {
    free(value);
}

static void morse_flipper_icr_settings_run(MorseFlipperIcrState* state, DialogsApp* dialogs) {
    DialogMessage* message;

    if(state == NULL || dialogs == NULL) return;
    message = dialog_message_alloc();
    dialog_message_set_header(message, "ICR", 64, 10, AlignCenter, AlignTop);
    dialog_message_set_text(message, "Reset statistics?", 64, 30, AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, "No", "Yes", NULL);
    if(dialog_message_show(dialogs, message) == DialogMessageButtonCenter) {
        morse_flipper_icr_stats_reset(&state->stats);
        bool saved = morse_flipper_icr_stats_save(&state->stats);
        dialog_message_set_text(
            message,
            saved ? "Statistics reset" : "Could not save",
            64,
            30,
            AlignCenter,
            AlignCenter);
        dialog_message_set_buttons(message, NULL, "OK", NULL);
        dialog_message_show(dialogs, message);
    }
    dialog_message_free(message);
}

bool morse_flipper_icr_runtime_enter(
    void* value,
    const MorseFlipperIcrEnterArgs* args,
    MorseFlipperIcrResult* initial) {
    MorseFlipperIcrState* state = value;

    if(state == NULL || args == NULL || initial == NULL) return false;

    if(args->entry_kind == MorseFlipperIcrEntrySettings && args->dialogs != NULL) {
        morse_flipper_icr_settings_run(state, args->dialogs);
        *initial = (MorseFlipperIcrResult){.handled = true, .request_exit = true};
        return true;
    }
    if(args->entry_kind != MorseFlipperIcrEntryTraining ||
       !morse_flipper_icr_stats_load(&state->stats))
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

MorseFlipperIcrResult
    morse_flipper_icr_runtime_input(void* value, const InputEvent* event, uint32_t now_ms) {
    MorseFlipperIcrState* state = value;

    if(state == NULL || event == NULL) return (MorseFlipperIcrResult){0};
    state->feedback = MorseFlipperIcrFeedbackNone;

    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        MorseFlipperIcrResult result = morse_flipper_icr_result(state, false);
        result.request_exit = true;
        return result;
    }

    if(state->phase == MorseFlipperIcrPhaseRecognition && event->key == InputKeyOk &&
       event->type == InputTypePress) {
        state->pending_reaction_ms = now_ms - state->reaction_started_at;
        state->phase = MorseFlipperIcrPhaseRecognizedHold;
        return morse_flipper_icr_result(state, true);
    }

    if(state->phase == MorseFlipperIcrPhaseRecognizedHold && event->key == InputKeyOk &&
       event->type == InputTypeRelease) {
        morse_flipper_icr_open_answer_guard(state, now_ms);
        return morse_flipper_icr_result(state, true);
    }

    if(state->phase == MorseFlipperIcrPhaseAnswer && event->type == InputTypePress) {
        uint8_t choice_pos = morse_flipper_icr_choice_from_key(event->key);
        if(choice_pos < MORSE_FLIPPER_ICR_CHOICE_COUNT) {
            state->pressed_choice = choice_pos;
            state->answer_key_held = true;
            state->guard_until = 0U;
            return morse_flipper_icr_result(state, true);
        }
    }

    if(state->phase == MorseFlipperIcrPhaseAnswer && event->type == InputTypeRelease) {
        uint8_t choice_pos = morse_flipper_icr_choice_from_key(event->key);
        if(choice_pos < MORSE_FLIPPER_ICR_CHOICE_COUNT) {
            state->pressed_choice = choice_pos;
            state->answer_key_held = false;
            state->guard_until = now_ms + MORSE_FLIPPER_ICR_RELEASE_POP_MS;
            morse_flipper_icr_answer(state, choice_pos, now_ms);
        }
        return morse_flipper_icr_result(state, true);
    }

    return morse_flipper_icr_result(state, false);
}

MorseFlipperIcrResult morse_flipper_icr_runtime_tick(void* value, uint32_t now_ms) {
    MorseFlipperIcrState* state = value;

    if(state == NULL) return (MorseFlipperIcrResult){0};
    state->feedback = MorseFlipperIcrFeedbackNone;

    if(state->phase == MorseFlipperIcrPhaseGraphWait) {
        if(morse_flipper_time_reached(now_ms, state->next_at)) {
            morse_flipper_icr_begin_prompt(state, now_ms);
            return morse_flipper_icr_result(state, true);
        }
        if(state->target < MORSE_FLIPPER_ICR_CHAR_COUNT &&
           morse_flipper_time_reached(now_ms, state->guard_until)) {
            do {
                state->guard_until += MORSE_FLIPPER_ICR_FLASH_STEP_MS;
            } while(morse_flipper_time_reached(now_ms, state->guard_until) &&
                    morse_flipper_time_pending(state->guard_until, state->next_at));
            return morse_flipper_icr_result(state, true);
        }
    } else if(
        state->phase == MorseFlipperIcrPhasePlayback &&
        morse_flipper_time_reached(now_ms, state->next_at)) {
        morse_flipper_icr_tick_playback(state, now_ms);
        return morse_flipper_icr_result(state, true);
    } else if(
        state->phase == MorseFlipperIcrPhaseRecognition &&
        morse_flipper_time_reached(now_ms, state->next_at)) {
        morse_flipper_icr_finish_timeout(state, now_ms);
        return morse_flipper_icr_result(state, true);
    } else if(
        state->phase == MorseFlipperIcrPhaseAnswerGuard &&
        morse_flipper_time_reached(now_ms, state->guard_until)) {
        state->phase = MorseFlipperIcrPhaseAnswer;
        return morse_flipper_icr_result(state, true);
    } else if(
        state->phase == MorseFlipperIcrPhaseResult &&
        morse_flipper_time_reached(now_ms, state->result_until)) {
        state->feedback = MorseFlipperIcrFeedbackClear;
        morse_flipper_icr_begin_wait(state, now_ms);
        return morse_flipper_icr_result(state, true);
    } else if(
        state->phase == MorseFlipperIcrPhaseResult && !state->answer_key_held &&
        state->pressed_choice < MORSE_FLIPPER_ICR_CHOICE_COUNT &&
        morse_flipper_time_reached(now_ms, state->guard_until)) {
        state->pressed_choice = MORSE_FLIPPER_ICR_NO_CHOICE;
        state->guard_until = 0U;
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
            MORSE_FLIPPER_ICR_GRAPH_RECOGNIZED_H + 1U);
    }
    if(bucket <= MORSE_FLIPPER_ICR_1000MS_BUCKET) {
        return morse_flipper_icr_scale_bar_height(
            bucket,
            MORSE_FLIPPER_ICR_INSTANT_BUCKET,
            MORSE_FLIPPER_ICR_1000MS_BUCKET,
            MORSE_FLIPPER_ICR_GRAPH_RECOGNIZED_H,
            MORSE_FLIPPER_ICR_GRAPH_Q_H + 1U);
    }
    return morse_flipper_icr_scale_bar_height(
        bucket,
        MORSE_FLIPPER_ICR_1000MS_BUCKET,
        MORSE_FLIPPER_ICR_TIMEOUT_BUCKET,
        MORSE_FLIPPER_ICR_GRAPH_Q_H,
        MORSE_FLIPPER_ICR_GRAPH_MIN_H);
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
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, x, top, 2U, h);
    canvas_set_color(canvas, ColorWhite);
    if(h > MORSE_FLIPPER_ICR_GRAPH_Q_H) {
        canvas_draw_box(canvas, x, base_y - MORSE_FLIPPER_ICR_GRAPH_Q_H, 2U, 1U);
    }
    if(h > MORSE_FLIPPER_ICR_GRAPH_RECOGNIZED_H) {
        canvas_draw_box(canvas, x, base_y - MORSE_FLIPPER_ICR_GRAPH_RECOGNIZED_H, 2U, 1U);
    }
    canvas_set_color(canvas, ColorBlack);
}

static bool morse_flipper_icr_flash_visible(const MorseFlipperIcrState* state, uint32_t now_ms) {
    uint32_t remaining;
    uint32_t elapsed;

    if(state->phase != MorseFlipperIcrPhaseGraphWait) return true;
    if(state->target >= MORSE_FLIPPER_ICR_CHAR_COUNT ||
       !morse_flipper_time_pending(now_ms, state->next_at))
        return true;

    remaining = state->next_at - now_ms;
    elapsed = remaining >= MORSE_FLIPPER_ICR_FLASH_MS ? 0U :
                                                        MORSE_FLIPPER_ICR_FLASH_MS - remaining;
    return ((elapsed / MORSE_FLIPPER_ICR_FLASH_STEP_MS) & 1U) == 0U;
}

static void
    morse_flipper_icr_draw_graph(Canvas* canvas, MorseFlipperIcrState* state, uint32_t now_ms) {
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
    uint8_t choice,
    bool selected,
    bool pressed_black) {
    char text[2] = {
        choice < MORSE_FLIPPER_ICR_CHAR_COUNT ? morse_flipper_icr_char_at(choice) : '?',
        '\0',
    };

    canvas_set_color(canvas, selected && pressed_black ? ColorWhite : ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, selected ? x + 1 : x, y + (selected ? 2 : 1), AlignCenter, AlignCenter, text);
    if(selected) canvas_set_color(canvas, ColorBlack);
}

static void
    morse_flipper_icr_draw_pressed_shape(Canvas* canvas, uint8_t choice, bool pressed_black) {
    const uint8_t* bitmap;
    int32_t x;
    int32_t y;
    size_t width;
    size_t height;

    switch(choice) {
    case 0U:
        x = 20;
        y = 5;
        width = 19U;
        height = 14U;
        bitmap = pressed_black ? morse_flipper_icr_choice_pressed_black_0 :
                                 morse_flipper_icr_choice_pressed_white_0;
        break;
    case 1U:
        x = 20;
        y = 45;
        width = 19U;
        height = 14U;
        bitmap = pressed_black ? morse_flipper_icr_choice_pressed_black_1 :
                                 morse_flipper_icr_choice_pressed_white_1;
        break;
    case 2U:
        x = 2;
        y = 22;
        width = 14U;
        height = 19U;
        bitmap = pressed_black ? morse_flipper_icr_choice_pressed_black_2 :
                                 morse_flipper_icr_choice_pressed_white_2;
        break;
    case 3U:
        x = 44;
        y = 22;
        width = 14U;
        height = 19U;
        bitmap = pressed_black ? morse_flipper_icr_choice_pressed_black_3 :
                                 morse_flipper_icr_choice_pressed_white_3;
        break;
    case 4U:
        x = 20;
        y = 22;
        width = 20U;
        height = 20U;
        bitmap = pressed_black ? morse_flipper_icr_choice_pressed_black_4 :
                                 morse_flipper_icr_choice_pressed_white_4;
        break;
    default:
        return;
    }

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, x, y, width, height);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_bitmap(canvas, x, y, width, height, bitmap);
}

static uint8_t morse_flipper_icr_selected_choice(const MorseFlipperIcrState* state) {
    if(state->phase != MorseFlipperIcrPhaseAnswer && state->phase != MorseFlipperIcrPhaseResult)
        return MORSE_FLIPPER_ICR_NO_CHOICE;
    return state->pressed_choice;
}

static void morse_flipper_icr_draw_choices(Canvas* canvas, const MorseFlipperIcrState* state) {
    static const uint8_t choice_x[MORSE_FLIPPER_ICR_CHOICE_COUNT] = {29U, 29U, 9U, 49U, 29U};
    static const uint8_t choice_y[MORSE_FLIPPER_ICR_CHOICE_COUNT] = {12U, 50U, 31U, 31U, 31U};
    uint8_t selected = morse_flipper_icr_selected_choice(state);
    bool pressed_black = selected < MORSE_FLIPPER_ICR_CHOICE_COUNT && state->answer_key_held;

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_bitmap(canvas, 2, 5, 56, 54, morse_flipper_icr_choice_outline);
    if(selected < MORSE_FLIPPER_ICR_CHOICE_COUNT)
        morse_flipper_icr_draw_pressed_shape(canvas, selected, pressed_black);

    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHOICE_COUNT; i++) {
        bool is_selected = i == selected;
        int32_t x = choice_x[i];

        if(is_selected && i == 2U) x--;
        morse_flipper_icr_draw_choice(
            canvas, x, choice_y[i], state->choices[i], is_selected, pressed_black);
    }
}

static void morse_flipper_icr_draw_answer_hint(Canvas* canvas) {
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 66, 8, AlignLeft, AlignBottom, "Press");
    canvas_draw_str_aligned(canvas, 66, 18, AlignLeft, AlignBottom, "the");
    canvas_draw_str_aligned(canvas, 66, 28, AlignLeft, AlignBottom, "joystick");
    canvas_draw_str_aligned(canvas, 66, 38, AlignLeft, AlignBottom, "for");
    canvas_draw_str_aligned(canvas, 66, 48, AlignLeft, AlignBottom, "your");
    canvas_draw_str_aligned(canvas, 66, 58, AlignLeft, AlignBottom, "answer");
}

static uint16_t morse_flipper_icr_prompt_row(uint8_t ch, uint8_t row) {
    switch(ch) {
#define MF_ICR_PROMPT_CASE(symbol, advance, height, offset, ...) \
    case symbol: {                                               \
        static const uint16_t rows[24] = {__VA_ARGS__};          \
        return rows[row];                                        \
    }
        MORSE_FLIPPER_TERMINUS24_GLYPHS(MF_ICR_PROMPT_CASE)
#undef MF_ICR_PROMPT_CASE
    default:
        return 0U;
    }
}

static void morse_flipper_icr_draw_prompt(Canvas* canvas, uint8_t ch) {
    const int32_t x0 = 92 - 6;
    const int32_t y0 = 36 - 12;

    for(uint8_t row = 0U; row < 24U; row++) {
        uint16_t bits = morse_flipper_icr_prompt_row(ch, row);
        for(uint8_t col = 0U; col < 12U; col++) {
            if((bits & (uint16_t)(1U << (11U - col))) != 0U)
                canvas_draw_dot(canvas, x0 + col, y0 + row);
        }
    }
}

void morse_flipper_icr_runtime_draw(void* value, Canvas* canvas, uint32_t now_ms) {
    MorseFlipperIcrState* state = value;

    if(state == NULL || canvas == NULL) return;
    if(state->phase < MorseFlipperIcrPhaseAnswerGuard) {
        morse_flipper_icr_draw_graph(canvas, state, now_ms);
        return;
    }

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 127, 8, AlignRight, AlignBottom, morse_flipper_icr_phase_label(state));
    if(state->choice != MORSE_FLIPPER_ICR_NO_CHOICE || state->phase != MorseFlipperIcrPhaseResult)
        morse_flipper_icr_draw_choices(canvas, state);

    if(state->phase == MorseFlipperIcrPhaseResult) {
        morse_flipper_icr_draw_prompt(canvas, morse_flipper_icr_char_at(state->target));
    } else {
        morse_flipper_icr_draw_answer_hint(canvas);
    }
}
