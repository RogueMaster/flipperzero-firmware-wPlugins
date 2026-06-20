#include "../led_remote.h"

typedef enum {
    ProfileOptionUse,
    ProfileOptionRename,
    ProfileOptionDelete,
} ProfileOption;

static void led_remote_scene_profile_options_callback(void* context, uint32_t index) {
    LedRemoteApp* app = context;
    scene_manager_handle_custom_event(app->scene_manager, index);
}

void led_remote_scene_profile_options_on_enter(void* context) {
    LedRemoteApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, app->profile_selected);

    submenu_add_item(
        app->submenu, "Use Profile", ProfileOptionUse,
        led_remote_scene_profile_options_callback, app);
    submenu_add_item(
        app->submenu, "Rename", ProfileOptionRename,
        led_remote_scene_profile_options_callback, app);
    submenu_add_item(
        app->submenu, "Delete", ProfileOptionDelete,
        led_remote_scene_profile_options_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, LedRemoteViewIdSubmenu);
}

bool led_remote_scene_profile_options_on_event(void* context, SceneManagerEvent event) {
    LedRemoteApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case ProfileOptionUse:
        led_remote_switch_profile(app, app->profile_selected);
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, LedRemoteSceneMenu);
        return true;

    case ProfileOptionRename:
        app->profile_edit_is_new = false;
        strncpy(app->text_input_buf, app->profile_selected, sizeof(app->text_input_buf) - 1);
        app->text_input_buf[sizeof(app->text_input_buf) - 1] = '\0';
        scene_manager_next_scene(app->scene_manager, LedRemoteSceneProfileEdit);
        return true;

    case ProfileOptionDelete:
        led_remote_delete_profile(app, app->profile_selected);
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, LedRemoteSceneProfiles);
        return true;
    }
    return false;
}

void led_remote_scene_profile_options_on_exit(void* context) {
    LedRemoteApp* app = context;
    submenu_reset(app->submenu);
}
