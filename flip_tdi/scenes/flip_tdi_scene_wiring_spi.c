#include "../flip_tdi_app_i.h"

void flip_tdi_scene_wiring_spi_on_enter(void* context) {
    furi_assert(context);

    FlipTDIApp* app = context;
    widget_add_icon_element(app->widget, 0, 0, &I_flip_tdi_wiring_spi);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipTDIViewWidget);
}

bool flip_tdi_scene_wiring_spi_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}
void flip_tdi_scene_wiring_spi_on_exit(void* context) {
    furi_assert(context);

    FlipTDIApp* app = context;
    widget_reset(app->widget);
}
