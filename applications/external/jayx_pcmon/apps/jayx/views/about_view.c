#include "../jayx.h"

void draw_about_view(Canvas* canvas, JayxApp* app) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, "JAYX");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignBottom, "PC Monitor  v" JAYX_VERSION);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignBottom, "by " JAYX_AUTHOR);

    char line[32];
    snprintf(line, sizeof(line), "Protocol v%u · scroll specs", (unsigned)JAYX_PROTO_VER);
    canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignBottom, line);

    const char* link = "Link: -";
    if(app->transport == JayxTransportUsb) {
        link = "Link: USB";
    } else if(app->transport == JayxTransportBt) {
        link = "Link: Bluetooth";
    }
    canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignBottom, link);

    jayx_draw_page_dots(canvas, JayxPageAbout);
}
