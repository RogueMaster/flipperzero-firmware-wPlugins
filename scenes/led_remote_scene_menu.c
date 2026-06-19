#include "../led_remote.h"

typedef enum {
    LedRemoteMenuItemRemote,
    LedRemoteMenuItemLearn,
    LedRemoteMenuItemProfiles,
    LedRemoteMenuItemRemoteType,
    LedRemoteMenuItemStyle,
} LedRemoteMenuItem;

static void led_remote_scene_menu_callback(void* context, uint32_t index) {
    LedRemoteApp* app = context;
    scene_manager_handle_custom_event(app->scene_manager, index);
}

void led_remote_scene_menu_on_enter(void* context) {
    LedRemoteApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, app->profile_name);
    submenu_add_item(
        app->submenu, "Use Remote", LedRemoteMenuItemRemote,
        led_remote_scene_menu_callback, app);
    submenu_add_item(
        app->submenu, "Learn Buttons", LedRemoteMenuItemLearn,
        led_remote_scene_menu_callback, app);
    submenu_add_item(
        app->submenu, "Switch Profile", LedRemoteMenuItemProfiles,
        led_remote_scene_menu_callback, app);
    submenu_add_item(
        app->submenu, "Remote Type", LedRemoteMenuItemRemoteType,
        led_remote_scene_menu_callback, app);
    submenu_add_item(
        app->submenu, "Remote Style", LedRemoteMenuItemStyle,
        led_remote_scene_menu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, LedRemoteViewIdSubmenu);
}

bool led_remote_scene_menu_on_event(void* context, SceneManagerEvent event) {
    LedRemoteApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case LedRemoteMenuItemRemote:
        scene_manager_next_scene(app->scene_manager, LedRemoteSceneRemote);
        return true;
    case LedRemoteMenuItemLearn:
        scene_manager_next_scene(app->scene_manager, LedRemoteSceneLearn);
        return true;
    case LedRemoteMenuItemProfiles:
        scene_manager_next_scene(app->scene_manager, LedRemoteSceneProfiles);
        return true;
    case LedRemoteMenuItemRemoteType:
        scene_manager_next_scene(app->scene_manager, LedRemoteSceneRemoteType);
        return true;
    case LedRemoteMenuItemStyle:
        scene_manager_next_scene(app->scene_manager, LedRemoteSceneStyle);
        return true;
    }
    return false;
}

void led_remote_scene_menu_on_exit(void* context) {
    LedRemoteApp* app = context;
    submenu_reset(app->submenu);
}
