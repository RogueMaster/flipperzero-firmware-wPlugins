#include "usb_hid_autofire_i.h"
#include "version.h"
#include <usb_hid_autofire_icons.h>

void usb_hid_autofire_render_callback(Canvas* canvas, void* ctx) {
    UsbHidAutofireApp* app = ctx;
    char status_str[24];
    char mode_str[24];
    char preset_str[24];
    char delay_rate_str[40];
    char cps_str[24];

    canvas_clear(canvas);

    if(app->show_help) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 10, "Autofire Help");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_icon(canvas, 0, 14, &I_Ok_btn_9x9);
        canvas_draw_str(canvas, 12, 22, "start/pause");

        canvas_draw_icon(canvas, 0, 24, &I_Ok_btn_9x9);
        canvas_draw_str(canvas, 12, 32, "long: cycle preset");

        canvas_draw_icon(canvas, 0, 36, &I_ButtonUp_7x4);
        canvas_draw_icon(canvas, 9, 36, &I_ButtonDown_7x4);
        canvas_draw_str(canvas, 20, 42, "tap+hold: mode");

        canvas_draw_icon(canvas, 0, 45, &I_ButtonLeft_4x7);
        canvas_draw_icon(canvas, 7, 45, &I_ButtonRight_4x7);
        canvas_draw_str(canvas, 20, 52, "tap+hold: delay");

        canvas_draw_icon(canvas, 0, 55, &I_Pin_back_arrow_10x8);
        canvas_draw_str(canvas, 13, 63, "close");
        return;
    }

    snprintf(status_str, sizeof(status_str), "Status: %s", app->active ? "ACTIVE" : "PAUSED");
    snprintf(mode_str, sizeof(mode_str), "Mode: %s", usb_hid_autofire_mode_label(app->mode));
    snprintf(
        preset_str, sizeof(preset_str), "Preset: %s", usb_hid_autofire_preset_label(app->preset));
    usb_hid_autofire_format_cps(cps_str, sizeof(cps_str), app->realtime_cps_x10);
    snprintf(
        delay_rate_str,
        sizeof(delay_rate_str),
        "Delay:%lums  Rate:%s",
        (unsigned long)app->autofire_delay_ms,
        cps_str);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "USB HID Autofire");
    canvas_draw_str(canvas, 90, 10, "v");
    canvas_draw_str(canvas, 96, 10, VERSION);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 22, status_str);
    canvas_draw_str(canvas, 0, 32, mode_str);
    canvas_draw_str(canvas, 0, 42, preset_str);
    canvas_draw_str(canvas, 0, 52, delay_rate_str);
    canvas_draw_icon(canvas, 0, 55, &I_Pin_back_arrow_10x8);
    canvas_draw_str(canvas, 13, 63, "hold:help");
    canvas_draw_icon(canvas, 72, 56, &I_ButtonUp_7x4);
    canvas_draw_icon(canvas, 81, 56, &I_ButtonDown_7x4);
    canvas_draw_str(canvas, 92, 63, "mode");
}
