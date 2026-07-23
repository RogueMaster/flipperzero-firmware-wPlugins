#include "../jayx.h"

static void draw_net_panel(Canvas* canvas, int32_t y, const char* label, const char* value) {
    canvas_draw_rframe(canvas, 4, y, 120, 17, 2);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 10, y + 12, label);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 118, y + 12, AlignRight, AlignBottom, value);
}

void draw_network_view(Canvas* canvas, JayxApp* app) {
    char up_str[32];
    char down_str[32];
    char unit[4];

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    /* Top bar */
    canvas_draw_box(canvas, 0, 0, 128, JAYX_SPECS_HEADER_H);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 8, "NETWORK");
    canvas_set_color(canvas, ColorBlack);

    if(app->net_valid) {
        jayx_unit_cstr(unit, sizeof(unit), app->net_up_unit);
        snprintf(up_str, sizeof(up_str), "%.1f%s/s", (double)(app->net_up_val * 0.1f), unit);

        jayx_unit_cstr(unit, sizeof(unit), app->net_down_unit);
        snprintf(down_str, sizeof(down_str), "%.1f%s/s", (double)(app->net_down_val * 0.1f), unit);
    } else {
        snprintf(up_str, sizeof(up_str), "--");
        snprintf(down_str, sizeof(down_str), "--");
    }

    draw_net_panel(canvas, 14, "UP", up_str);
    draw_net_panel(canvas, 34, "DOWN", down_str);

    jayx_draw_page_dots(canvas, JayxPageNetwork);
}
