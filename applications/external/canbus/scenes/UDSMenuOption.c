#include "../app_user.h"

typedef enum {
    MANUAL_UDS_OPTION,
    SET_DIAGNOSTIC_SESSION_OPTION,
    ECU_RESET_OPTION,
    GET_VIN_OPTION,
    STORED_DTC,
    UDS_SETTINGS
} uds_elements_list;

// Variable to set the item selected
uint32_t selector_option = 0;

/**
 * Scene to show the menu in the UDS Menu option
 */

// Callback function
void uds_menu_callback(void* context, uint32_t index) {
    App* app = context;

    // Selector option set
    selector_option = index;

    switch(index) {
    case MANUAL_UDS_OPTION: // Manual Sender UDS service
        scene_manager_next_scene(
            app->scene_manager, app_scene_uds_single_frame_request_sender_option);
        break;

    case SET_DIAGNOSTIC_SESSION_OPTION: // To set the diagnostic Session
        scene_manager_next_scene(app->scene_manager, app_scene_uds_set_diagnostic_session_option);
        break;

    case GET_VIN_OPTION: // Request VIN from car
        scene_manager_next_scene(app->scene_manager, app_scene_uds_request_vin_option);
        break;

    case ECU_RESET_OPTION: // Reset the ECU
        scene_manager_next_scene(app->scene_manager, app_scene_uds_ecu_reset_option);
        break;

    case STORED_DTC:
        scene_manager_next_scene(app->scene_manager, app_scene_uds_get_dtc_menu_option);
        break;

    case UDS_SETTINGS:
        scene_manager_next_scene(app->scene_manager, app_scene_uds_settings_option);
        break;

    default:
        break;
    }
}

// Scene on enter to submenu
void app_scene_uds_menu_on_enter(void* context) {
    App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "UDS Services");
    submenu_add_item(app->submenu, "Manual Request", MANUAL_UDS_OPTION, uds_menu_callback, app);
    submenu_add_item(
        app->submenu, "Set Session", SET_DIAGNOSTIC_SESSION_OPTION, uds_menu_callback, app);
    submenu_add_item(app->submenu, "ECU Reset", ECU_RESET_OPTION, uds_menu_callback, app);
    submenu_add_item(app->submenu, "Get VIN Number", GET_VIN_OPTION, uds_menu_callback, app);
    submenu_add_item(app->submenu, "Get DTC", STORED_DTC, uds_menu_callback, app);
    submenu_add_item(app->submenu, "Settings", UDS_SETTINGS, uds_menu_callback, app);

    submenu_set_selected_item(app->submenu, selector_option);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

// Scene on event
bool app_scene_uds_menu_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    bool consumed = false;

    return consumed;
}

// Scene on Exit
void app_scene_uds_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
