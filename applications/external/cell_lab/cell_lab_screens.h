#pragma once

#include "cell_lab_transition.h"
#include "cell_lab_world.h"

#include <gui/canvas.h>

// RU: Контекст от приложения, который нужен рендеру, но не принадлежит модели мира.
// EN: App context needed by rendering but not owned by the world model.
typedef struct {
    bool running;
    uint8_t tick_delay_ms;
    uint8_t selected_world_setting;
    uint8_t selected_app_setting;
    uint32_t hud_until_tick;
    const CellLabTransition* transition;
} CellLabScreenContext;

// RU: Рисует выбранную страницу с горизонтальным смещением для анимации прокрутки.
// EN: Draws the selected page with a horizontal offset for scroll animation.
void cell_lab_screens_draw(
    Canvas* canvas,
    const CellLabWorld* world,
    const CellLabScreenContext* context,
    CellLabScreen screen,
    int16_t x_offset);
