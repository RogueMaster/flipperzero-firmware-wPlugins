#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "cards_data.h"

typedef struct {
    uint8_t order[LOTERIA_CARD_COUNT];
    uint8_t index; /* next card to reveal; also count of sung cards */
    bool active;
    bool finished;
    bool paused;
} LoteriaGame;

void loteria_game_init(LoteriaGame* game);
void loteria_game_start_new(LoteriaGame* game);
bool loteria_game_has_save(const LoteriaGame* game);
const LoteriaCard* loteria_game_current_card(const LoteriaGame* game);
const LoteriaCard* loteria_game_previous_card(const LoteriaGame* game);
/* lookback 0 = current (last sung), 1 = one before, ... */
const LoteriaCard* loteria_game_card_at_lookback(const LoteriaGame* game, uint8_t lookback);
uint8_t loteria_game_max_lookback(const LoteriaGame* game);
bool loteria_game_next(LoteriaGame* game);
uint8_t loteria_game_sung_count(const LoteriaGame* game);
bool loteria_game_is_sung(const LoteriaGame* game, uint8_t card_id);
uint8_t loteria_game_order_position(const LoteriaGame* game, uint8_t card_id);
