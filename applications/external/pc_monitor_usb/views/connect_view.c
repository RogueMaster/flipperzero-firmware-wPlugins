#include "connect_view.h"

void draw_connect_view(Canvas* canvas) {
    canvas_draw_str(canvas, 1, 10, "Waiting for connection...");

    canvas_draw_str(canvas, 1, 40, "Download back-end");
    canvas_draw_str(canvas, 1, 50, "app for your PC from:");

    canvas_draw_icon(canvas, 128 - 34, 64 - 34, &I_qr_33px);
}
