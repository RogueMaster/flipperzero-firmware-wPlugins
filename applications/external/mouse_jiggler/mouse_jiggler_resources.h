#pragma once

#include <stdint.h>

static const uint8_t MOUSE_JIGGLER_IMAGE_BACK_BUTTON[] =
    {0x04, 0x00, 0x06, 0x00, 0xff, 0x00, 0x06, 0x01, 0x04, 0x02, 0x00, 0x02, 0x00, 0x01, 0xf8, 0x00};
static const uint8_t MOUSE_JIGGLER_IMAGE_OK_BUTTON[] = {0x1c, 0x22, 0x5d, 0x5d, 0x5d, 0x22, 0x1c};
static const uint8_t MOUSE_JIGGLER_IMAGE_DOWN_BUTTON[] = {0x7f, 0x3e, 0x1c, 0x08};
static const uint8_t MOUSE_JIGGLER_IMAGE_UP_BUTTON[] = {0x08, 0x1c, 0x3e, 0x7f};
static const char* MOUSE_JIGGLER_INTERVAL_STR[] = {
    "500 ms",
    "1 s",
    "2 s",
    "5 s",
    "10 s",
    "15 s",
    "30 s",
    "1 min",
    "1.5 min",
    "2 min",
    "5 min",
    "10 min"};
