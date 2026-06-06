#include "../../include/data/levels.h"
#include "levels_data.h"
#include <stddef.h>

uint16_t tutu_levels_count(void) {
    return TUTU_LEVEL_COUNT;
}

const TutuLevel *tutu_levels_get(uint16_t index) {
    if (index >= TUTU_LEVEL_COUNT)
        return NULL;
    return &TUTU_LEVELS[index];
}
