#include "app_elements.h"
#include "t_union_master_icons.h"

void elements_draw_travel_attr_icons(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    TUnionTravel* travel,
    TUnionTravelExt* travel_ext) {
    // 进出站图标
    if(travel->type == 0x03)
        canvas_draw_icon(canvas, x, y, &I_ButtonUp_7x4);
    else if(travel->type == 0x04)
        canvas_draw_icon(canvas, x + 8, y, &I_ButtonDown_7x4);

    // 换乘图标
    if(travel_ext->transfer) canvas_draw_icon(canvas, x + 2 * 8, y, &I_transfer_7x4);

    // 夜间图标
    if(travel_ext->night) canvas_draw_icon(canvas, x + 3 * 8, y, &I_night_7x4);

    // 漫游图标
    if(travel_ext->roaming) canvas_draw_icon(canvas, x + 4 * 8, y, &I_roaming_7x4);
}

// 翻页箭头
void elements_draw_page_cursor(Canvas* canvas, uint8_t index, uint8_t max) {
    if(index != 0) canvas_draw_icon(canvas, 118, 3, &I_ButtonUp_7x4);
    if(index != max - 1) canvas_draw_icon(canvas, 118, 60, &I_ButtonDown_7x4);
}
