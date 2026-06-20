#include "../led_remote.h"

static void led_remote_scene_remote_type_callback(void* context, uint32_t index) {
    LedRemoteApp* app = context;
    scene_manager_handle_custom_event(app->scene_manager, index);
}

void led_remote_scene_remote_type_on_enter(void* context) {
    LedRemoteApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Remote Type");

    for(uint32_t i = 0; i < LedRemoteTypeCount; i++) {
        submenu_add_item(
            app->submenu,
            LED_REMOTE_TYPE_DEFS[i].type_name,
            i,
            led_remote_scene_remote_type_callback,
            app);
    }

    submenu_set_selected_item(app->submenu, (uint32_t)app->remote_type);
    view_dispatcher_switch_to_view(app->view_dispatcher, LedRemoteViewIdSubmenu);
}

bool led_remote_scene_remote_type_on_event(void* context, SceneManagerEvent event) {
    LedRemoteApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    LedRemoteType new_type = (LedRemoteType)event.event;
    if(new_type < LedRemoteTypeCount) {
        led_remote_switch_type(app, new_type);
    }
    scene_manager_previous_scene(app->scene_manager);
    return true;
}

void led_remote_scene_remote_type_on_exit(void* context) {
    LedRemoteApp* app = context;
    submenu_reset(app->submenu);
}
