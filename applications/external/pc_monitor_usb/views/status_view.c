#include "status_view.h"

void draw_status_view(Canvas* canvas, void* ctx) {
    PcMonitorApp* app = ctx;

    canvas_draw_str_aligned(
        canvas,
        64,
        32,
        AlignCenter,
        AlignCenter,
        app->usb_state == UsbStateWaiting  ? "Waiting for connection..." :
        app->usb_state == UsbStateInactive ? "USB Inactive!" :
        app->usb_state == UsbStateLost     ? "Connection lost!" :
                                             "No data!");
}
