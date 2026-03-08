#include "../can_commander.h"

typedef enum {
    DebugConnect = 0,
    DebugBusConfig,
    DebugBusFilters,
    DebugStats,
    DebugPing,
    DebugGetInfo,
} DebugMenuIndex;

static void cancommander_scene_debug_menu_callback(void* context, uint32_t index) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void cancommander_scene_debug_menu_on_enter(void* context) {
    App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Settings");

    submenu_add_item(
        app->submenu,
        "Connect/Reconnect",
        DebugConnect,
        cancommander_scene_debug_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Bus Config",
        DebugBusConfig,
        cancommander_scene_debug_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Bus Filters",
        DebugBusFilters,
        cancommander_scene_debug_menu_callback,
        app);
    submenu_add_item(app->submenu, "Stats", DebugStats, cancommander_scene_debug_menu_callback, app);
    submenu_add_item(app->submenu, "Ping", DebugPing, cancommander_scene_debug_menu_callback, app);
    submenu_add_item(app->submenu, "Get Info", DebugGetInfo, cancommander_scene_debug_menu_callback, app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, cancommander_scene_debug_menu));

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
}

bool cancommander_scene_debug_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    scene_manager_set_scene_state(app->scene_manager, cancommander_scene_debug_menu, event.event);

    switch(event.event) {
    case DebugConnect:
        if(app_connect(app, true)) {
            app_set_status(app, "Connected at %lu baud", (unsigned long)CC_UART_BAUD);
        } else {
            app_set_status(app, "Connection failed");
        }
        scene_manager_next_scene(app->scene_manager, cancommander_scene_status);
        return true;

    case DebugBusConfig:
        scene_manager_next_scene(app->scene_manager, cancommander_scene_bus_cfg_menu);
        return true;

    case DebugBusFilters:
        scene_manager_next_scene(app->scene_manager, cancommander_scene_bus_filter_menu);
        return true;

    case DebugStats:
        app_action_stats(app);
        scene_manager_next_scene(
            app->scene_manager,
            app->connected ? cancommander_scene_monitor : cancommander_scene_status);
        return true;

    case DebugPing:
        app_action_ping(app);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_status);
        return true;

    case DebugGetInfo:
        app_action_get_info(app);
        scene_manager_next_scene(
            app->scene_manager,
            app->connected ? cancommander_scene_monitor : cancommander_scene_status);
        return true;

    default:
        return false;
    }
}

void cancommander_scene_debug_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
