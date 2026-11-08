#pragma once

#include <stdint.h>
#include <gui/icon.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOTERIA_CARD_COUNT 54

typedef struct {
    uint8_t id;
    const char* name;
    const char* rhyme;
    const Icon* icon;
} LoteriaCard;

extern const LoteriaCard loteria_cards[LOTERIA_CARD_COUNT];

#ifdef __cplusplus
}
#endif
