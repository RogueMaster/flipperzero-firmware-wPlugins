#include "../app_user.h"

// odbii menu casllback
void obdii_menu_callback(void* context, uint32_t index) {
    App* app = context;

    app->obdii_aux_index = index;

    switch(index) {
    case 0:
        scene_manager_next_scene(app->scene_manager, app_scene_supported_pid_option);
        break;

    case 1:
        scene_manager_next_scene(app->scene_manager, app_scene_obdii_typical_codes_option);
        break;

    case 2:
        app->request_data = 1;
        scene_manager_next_scene(app->scene_manager, app_scene_car_data_option);
        break;

    case 3:
        app->request_data = 2;
        scene_manager_next_scene(app->scene_manager, app_scene_car_data_option);
        break;

    case 4:
        scene_manager_next_scene(app->scene_manager, app_scene_obdii_get_errors_option);
        break;

    case 5:
        scene_manager_next_scene(app->scene_manager, app_scene_obdii_delete_errors_option);
        break;

    case 6:
        scene_manager_next_scene(app->scene_manager, app_scene_manual_sender_pid_option);
        break;

    default:
        break;
    }
}

void app_scene_obdii_menu_on_enter(void* context) {
    App* app = context;

    submenu_reset(app->submenu);

    submenu_set_header(app->submenu, "OBDII SCANNER");

    // Examples
    submenu_add_item(app->submenu, "Get Supported PID Codes", 0, obdii_menu_callback, app);
    submenu_add_item(app->submenu, "Show Typical Data", 1, obdii_menu_callback, app);
    submenu_add_item(app->submenu, "Get VIN number", 2, obdii_menu_callback, app);
    submenu_add_item(app->submenu, "Get ECU name", 3, obdii_menu_callback, app);
    submenu_add_item(app->submenu, "Show DTC", 4, obdii_menu_callback, app);
    submenu_add_item(app->submenu, "Delete DTC", 5, obdii_menu_callback, app);
    submenu_add_item(app->submenu, "Manual Sender PID", 6, obdii_menu_callback, app);

    submenu_set_selected_item(app->submenu, app->obdii_aux_index);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

bool app_scene_obdii_menu_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void app_scene_obdii_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
