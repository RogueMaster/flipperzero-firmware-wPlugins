#include "portal_of_flipper_i.h"

#include <furi.h>

#define TAG "PoF"

void pof_start(PoFApp* app) {
    furi_assert(app);

    if (app->virtual_portal->type == PoFHid) {
        app->pof_usb = pof_usb_start(app->virtual_portal);
    }
    if (app->virtual_portal->type == PoFXbox360) {
        app->pof_usb = pof_usb_start_xbox360(app->virtual_portal);
    }
}

void pof_stop(PoFApp* app) {
    furi_assert(app);

    if (app->virtual_portal->type == PoFHid) {
        pof_usb_stop(app->pof_usb);
    }
    if (app->virtual_portal->type == PoFXbox360) {
        pof_usb_stop_xbox360(app->pof_usb);
    }
}
