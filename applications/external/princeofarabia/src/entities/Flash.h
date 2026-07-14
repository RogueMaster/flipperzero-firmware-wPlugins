#pragma once

#include <lib/Arduboy2.h>
#include "../utils/Constants.h"
#include "../utils/Enums.h"

struct Flash {
    uint8_t x;
    uint8_t y;
    uint8_t frame;
    FlashType type;
};
