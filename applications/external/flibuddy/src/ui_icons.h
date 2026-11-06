/* The XBM icons of the screens (flipper/images, generated from the design
 * by tools/fixtures/make_flipper_icons.py) for canvas_draw_xbm. */
#pragma once

#include <stdint.h>

typedef struct {
    uint8_t width;
    uint8_t height;
    const uint8_t* bits;
} UiBitmap;

extern const UiBitmap ui_icon_usb;
extern const UiBitmap ui_icon_bt;
extern const UiBitmap ui_icon_check;
extern const UiBitmap ui_icon_warn;
extern const UiBitmap ui_icon_arrow_left;
extern const UiBitmap ui_icon_arrow_right;
extern const UiBitmap ui_icon_arrow_up;
extern const UiBitmap ui_icon_arrow_down;
extern const UiBitmap ui_icon_app;
extern const UiBitmap ui_icon_app_2x;
extern const UiBitmap ui_icon_ok_button; /* the firmware's ButtonCenter_7x7 */
extern const UiBitmap ui_icon_qr_get_app; /* Get app's QR code, 2 px a module */
