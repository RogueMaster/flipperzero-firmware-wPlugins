#include "../chameleon_app_i.h"

void chameleon_scene_tag_write_on_enter(void* context) {
    ChameleonApp* app = context;

    popup_reset(app->popup);
    popup_set_header(app->popup, "Coming Soon", 64, 10, AlignCenter, AlignTop);
    popup_set_text(
        app->popup,
        "Tag writing to\nChameleon Ultra\nwill be implemented",
        64,
        32,
        AlignCenter,
        AlignCenter);

    view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewPopup);

    furi_delay_ms(2500);
    scene_manager_previous_scene(app->scene_manager);
}

bool chameleon_scene_tag_write_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void chameleon_scene_tag_write_on_exit(void* context) {
    ChameleonApp* app = context;
    popup_reset(app->popup);
}
