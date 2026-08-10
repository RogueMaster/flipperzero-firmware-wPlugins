/* Runs the app screens on an ordinary computer: the very same tpms_view.c
 * as in the firmware, only the screen is replaced by a canvas emulator.
 *
 * It catches what would otherwise show up on the device alone: text past
 * the edge of the screen and columns overlapping each other.
 *
 * Build and run: ./run.sh */

#include "canvas_stub.h"
#include "tpms_view.h"

uint32_t tpms_test_tick = 0;

static int g_problems;

static void add_frame(
    TpmsBridgeApp* app,
    uint32_t id,
    uint16_t pressure_raw,
    int16_t temperature_c,
    int16_t rssi_x10,
    uint32_t tick) {
    TpmsRenaultFrame frame = {
        .id = id,
        .pressure_raw = pressure_raw,
        .temperature_c = temperature_c,
        .flags = 0x33,
        .unknown = 0xFFFF,
    };
    tpms_store_update(&app->store, &frame, rssi_x10, tick);
}

static void show(TpmsBridgeApp* app, const char* title) {
    tpms_view_follow_selection(app);
    tpms_view_draw(NULL, app);
    tpms_canvas_print(title);
    g_problems += tpms_canvas_check(title);
}

int main(void) {
    static TpmsBridgeApp app;
    memset(&app, 0, sizeof(app));

    tpms_test_tick = 1000;

    /* Empty list: the app has only just been opened. */
    app.local_rx = true;
    show(&app, "List: no sensors yet");

    /* Four wheels: typical values, different signal levels. */
    add_frame(&app, 0x02c99d, 293, 26, -605, 1000); /* 2.20 bar, right next to it */
    add_frame(&app, 0x7ad779, 300, 24, -725, 1100);
    add_frame(&app, 0x1b04f2, 287, 31, -845, 1200);
    add_frame(&app, 0x0a11c3, 306, 19, -960, 1300);
    tpms_test_tick = 5000;
    show(&app, "List: four wheels");

    /* Extreme values: maximum pressure, sub-zero outside, a long frame
     * counter and a sensor that has been silent for a while. */
    add_frame(&app, 0xffffff, 1023, -40, -1000, 5000);
    add_frame(&app, 0x123456, 1023, -40, -1000, 5000);
    for(uint32_t i = 0; i < 99999; i++) add_frame(&app, 0x02c99d, 293, 26, -605, 5000);
    app.selected = 4;
    app.auto_wake = true;
    app.usb_streaming = true;
    tpms_test_tick = 5000 + 120000; /* sensors went silent two minutes ago */
    show(&app, "List: scrolling and extreme values");

    /* Detail screen: an ordinary sensor. */
    app.screen = TpmsScreenDetail;
    app.selected = 0;
    app.usb_streaming = false;
    tpms_test_tick = 5000 + 3000;
    show(&app, "Detail: ordinary values");

    /* Detail screen: extreme values. */
    app.selected = 4;
    tpms_test_tick = 5000 + 600000;
    show(&app, "Detail: extreme values");

    if(g_problems == 0) {
        printf("\nlayout is fine\n");
        return 0;
    }

    printf("\nlayout problems: %d\n", g_problems);
    return 1;
}
