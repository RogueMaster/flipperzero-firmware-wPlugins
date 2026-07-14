#ifndef CARD_GRAPHICS_H
#define CARD_GRAPHICS_H

#include <gui/gui.h>
#include "blackjack.h"

void draw_card(
    Canvas* canvas,
    int x,
    int y,
    Card* card,
    bool hidden,
    int card_width,
    int card_height);

#endif
