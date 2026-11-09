#include "game.h"

#include <furi_hal_random.h>
#include <string.h>

void loteria_game_init(LoteriaGame* game) {
    memset(game, 0, sizeof(LoteriaGame));
}

static void loteria_game_shuffle(uint8_t* order, size_t count) {
    for(size_t i = 0; i < count; i++) {
        order[i] = (uint8_t)i;
    }
    for(size_t i = count - 1; i > 0; i--) {
        uint32_t r = furi_hal_random_get();
        size_t j = (size_t)(r % (i + 1));
        uint8_t tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
    }
}

void loteria_game_start_new(LoteriaGame* game) {
    loteria_game_shuffle(game->order, LOTERIA_CARD_COUNT);

    /* Guarantee a permutation (no duplicate card indices). */
    bool seen[LOTERIA_CARD_COUNT] = {false};
    bool unique = true;
    for(uint8_t i = 0; i < LOTERIA_CARD_COUNT; i++) {
        uint8_t v = game->order[i];
        if(v >= LOTERIA_CARD_COUNT || seen[v]) {
            unique = false;
            break;
        }
        seen[v] = true;
    }
    if(!unique) {
        for(uint8_t i = 0; i < LOTERIA_CARD_COUNT; i++) {
            game->order[i] = i;
        }
        loteria_game_shuffle(game->order, LOTERIA_CARD_COUNT);
    }

    game->active = true;
    game->finished = false;
    game->paused = false;
    /* Reveal first card immediately (index = sung count). */
    game->index = 1;
}

bool loteria_game_has_save(const LoteriaGame* game) {
    return game->active && !game->finished && game->index > 0;
}

const LoteriaCard* loteria_game_current_card(const LoteriaGame* game) {
    return loteria_game_card_at_lookback(game, 0);
}

const LoteriaCard* loteria_game_previous_card(const LoteriaGame* game) {
    return loteria_game_card_at_lookback(game, 1);
}

const LoteriaCard* loteria_game_card_at_lookback(const LoteriaGame* game, uint8_t lookback) {
    uint8_t sung = loteria_game_sung_count(game);
    if(sung == 0 || lookback >= sung) {
        return NULL;
    }
    uint8_t pos = (uint8_t)(sung - 1 - lookback);
    uint8_t idx = game->order[pos];
    if(idx >= LOTERIA_CARD_COUNT) {
        return NULL;
    }
    return &loteria_cards[idx];
}

uint8_t loteria_game_max_lookback(const LoteriaGame* game) {
    uint8_t sung = loteria_game_sung_count(game);
    if(sung <= 1) {
        return 0;
    }
    return (uint8_t)(sung - 1);
}

bool loteria_game_next(LoteriaGame* game) {
    if(!game->active || game->finished || game->paused) {
        return false;
    }
    /* Already showing the last card: finish on next press. */
    if(game->index >= LOTERIA_CARD_COUNT) {
        game->finished = true;
        game->active = false;
        return false;
    }
    game->index++;
    return true;
}

uint8_t loteria_game_sung_count(const LoteriaGame* game) {
    if(!game->active && !game->finished) {
        return 0;
    }
    if(game->index > LOTERIA_CARD_COUNT) {
        return LOTERIA_CARD_COUNT;
    }
    return game->index;
}

bool loteria_game_is_sung(const LoteriaGame* game, uint8_t card_id) {
    uint8_t sung = loteria_game_sung_count(game);
    for(uint8_t i = 0; i < sung; i++) {
        if(loteria_cards[game->order[i]].id == card_id) {
            return true;
        }
    }
    return false;
}

uint8_t loteria_game_order_position(const LoteriaGame* game, uint8_t card_id) {
    uint8_t sung = loteria_game_sung_count(game);
    for(uint8_t i = 0; i < sung; i++) {
        if(loteria_cards[game->order[i]].id == card_id) {
            return (uint8_t)(i + 1);
        }
    }
    return 0;
}
