#include "../can_commander.h"

#include <string.h>

typedef enum {
    ToolsMonitorReadAll = 0,
    ToolsMonitorFiltered,
    ToolsMonitorUniqueIds,
    ToolsMonitorBittrack,
    ToolsMonitorReverseAuto,
    ToolsMonitorValtrack,
    ToolsMonitorSpeed,
} ToolsMonitorMenuIndex;

static void cancommander_scene_tools_monitor_menu_callback(void* context, uint32_t index) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void cancommander_scene_tools_monitor_start_pending(App* app) {
    app->args_editor_apply_next_scene = cancommander_scene_monitor;
    app_action_tool_start(
        app,
        app->pending_tool_start_id,
        app->args_editor_target,
        app->pending_tool_start_name[0] ? app->pending_tool_start_name : "tool");
    if(!app->connected) {
        app->args_editor_apply_next_scene = cancommander_scene_status;
    }
}

static void cancommander_scene_tools_monitor_open_tool_args(
    App* app,
    CcToolId tool_id,
    const char* tool_name,
    char* args,
    size_t args_size,
    const char* title) {
    app->pending_tool_start_id = tool_id;
    strncpy(app->pending_tool_start_name, tool_name, sizeof(app->pending_tool_start_name) - 1U);
    app->pending_tool_start_name[sizeof(app->pending_tool_start_name) - 1U] = '\0';

    app_begin_args_editor_apply(
        app,
        args,
        args_size,
        title,
        "Start",
        cancommander_scene_tools_monitor_start_pending,
        cancommander_scene_monitor);

    scene_manager_next_scene(app->scene_manager, cancommander_scene_args_editor);
}

void cancommander_scene_tools_monitor_menu_on_enter(void* context) {
    App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Monitor & Discovery");

    submenu_add_item(
        app->submenu,
        "Read All Frames",
        ToolsMonitorReadAll,
        cancommander_scene_tools_monitor_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Filter & Read Frames",
        ToolsMonitorFiltered,
        cancommander_scene_tools_monitor_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Unique IDs",
        ToolsMonitorUniqueIds,
        cancommander_scene_tools_monitor_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Bit Tracker",
        ToolsMonitorBittrack,
        cancommander_scene_tools_monitor_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Auto Reverse Engineer",
        ToolsMonitorReverseAuto,
        cancommander_scene_tools_monitor_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Value Tracker",
        ToolsMonitorValtrack,
        cancommander_scene_tools_monitor_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "CAN Speed Test",
        ToolsMonitorSpeed,
        cancommander_scene_tools_monitor_menu_callback,
        app);

    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(app->scene_manager, cancommander_scene_tools_monitor_menu));

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
}

bool cancommander_scene_tools_monitor_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    scene_manager_set_scene_state(
        app->scene_manager, cancommander_scene_tools_monitor_menu, event.event);

    switch(event.event) {
    case ToolsMonitorReadAll:
        cancommander_scene_tools_monitor_open_tool_args(
            app,
            CcToolReadAll,
            "read_all",
            app->args_read_all,
            sizeof(app->args_read_all),
            "Read All");
        return true;

    case ToolsMonitorFiltered:
        cancommander_scene_tools_monitor_open_tool_args(
            app,
            CcToolFiltered,
            "filtered",
            app->args_filtered,
            sizeof(app->args_filtered),
            "Read Filtered Frames");
        return true;

    case ToolsMonitorUniqueIds:
        cancommander_scene_tools_monitor_open_tool_args(
            app,
            CcToolUniqueIds,
            "unique_ids",
            app->args_unique_ids,
            sizeof(app->args_unique_ids),
            "Unique IDs Tool");
        return true;

    case ToolsMonitorBittrack:
        cancommander_scene_tools_monitor_open_tool_args(
            app,
            CcToolBittrack,
            "bittrack",
            app->args_bittrack,
            sizeof(app->args_bittrack),
            "Bit Tracker");
        return true;

    case ToolsMonitorReverseAuto:
        cancommander_scene_tools_monitor_open_tool_args(
            app,
            CcToolReverse,
            "reverse(auto)",
            app->args_reverse_auto,
            sizeof(app->args_reverse_auto),
            "Auto Reverse Engineer");
        return true;

    case ToolsMonitorValtrack:
        cancommander_scene_tools_monitor_open_tool_args(
            app,
            CcToolValtrack,
            "valtrack",
            app->args_valtrack,
            sizeof(app->args_valtrack),
            "Value Tracker");
        return true;

    case ToolsMonitorSpeed:
        cancommander_scene_tools_monitor_open_tool_args(
            app,
            CcToolSpeed,
            "speed",
            app->args_speed,
            sizeof(app->args_speed),
            "CAN Speed Test");
        return true;

    default:
        return false;
    }
}

void cancommander_scene_tools_monitor_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

