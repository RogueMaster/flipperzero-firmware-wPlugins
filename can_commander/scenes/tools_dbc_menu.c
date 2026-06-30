#include "../can_commander.h"

typedef enum {
    ToolsDbcDecode = 0,
    ToolsDbcLoadConfig,
    ToolsDbcDatabaseManager,
} ToolsDbcMenuIndex;

static void cancommander_scene_tools_dbc_menu_callback(void* context, uint32_t index) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void cancommander_scene_tools_dbc_menu_on_enter(void* context) {
    App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "DBC & Databases");

    submenu_add_item(
        app->submenu,
        "DBC Decode",
        ToolsDbcDecode,
        cancommander_scene_tools_dbc_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Load DBC Profile",
        ToolsDbcLoadConfig,
        cancommander_scene_tools_dbc_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "DBC Database Manager",
        ToolsDbcDatabaseManager,
        cancommander_scene_tools_dbc_menu_callback,
        app);

    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(app->scene_manager, cancommander_scene_tools_dbc_menu));

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
}

bool cancommander_scene_tools_dbc_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    scene_manager_set_scene_state(
        app->scene_manager, cancommander_scene_tools_dbc_menu, event.event);

    switch(event.event) {
    case ToolsDbcDecode:
        app_action_tool_start(app, CcToolDbcDecode, app->args_dbc_decode, "dbc_decode");
        scene_manager_next_scene(
            app->scene_manager,
            app->connected ? cancommander_scene_monitor : cancommander_scene_status);
        return true;

    case ToolsDbcLoadConfig:
        scene_manager_next_scene(app->scene_manager, cancommander_scene_dbc_load_config_menu);
        return true;

    case ToolsDbcDatabaseManager:
        scene_manager_next_scene(app->scene_manager, cancommander_scene_dbc_menu);
        return true;

    default:
        return false;
    }
}

void cancommander_scene_tools_dbc_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
