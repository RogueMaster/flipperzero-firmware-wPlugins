#pragma once

#include <stdint.h>

#define AVOCADO_GRIME_GAME_OVER 10u
#define AVOCADO_ROOTS_MAX 10u

typedef struct {
    uint32_t last_timestamp;
    uint8_t dirty_level;
    uint8_t roots_length;
    uint8_t victory_seen;
} AvocadoData;
