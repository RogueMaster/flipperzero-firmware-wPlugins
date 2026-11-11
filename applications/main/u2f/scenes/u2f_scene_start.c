#include "../u2f_app_i.h"

typedef enum {
    U2fSceneStartUsb,
    U2fSceneStartNfc,
} U2fSceneStartIndex;

static void u2f_scene_start_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    U2fApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void u2f_scene_start_on_enter(void* context) {
    U2fApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "U2F Security Key");
    submenu_add_item(submenu, "USB", U2fSceneStartUsb, u2f_scene_start_submenu_callback, app);
    submenu_add_item(submenu, "NFC", U2fSceneStartNfc, u2f_scene_start_submenu_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, U2fSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, U2fAppViewStart);
}

bool u2f_scene_start_on_event(void* context, SceneManagerEvent event) {
    U2fApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, U2fSceneStart, event.event);
        if(event.event == U2fSceneStartUsb) {
            scene_manager_next_scene(app->scene_manager, U2fSceneMain);
            consumed = true;
        } else if(event.event == U2fSceneStartNfc) {
            scene_manager_next_scene(app->scene_manager, U2fSceneNfc);
            consumed = true;
        }
    }

    return consumed;
}

void u2f_scene_start_on_exit(void* context) {
    U2fApp* app = context;
    submenu_reset(app->submenu);
}
