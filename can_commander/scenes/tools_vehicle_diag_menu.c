#include "../can_commander.h"

typedef enum {
    ToolsVehicleDiagLiveData = 0,
    ToolsVehicleDiagFetchVin,
    ToolsVehicleDiagFetchDtcs,
    ToolsVehicleDiagClearDtcs,
} ToolsVehicleDiagMenuIndex;

static void cancommander_scene_tools_vehicle_diag_menu_callback(void* context, uint32_t index) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void cancommander_scene_tools_vehicle_diag_set_pid(App* app, const char* pid_token, const char* label) {
    app_args_set_key_value(app->args_obd_pid, sizeof(app->args_obd_pid), "pid", pid_token);
    app_set_status(app, "OBD PID selected: %s", label);
}

void cancommander_scene_tools_vehicle_diag_menu_on_enter(void* context) {
    App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Vehicle Diagnostics");

    submenu_add_item(
        app->submenu,
        "OBD2 Live Data",
        ToolsVehicleDiagLiveData,
        cancommander_scene_tools_vehicle_diag_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Fetch VIN",
        ToolsVehicleDiagFetchVin,
        cancommander_scene_tools_vehicle_diag_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Fetch DTCs",
        ToolsVehicleDiagFetchDtcs,
        cancommander_scene_tools_vehicle_diag_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Clear DTCs",
        ToolsVehicleDiagClearDtcs,
        cancommander_scene_tools_vehicle_diag_menu_callback,
        app);

    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(app->scene_manager, cancommander_scene_tools_vehicle_diag_menu));

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
}

bool cancommander_scene_tools_vehicle_diag_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    scene_manager_set_scene_state(
        app->scene_manager, cancommander_scene_tools_vehicle_diag_menu, event.event);

    switch(event.event) {
    case ToolsVehicleDiagLiveData:
        scene_manager_next_scene(app->scene_manager, cancommander_scene_obd_pid_menu);
        return true;

    case ToolsVehicleDiagFetchVin:
        cancommander_scene_tools_vehicle_diag_set_pid(app, "vin", "VIN");
        app_action_tool_start(app, CcToolObdPid, app->args_obd_pid, "obd_pid");
        scene_manager_next_scene(
            app->scene_manager,
            app->connected ? cancommander_scene_monitor : cancommander_scene_status);
        return true;

    case ToolsVehicleDiagFetchDtcs:
        cancommander_scene_tools_vehicle_diag_set_pid(app, "dtc_all", "DTCs (All)");
        app_action_tool_start(app, CcToolObdPid, app->args_obd_pid, "obd_pid");
        scene_manager_next_scene(
            app->scene_manager,
            app->connected ? cancommander_scene_monitor : cancommander_scene_status);
        return true;

    case ToolsVehicleDiagClearDtcs:
        cancommander_scene_tools_vehicle_diag_set_pid(app, "clear_dtcs", "Clear DTCs");
        app_action_tool_start(app, CcToolObdPid, app->args_obd_pid, "obd_pid");
        scene_manager_next_scene(
            app->scene_manager,
            app->connected ? cancommander_scene_monitor : cancommander_scene_status);
        return true;

    default:
        return false;
    }
}

void cancommander_scene_tools_vehicle_diag_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

