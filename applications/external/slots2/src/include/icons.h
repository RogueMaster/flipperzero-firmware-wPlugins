/*
 * File: icons.h
 * Author: vh8t
 * Created: 28/12/2025
 */

#ifndef ICONS_H
#define ICONS_H

#include <stdint.h>

#include "slots.h"

typedef enum {
    ICON_ARROW_DOWN,
    ICON_ARROW_UP,
    ICON_BAR,
    ICON_BELL,
    ICON_CHERRY,
    ICON_DIAMOND,
    ICON_GRAPES,
    ICON_LEMON,
    ICON_ORANGE,
    ICON_PLUM,
    ICON_SEVEN,
    ICON_WATERMELON,
    ICON_MAX,
} icon_t;

icon_t get_icon_from_symbol(symbol_t sym);
const uint8_t* get_icon(icon_t icon);

#endif // ICONS_H
