#pragma once

#include <lib/Arduboy2.h>
#include "../utils/Constants.h"
#include "../../fxdata/fxdata.h"

struct ImageDetails {
    int8_t reach;
    int8_t toe;
    int8_t heel;
};

struct Character {
    uint8_t x;
    uint8_t image;
};
