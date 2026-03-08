#include "../can_commander.h"

typedef enum {
    ToolsMonitorDiscovery = 0,
    ToolsControlInjection,
    ToolsVehicleDiagnostics,
    ToolsDbcDatabases,
} ToolsMenuIndex;

static void cancommander_scene_tools_menu_callback(void* context, uint32_t index) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void cancommander_scene_tools_menu_on_enter(void* context) {
    App* app = context;

    submenu_reset(app->submenu);

    submenu_add_item(
        app->submenu,
        "Monitor & Discovery",
        ToolsMonitorDiscovery,
        cancommander_scene_tools_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Control & Injection",
        ToolsControlInjection,
        cancommander_scene_tools_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Vehicle Diagnostics",
        ToolsVehicleDiagnostics,
        cancommander_scene_tools_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "DBC & Databases",
        ToolsDbcDatabases,
        cancommander_scene_tools_menu_callback,
        app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, cancommander_scene_tools_menu));

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
}

bool cancommander_scene_tools_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    scene_manager_set_scene_state(app->scene_manager, cancommander_scene_tools_menu, event.event);

    switch(event.event) {
    case ToolsMonitorDiscovery:
        scene_manager_next_scene(app->scene_manager, cancommander_scene_tools_monitor_menu);
        return true;

    case ToolsControlInjection:
        scene_manager_next_scene(app->scene_manager, cancommander_scene_tools_control_menu);
        return true;

    case ToolsVehicleDiagnostics:
        scene_manager_next_scene(app->scene_manager, cancommander_scene_tools_vehicle_diag_menu);
        return true;

    case ToolsDbcDatabases:
        scene_manager_next_scene(app->scene_manager, cancommander_scene_tools_dbc_menu);
        return true;

    default:
        return false;
    }
}

void cancommander_scene_tools_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

