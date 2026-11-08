#pragma once

#include <gui/canvas.h>
#include <gui/icon.h>

#include "cards_data.h"
#include "game.h"

typedef struct {
    const LoteriaCard* card;
    uint8_t sung;
    uint8_t total;
    bool paused;
    bool showing_previous;
    bool finished;
    uint8_t auto_mode;
    int8_t rhyme_scroll;
} LoteriaRenderModel;

void loteria_renderer_draw_card(Canvas* canvas, const LoteriaRenderModel* model);
