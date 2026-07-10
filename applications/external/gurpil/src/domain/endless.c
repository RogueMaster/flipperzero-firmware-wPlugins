#include "include/domain/endless.h"

uint32_t endless_checkpoint_bonus_ms(uint32_t checkpoints_hit) {
    uint32_t decay = checkpoints_hit * ENDLESS_CHECKPOINT_BONUS_DECAY_MS;
    uint32_t range = ENDLESS_CHECKPOINT_BONUS_BASE_MS - ENDLESS_CHECKPOINT_BONUS_FLOOR_MS;
    if (decay >= range) {
        return ENDLESS_CHECKPOINT_BONUS_FLOOR_MS;
    }
    return ENDLESS_CHECKPOINT_BONUS_BASE_MS - decay;
}

void endless_init(EndlessState *state) {
    state->phase = EndlessIdle;
    state->time_left_ms = ENDLESS_START_TIME_MS;
    state->distance = 0;
    state->next_checkpoint = ENDLESS_CHECKPOINT_SPACING;
    state->checkpoints_hit = 0;
    state->last_bonus_ms = 0;
}

void endless_start(EndlessState *state) {
    if (state->phase == EndlessIdle) {
        state->phase = EndlessRunning;
    }
}

void endless_tick(EndlessState *state, uint32_t dt_ms, int32_t distance) {
    if (state->phase != EndlessRunning) {
        return;
    }

    if (distance > state->distance) {
        state->distance = distance;
    }

    // Saturating subtract: a dt_ms larger than the remaining budget clamps to 0 instead of
    // wrapping around (time_left_ms is unsigned, so plain subtraction could underflow).
    if (dt_ms >= state->time_left_ms) {
        state->time_left_ms = 0;
    } else {
        state->time_left_ms -= dt_ms;
    }

    // Loop so a single big distance jump that crosses several checkpoints at once still grants a
    // (decaying) bonus and a next_checkpoint advance for each one crossed.
    while (state->distance >= state->next_checkpoint) {
        uint32_t bonus = endless_checkpoint_bonus_ms(state->checkpoints_hit);
        uint32_t bonus_total = state->time_left_ms + bonus;
        state->time_left_ms = bonus_total > ENDLESS_MAX_TIME_MS ? ENDLESS_MAX_TIME_MS : bonus_total;
        state->last_bonus_ms = bonus;
        state->next_checkpoint += ENDLESS_CHECKPOINT_SPACING;
        state->checkpoints_hit++;
    }

    if (state->time_left_ms == 0) {
        state->phase = EndlessOver;
    }
}
