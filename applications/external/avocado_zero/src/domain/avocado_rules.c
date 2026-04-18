#include "include/domain/avocado_rules.h"

#define SECONDS_PER_DAY (24u * 60u * 60u)

bool avocado_rules_is_game_over(const AvocadoData *state) {
    return state->dirty_level >= AVOCADO_GRIME_GAME_OVER;
}

bool avocado_rules_should_show_victory(const AvocadoData *state) {
    return !avocado_rules_is_game_over(state) && state->roots_length >= AVOCADO_ROOTS_MAX &&
           state->victory_seen == 0;
}

void avocado_rules_apply_elapsed_days(AvocadoData *state, uint32_t now_ts) {
    if (now_ts < state->last_timestamp) {
        state->last_timestamp = now_ts;
        return;
    }

    uint32_t elapsed = now_ts - state->last_timestamp;
    uint32_t days = elapsed / SECONDS_PER_DAY;
    if (days == 0) {
        return;
    }

    uint32_t sum = (uint32_t)state->dirty_level + days;
    if (sum > 255) {
        sum = 255;
    }
    state->dirty_level = (uint8_t)sum;
    state->last_timestamp = now_ts;
}

void avocado_rules_on_primary_action(AvocadoData *state) {
    if (avocado_rules_is_game_over(state)) {
        state->dirty_level = 0;
        state->roots_length = 0;
        state->victory_seen = 0;
    } else {
        const bool had_grime = state->dirty_level > 0;
        state->dirty_level = 0;
        /* Roots only when cleaning real grime; spamming Clean on clear water does nothing. */
        if (had_grime && state->roots_length < AVOCADO_ROOTS_MAX) {
            state->roots_length++;
            if (state->roots_length == AVOCADO_ROOTS_MAX) {
                state->victory_seen = 0;
            }
        }
    }
}

void avocado_rules_acknowledge_victory(AvocadoData *state, uint32_t now_ts) {
    if (state->roots_length < AVOCADO_ROOTS_MAX) {
        return;
    }
    state->victory_seen = 1;
    state->roots_length = 0;
    state->dirty_level = 0;
    state->last_timestamp = now_ts;
}

void avocado_rules_debug_add_simulated_days(AvocadoData *state, uint32_t days) {
    if (days == 0u || avocado_rules_is_game_over(state)) {
        return;
    }
    uint32_t sum = (uint32_t)state->dirty_level + days;
    if (sum > 255u) {
        sum = 255u;
    }
    state->dirty_level = (uint8_t)sum;
    state->last_timestamp += days * SECONDS_PER_DAY;
}

void avocado_rules_debug_bump_roots(AvocadoData *state) {
    if (avocado_rules_is_game_over(state) || state->roots_length >= AVOCADO_ROOTS_MAX) {
        return;
    }
    state->dirty_level = 0;
    state->roots_length++;
    if (state->roots_length == AVOCADO_ROOTS_MAX) {
        state->victory_seen = 0;
    }
}

void avocado_rules_debug_preset_victory_pending(AvocadoData *state) {
    state->dirty_level = 0;
    state->roots_length = AVOCADO_ROOTS_MAX;
    state->victory_seen = 0;
}

void avocado_rules_debug_preset_game_over(AvocadoData *state) {
    state->dirty_level = AVOCADO_GRIME_GAME_OVER;
}
