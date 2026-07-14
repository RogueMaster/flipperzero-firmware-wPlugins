#include <stdint.h>
#include <stdio.h>

#include "include/icons.h"
#include "include/slots.h"

#include "icons/arrow_down.h"
#include "icons/arrow_up.h"
#include "icons/bar.h"
#include "icons/bell.h"
#include "icons/cherry.h"
#include "icons/diamond.h"
#include "icons/grapes.h"
#include "icons/lemon.h"
#include "icons/orange.h"
#include "icons/plum.h"
#include "icons/seven.h"
#include "icons/watermelon.h"

icon_t get_icon_from_symbol(symbol_t sym) {
    switch(sym) {
    case SYM_CHERRY:
        return ICON_CHERRY;
    case SYM_LEMON:
        return ICON_LEMON;
    case SYM_ORANGE:
        return ICON_ORANGE;
    case SYM_PLUM:
        return ICON_PLUM;
    case SYM_GRAPES:
        return ICON_GRAPES;
    case SYM_WATERMELON:
        return ICON_WATERMELON;
    case SYM_BELL:
        return ICON_BELL;
    case SYM_BAR:
        return ICON_BAR;
    case SYM_SEVEN:
        return ICON_SEVEN;
    case SYM_DIAMOND:
        return ICON_DIAMOND;

    // Unreachable
    case SYM_MAX:
        return ICON_MAX;
    }
    return ICON_MAX;
}

const uint8_t* get_icon(icon_t icon) {
    switch(icon) {
    case ICON_ARROW_DOWN:
        return arrow_down_icon;
    case ICON_ARROW_UP:
        return arrow_up_icon;
    case ICON_BAR:
        return bar_icon;
    case ICON_BELL:
        return bell_icon;
    case ICON_CHERRY:
        return cherry_icon;
    case ICON_DIAMOND:
        return diamond_icon;
    case ICON_GRAPES:
        return grapes_icon;
    case ICON_LEMON:
        return lemon_icon;
    case ICON_ORANGE:
        return orange_icon;
    case ICON_PLUM:
        return plum_icon;
    case ICON_SEVEN:
        return seven_icon;
    case ICON_WATERMELON:
        return watermelon_icon;

        // Unreachable
    case ICON_MAX:
        return NULL;
    }
    return NULL;
}
