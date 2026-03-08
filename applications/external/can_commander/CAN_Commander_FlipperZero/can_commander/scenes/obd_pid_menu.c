#include "../can_commander.h"

typedef enum {
    ObdPidPidList = 0,
    ObdPidLiveConfig,
    ObdPidStartLive,
    ObdPidAdvancedArgs,
} ObdPidMenuIndex;

static void cancommander_scene_obd_pid_menu_callback(void* context, uint32_t index) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void cancommander_scene_obd_pid_start_apply(App* app) {
    app->args_editor_apply_next_scene = cancommander_scene_monitor;
    app_action_tool_start(app, CcToolObdPid, app->args_obd_pid, "obd_pid");
    if(!app->connected) {
        app->args_editor_apply_next_scene = cancommander_scene_status;
    }
}

void cancommander_scene_obd_pid_menu_on_enter(void* context) {
    App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "OBD2 Live Data");

    submenu_add_item(app->submenu, "PID List", ObdPidPidList, cancommander_scene_obd_pid_menu_callback, app);
    submenu_add_item(
        app->submenu,
        "Live PID Config",
        ObdPidLiveConfig,
        cancommander_scene_obd_pid_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Start Live PID",
        ObdPidStartLive,
        cancommander_scene_obd_pid_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Advanced Args",
        ObdPidAdvancedArgs,
        cancommander_scene_obd_pid_menu_callback,
        app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, cancommander_scene_obd_pid_menu));

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
}

bool cancommander_scene_obd_pid_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    scene_manager_set_scene_state(app->scene_manager, cancommander_scene_obd_pid_menu, event.event);

    switch(event.event) {
    case ObdPidPidList:
        scene_manager_next_scene(app->scene_manager, cancommander_scene_obd_pid_list_menu);
        return true;

    case ObdPidLiveConfig:
        app_begin_args_editor(
            app,
            app->args_obd_pid,
            sizeof(app->args_obd_pid),
            "Live PID Config");
        scene_manager_next_scene(app->scene_manager, cancommander_scene_args_editor);
        return true;

    case ObdPidStartLive:
        app_action_tool_start(app, CcToolObdPid, app->args_obd_pid, "obd_pid");
        scene_manager_next_scene(
            app->scene_manager,
            app->connected ? cancommander_scene_monitor : cancommander_scene_status);
        return true;

    case ObdPidAdvancedArgs:
        app_begin_args_editor_apply(
            app,
            app->args_obd_pid,
            sizeof(app->args_obd_pid),
            "OBD PID Args",
            "Start",
            cancommander_scene_obd_pid_start_apply,
            cancommander_scene_monitor);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_args_editor);
        return true;

    default:
        return false;
    }
}

void cancommander_scene_obd_pid_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
