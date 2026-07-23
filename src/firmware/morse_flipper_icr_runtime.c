/*
 * Purpose: Manage ICR runtime lifetime and first-screen drawing.
 * Owns: ICR stats allocation, scene reset, and ICR canvas rendering.
 * Depends on: morse_flipper_app_i.h and morse_flipper_icr.h.
 * Tests: firmware build; ICR model tests stay private.
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
    app->icr_playback_mark = false;
    app->icr_next_at = now_ms + MORSE_FLIPPER_ICR_WAIT_MS;
    app->icr_reaction_started_at = 0U;
    app->icr_pending_reaction_ms = 0U;
    app->icr_guard_until = 0U;
    app->icr_result_until = 0U;
    app->icr_answer_correct = false;
    morse_flipper_update_sidetone(app);
    for(uint8_t i = 0U; i < MORSE_FLIPPER_ICR_CHOICE_COUNT; i++) {
        app->icr_choices[i] = MORSE_FLIPPER_ICR_NO_CHOICE;
    }
}

void morse_flipper_enter_icr(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    morse_flipper_ensure_icr_stats_loaded(app);
    if(app->icr_rng_state == 0U) app->icr_rng_state = now_ms ^ 0x49435231UL;
    morse_flipper_reset_icr_runtime(app, now_ms);
}

void morse_flipper_leave_icr(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;
    UNUSED(now_ms);

    app->icr_playback_mark = false;
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

static void morse_flipper_icr_finish_playback(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    app->icr_playback_mark = false;
    app->icr_phase = MorseFlipperIcrPhaseRecognition;
    app->icr_reaction_started_at = now_ms;
    app->icr_next_at = now_ms + MORSE_FLIPPER_ICR_TIMEOUT_MS;
    morse_flipper_update_sidetone(app);
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
    }
}

void morse_flipper_draw_icr(Canvas* canvas, MorseFlipperApp* app) {
    UNUSED(app);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignCenter, "ICR");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "Preparing practice");
    canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignBottom, "Back exits");
}
