#pragma once

#include <stdint.h>

typedef enum {
    DndRollNormal,
    DndRollAdvantage,
    DndRollDisadvantage,
    DndRollGuidance,
} DndRollMode;

uint8_t dndolphins_dice_roll_d20_mode(DndRollMode mode);
uint16_t
    dndolphins_dice_roll_values(uint8_t count, uint8_t sides, uint8_t* values, uint8_t capacity);
