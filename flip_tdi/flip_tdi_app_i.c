#include "flip_tdi_app_i.h"

#include <furi.h>

#define TAG "FlipTDI"

void flip_tdi_start(FlipTDIApp* app) {
    furi_assert(app);

    app->ftdi_usb = ftdi_usb_start();
}

void flip_tdi_stop(FlipTDIApp* app) {
    furi_assert(app);

    ftdi_usb_stop(app->ftdi_usb);
}
