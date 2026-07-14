#ifndef SPRITE_FRAME_H_
#define SPRITE_FRAME_H_

#include "lib/Arduino.h"

struct SpriteFrame {
    uint16_t offset;
    uint8_t width, height;
    uint8_t xOffset;
};

#endif
