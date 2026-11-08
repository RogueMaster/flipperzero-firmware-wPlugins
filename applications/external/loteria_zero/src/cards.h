#pragma once

#include "cards_data.h"

static inline const LoteriaCard* loteria_card_by_id(uint8_t id) {
    if(id < 1 || id > LOTERIA_CARD_COUNT) {
        return NULL;
    }
    return &loteria_cards[id - 1];
}
