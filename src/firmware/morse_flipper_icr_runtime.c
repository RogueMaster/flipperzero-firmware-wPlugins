/*
 * Purpose: Manage ICR runtime lifetime and first-screen drawing.
 * Owns: ICR stats allocation, scene reset, and ICR canvas rendering.
 * Depends on: morse_flipper_app_i.h and morse_flipper_icr.h.
 * Tests: firmware build; ICR model tests stay private.
 */

#include "morse_flipper_app_i.h"

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

    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_ICR_PLAYBACK, false);
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

    morse_flipper_set_note_source(app, 0U, MORSE_SOURCE_ICR_PLAYBACK, false);
    app->icr_playback_mark = false;
    morse_flipper_release_icr_stats(app, true);
    morse_flipper_update_sidetone(app);
}

void morse_flipper_draw_icr(Canvas* canvas, MorseFlipperApp* app) {
    UNUSED(app);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignCenter, "ICR");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "Preparing practice");
    canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignBottom, "Back exits");
}
