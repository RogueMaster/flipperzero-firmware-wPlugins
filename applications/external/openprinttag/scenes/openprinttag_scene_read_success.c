#include "../openprinttag_i.h"

static void openprinttag_scene_read_success_popup_callback(void* context) {
    OpenPrintTag* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void openprinttag_scene_read_success_on_enter(void* context) {
    OpenPrintTag* app = context;
    Popup* popup = app->popup;

    popup_set_header(popup, "Success!", 64, 10, AlignCenter, AlignTop);
    popup_set_text(popup, "Tag read successfully", 64, 32, AlignCenter, AlignCenter);
    popup_set_timeout(popup, 1500);
    popup_set_context(popup, app);
    popup_set_callback(popup, openprinttag_scene_read_success_popup_callback);
    popup_enable_timeout(popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, OpenPrintTagViewPopup);
}

bool openprinttag_scene_read_success_on_event(void* context, SceneManagerEvent event) {
    OpenPrintTag* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_next_scene(app->scene_manager, OpenPrintTagSceneDisplay);
        consumed = true;
    }

    return consumed;
}

void openprinttag_scene_read_success_on_exit(void* context) {
    OpenPrintTag* app = context;
    popup_reset(app->popup);
}
