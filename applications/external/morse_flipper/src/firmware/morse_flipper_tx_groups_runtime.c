/*
 * Purpose: Bridge TX Groups drills into app input, audio, and screen flow.
 * Owns: TX Groups session lifecycle, runtime ticking, and result transitions.
 * Depends on: morse_flipper_app_i.h and morse_flipper_tx_groups.h.
 * Tests: tests/test_tx_groups.c covers the host model.
 */

#include "morse_flipper_app_i.h"

static bool morse_flipper_tx_groups_sk_now(const MorseFlipperApp* app) {
    if(app == NULL) return true;
    if(app->input_source == MorseFlipperInputSourceStraight) return true;
    if(app->input_source == MorseFlipperInputSourcePaddle &&
       morse_flipper_gpio_probe_use_straight(app))
        return true;
    if(app->input_source == MorseFlipperInputSourceButtons &&
       morse_flipper_straight_like_mode(app))
        return true;
    return false;
}

static uint8_t morse_flipper_txg_clean_difficulty(uint8_t difficulty) {
    return difficulty < MorseFlipperTxgDifficultyCount ? difficulty :
                                                         MorseFlipperTxgDifficultyCompetition;
}

const char* morse_flipper_txg_difficulty_name(uint8_t difficulty) {
    static const char* const names[] = {"Easy", "Medium", "IARU HST"};

    difficulty = morse_flipper_txg_clean_difficulty(difficulty);
    return names[difficulty];
}

uint8_t morse_flipper_txg_range_low(uint8_t difficulty) {
    static const uint8_t lows[] = {75U, 80U, 90U};

    difficulty = morse_flipper_txg_clean_difficulty(difficulty);
    return lows[difficulty];
}

uint8_t morse_flipper_txg_range_high(uint8_t difficulty) {
    static const uint8_t highs[] = {125U, 120U, 110U};

    difficulty = morse_flipper_txg_clean_difficulty(difficulty);
    return highs[difficulty];
}

void morse_flipper_reset_tx_groups_state(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;
    UNUSED(now_ms);

    app->txg_started = false;
    app->txg_wait_answer = false;
    app->txg_done = false;
    app->txg_sk = morse_flipper_tx_groups_sk_now(app);
    app->txg_start_holdoff = false;
    app->txg_wait_started_at = 0U;
    app->txg_last_input_at = 0U;
    app->txg_result_open_at = 0U;
    app->txg_result_until = 0U;
    app->txg_result_draw_s = 0xFFU;
    app->txg_repeated_timeouts = 0U;
    app->txg_session_total = 0U;
    app->txg_session_good = 0U;
    app->txg_session_sk = 0U;
    app->txg_sum_speed = 0U;
    app->txg_sum_lgap = 0U;
    app->txg_sum_ratio = 0U;
    app->txg_sum_accuracy = 0U;
    app->txg_sum_dgap = 0U;
    app->txg_sum_variance = 0U;
    morse_flipper_tx_group_init(&app->tx_group);
    morse_flipper_tx_group_set_seed(&app->tx_group, furi_hal_random_get());
    morse_flipper_cw_decoder_init(&app->tx_decoder, morse_flipper_current_dit_ms(app));
    morse_flipper_clear_button_keying(app, furi_get_tick());
}

void morse_flipper_start_tx_groups_round(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;

    if(!app->txg_started) {
        app->txg_session_total = 0U;
        app->txg_session_good = 0U;
        app->txg_repeated_timeouts = 0U;
    }

    app->txg_started = true;
    app->txg_wait_answer = true;
    app->txg_done = false;
    app->txg_sk = morse_flipper_tx_groups_sk_now(app);
    app->txg_start_holdoff = false;
    app->txg_wait_started_at = now_ms;
    app->txg_last_input_at = now_ms;
    app->txg_result_open_at = 0U;
    app->txg_result_until = 0U;
    app->txg_result_draw_s = 0xFFU;
    morse_flipper_tx_group_set_range(
        &app->tx_group,
        morse_flipper_txg_range_low(app->txg_difficulty),
        morse_flipper_txg_range_high(app->txg_difficulty));
    morse_flipper_tx_group_start(&app->tx_group, app->txg_sk);
    morse_flipper_cw_decoder_init(&app->tx_decoder, morse_flipper_current_dit_ms(app));
    app->rf_tx_edge_at = 0U;
    app->rf_tx_gap_flushed = true;
    app->rf_tx_level = false;
    morse_flipper_refresh_keyer(app, now_ms);
    morse_flipper_view_dirty(app);
}

void morse_flipper_leave_tx_groups(MorseFlipperApp* app, uint32_t now_ms) {
    if(app == NULL) return;
    morse_flipper_gpio_probe_reset(app);
    app->txg_result_open_at = 0U;
    app->txg_result_until = 0U;
    app->txg_result_draw_s = 0xFFU;
    app->txg_start_holdoff = false;
    morse_flipper_clear_button_keying(app, now_ms);
    morse_flipper_release_all_notes(app);
    scene_manager_search_and_switch_to_another_scene(
        app->scene_manager, MorseFlipperSceneTxGroupsFinal);
}
