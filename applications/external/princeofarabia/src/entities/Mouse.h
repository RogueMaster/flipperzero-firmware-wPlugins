#pragma once

#include <lib/Arduboy2.h>
#include "../utils/Constants.h"
#include "../utils/Enums.h"

struct Mouse {
    uint8_t counter;
    uint8_t exits; // How many entry eots have we done?

    void init() {
        this->counter = 0;
        this->exits = 0;
    }

    bool update() {
        uint8_t counter = this->counter;
        if(counter != 0) --counter;
        this->counter = counter;

        return this->counter == 70;
    }
};
