#include "../can_commander.h"

#include <string.h>

typedef enum {
    ToolsControlWriteFrames = 0,
    ToolsControlSmartInjection,
    ToolsControlStopActive,
} ToolsControlMenuIndex;

static void cancommander_scene_tools_control_menu_callback(void* context, uint32_t index) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void cancommander_scene_tools_control_start_pending(App* app) {
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

static void cancommander_scene_tools_control_open_tool_args(
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
        cancommander_scene_tools_control_start_pending,
        cancommander_scene_monitor);

    scene_manager_next_scene(app->scene_manager, cancommander_scene_args_editor);
}

void cancommander_scene_tools_control_menu_on_enter(void* context) {
    App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Control & Injection");

    submenu_add_item(
        app->submenu,
        "Write Frames",
        ToolsControlWriteFrames,
        cancommander_scene_tools_control_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Smart Injection",
        ToolsControlSmartInjection,
        cancommander_scene_tools_control_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Stop Active Tool",
        ToolsControlStopActive,
        cancommander_scene_tools_control_menu_callback,
        app);

    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(app->scene_manager, cancommander_scene_tools_control_menu));

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
}

bool cancommander_scene_tools_control_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    scene_manager_set_scene_state(
        app->scene_manager, cancommander_scene_tools_control_menu, event.event);

    switch(event.event) {
    case ToolsControlWriteFrames:
        cancommander_scene_tools_control_open_tool_args(
            app,
            CcToolWrite,
            "write",
            app->args_write_tool,
            sizeof(app->args_write_tool),
            "Write Frames");
        return true;

    case ToolsControlSmartInjection:
        scene_manager_next_scene(app->scene_manager, cancommander_scene_custom_inject_menu);
        return true;

    case ToolsControlStopActive:
        app_action_tool_stop(app);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_status);
        return true;

    default:
        return false;
    }
}

void cancommander_scene_tools_control_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

