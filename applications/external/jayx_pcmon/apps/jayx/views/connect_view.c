#include "../jayx.h"

void draw_connect_view(Canvas* canvas, JayxApp* app) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, "JAYX");

    if(app->ui_state == JayxUiBtDev) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignBottom, "Bluetooth is still");
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignBottom, "in development");
        canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignBottom, "Check GitHub");
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignBottom, "for updates");
        return;
    }

    if(app->ui_state == JayxUiModeSelect) {
        canvas_set_font(canvas, FontSecondary);

        const char* usb_line = app->select_cursor == JayxTransportUsb ? "> USB" : "  USB";
        const char* bt_line =
            app->select_cursor == JayxTransportBt ? "> Bluetooth (WIP)" : "  Bluetooth (WIP)";

        canvas_draw_str(canvas, 18, 28, usb_line);
        canvas_draw_str(canvas, 18, 40, bt_line);
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignBottom, "OK start  Back exit");
        return;
    }

    /* Waiting for link (USB only in practice) */
    canvas_set_font(canvas, FontSecondary);
    if(app->transport == JayxTransportUsb) {
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignBottom, "USB · waiting...");
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignBottom, "monitor.py --usb");
    } else {
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignBottom, "Select a link");
    }
}
