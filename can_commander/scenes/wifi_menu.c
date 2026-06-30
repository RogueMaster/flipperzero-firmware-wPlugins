#include "../can_commander.h"

typedef enum {
    WifiMenuConfig = 0,
    WifiMenuEnableAp,
    WifiMenuDisableAp,
} WifiMenuIndex;

static void cancommander_scene_wifi_menu_callback(void* context, uint32_t index) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void cancommander_scene_wifi_menu_apply_config(App* app) {
    app_action_wifi_set_cfg(app, app->args_wifi_cfg);
}

void cancommander_scene_wifi_menu_on_enter(void* context) {
    App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "WiFi Settings");

    submenu_add_item(
        app->submenu, "WiFi AP Config", WifiMenuConfig, cancommander_scene_wifi_menu_callback, app);
    submenu_add_item(
        app->submenu, "Enable AP", WifiMenuEnableAp, cancommander_scene_wifi_menu_callback, app);
    submenu_add_item(
        app->submenu, "Disable AP", WifiMenuDisableAp, cancommander_scene_wifi_menu_callback, app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, cancommander_scene_wifi_menu));

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
}

bool cancommander_scene_wifi_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    scene_manager_set_scene_state(app->scene_manager, cancommander_scene_wifi_menu, event.event);

    switch(event.event) {
    case WifiMenuConfig:
        app_begin_args_editor_apply(
            app,
            app->args_wifi_cfg,
            sizeof(app->args_wifi_cfg),
            "WiFi AP Settings",
            "Apply",
            cancommander_scene_wifi_menu_apply_config,
            cancommander_scene_status);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_args_editor);
        return true;

    case WifiMenuEnableAp:
        app_args_set_key_value(app->args_wifi_cfg, sizeof(app->args_wifi_cfg), "ap", "1");
        app_action_wifi_set_cfg(app, "ap=1");
        scene_manager_next_scene(app->scene_manager, cancommander_scene_status);
        return true;

    case WifiMenuDisableAp:
        app_args_set_key_value(app->args_wifi_cfg, sizeof(app->args_wifi_cfg), "ap", "0");
        app_action_wifi_set_cfg(app, "ap=0");
        scene_manager_next_scene(app->scene_manager, cancommander_scene_status);
        return true;

    default:
        return false;
    }
}

void cancommander_scene_wifi_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

