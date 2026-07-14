#pragma once

#include <lib/Arduboy2.h>
#include "../utils/Constants.h"

struct MenuItem {
    Direction direction = Direction::None;
    uint8_t x;
    uint8_t cursor;

    void init() {
        this->x = 130;
        this->direction = Direction::None;
    }

    bool update() {
        bool result = false;
        uint8_t x = this->x;

        switch(this->direction) {
        case Direction::Left:

            x -= 4;

            if(x == 86) {
                this->direction = Direction::None;
            }

            break;

        case Direction::Right:

            x += 4;

            if(x == 130) {
                this->direction = Direction::None;
                result = true;
            }

            break;

        default:
            break;
        }
        this->x = x;
        return result;
    }
};
