#include "../jayx.h"

void draw_status_view(Canvas* canvas, JayxApp* app) {
    UNUSED(app);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignBottom, "Connection lost");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignBottom, "Waiting for PC...");
    canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignBottom, "Back to exit");
}
