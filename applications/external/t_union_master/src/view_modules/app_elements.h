#pragma once

#include <furi.h>
#include <gui/canvas.h>
#include "../protocol/t_union_msg.h"
#include "../utils/t_union_msgext.h"

void elements_draw_travel_attr_icons(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    TUnionTravel* travel,
    TUnionTravelExt* travel_ext);
void elements_draw_page_cursor(Canvas* canvas, uint8_t index, uint8_t max);
