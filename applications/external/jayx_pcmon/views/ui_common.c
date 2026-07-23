#include "../jayx.h"

#define ROW_H 13

void jayx_unit_cstr(char* out, size_t n, const char unit[4]) {
    size_t i = 0;
    for(; i < 3 && i + 1 < n && unit[i]; i++) {
        out[i] = unit[i];
    }
    out[i] = '\0';
}

void jayx_draw_metric_row(
    Canvas* canvas,
    uint8_t row,
    const char* label,
    const char* value,
    float percent) {
    int32_t y0 = row * ROW_H;

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_disc(canvas, 3, y0 + 4, 1);
    canvas_draw_str(canvas, 7, y0 + 7, label);

    uint16_t val_w = canvas_string_width(canvas, value);
    canvas_draw_str(canvas, 125 - val_w, y0 + 7, value);

    if(percent >= 0.0f) {
        if(percent > 1.0f) percent = 1.0f;
        canvas_draw_frame(canvas, 2, y0 + 8, 124, 3);
        uint8_t fill = (uint8_t)(122.0f * percent);
        if(fill > 0) canvas_draw_box(canvas, 3, y0 + 9, fill, 1);
    }
}

void jayx_draw_page_dots(Canvas* canvas, JayxPage page) {
    const int spacing = 10;
    const int start_x = 64 - ((JAYX_PAGE_COUNT - 1) * spacing) / 2;

    for(int i = 0; i < JAYX_PAGE_COUNT; i++) {
        int x = start_x + i * spacing;
        if((JayxPage)i == page) {
            canvas_draw_disc(canvas, x, JAYX_DOT_Y, 2);
        } else {
            canvas_draw_circle(canvas, x, JAYX_DOT_Y, 2);
        }
    }
}
