#include "status_view.h"

void draw_status_view(Canvas* canvas, void* ctx) {
    PcMonitorApp* app = ctx;

    canvas_draw_str_aligned(
        canvas,
        64,
        32,
        AlignCenter,
        AlignCenter,
        app->bt_state == BtStateChecking ? "Checking BLE..." :
        app->bt_state == BtStateInactive ? "BLE inactive!" :
        app->bt_state == BtStateLost     ? "Connection lost!" :
                                           "No data!");
}
