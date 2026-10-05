#include "dndolphins_splash.h"
#include "dndolphins_icons.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>

#define DNDOLPHINS_SPLASH_DURATION_MS 2000U

static void dndolphins_splash_draw(Canvas* canvas, void* context) {
    UNUSED(context);
    canvas_clear(canvas);
    canvas_draw_icon(canvas, 0, 0, &I_logo_128x64);
}

void dndolphins_splash_show(void) {
    Gui* gui = furi_record_open(RECORD_GUI);
    if(!gui) return;

    ViewPort* view_port = view_port_alloc();
    if(!view_port) {
        furi_record_close(RECORD_GUI);
        return;
    }

    view_port_draw_callback_set(view_port, dndolphins_splash_draw, NULL);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    view_port_update(view_port);

    furi_delay_ms(DNDOLPHINS_SPLASH_DURATION_MS);

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
}
