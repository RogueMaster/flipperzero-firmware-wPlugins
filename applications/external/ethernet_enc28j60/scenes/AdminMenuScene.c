#include "../app_user.h"

enum {
    GET_IP_OPTION,
    SCAN_HOSTS_OPTION,
    PASSIVE_DISCOVERY_OPTION,
    PING_OPTION,
    PORTS_SCANNER_OPTION,
    OS_DETECTOR_OPTION,
};

//  Callback for the Options on the main menu
void main_menu_options_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    scene_manager_set_scene_state(app->scene_manager, app_scene_main_menu_option, index);

    switch(index) {
    case GET_IP_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_get_ip_option);
        break;

    case SCAN_HOSTS_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_arp_scanner_menu_option);
        break;

    case PASSIVE_DISCOVERY_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_passive_discovery_option);
        break;

    case PING_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_ping_menu_option);
        break;

    case PORTS_SCANNER_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_ports_scanner_option);
        break;

    case OS_DETECTOR_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_os_detector_option);
        break;

    default:
        break;
    }
}

// Function for the main menu on enter
void app_scene_main_menu_on_enter(void* context) {
    App* app = (App*)context;

    // Reset Menu
    submenu_reset(app->submenu);

    // header for the  submenu
    submenu_set_header(app->submenu, "ETHERNET ADMIN");

    submenu_add_item(app->submenu, "Get IP", GET_IP_OPTION, main_menu_options_callback, app);

    submenu_add_item(
        app->submenu, "Scan Hosts", SCAN_HOSTS_OPTION, main_menu_options_callback, app);

    submenu_add_item(
        app->submenu,
        "Passive Discovery",
        PASSIVE_DISCOVERY_OPTION,
        main_menu_options_callback,
        app);

    submenu_add_item(app->submenu, "Ping Host", PING_OPTION, main_menu_options_callback, app);

    submenu_add_item(
        app->submenu, "Scan Ports", PORTS_SCANNER_OPTION, main_menu_options_callback, app);

    submenu_add_item(
        app->submenu, "Detect OS", OS_DETECTOR_OPTION, main_menu_options_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);

    uint32_t index = scene_manager_get_scene_state(app->scene_manager, app_scene_main_menu_option);
    submenu_set_selected_item(app->submenu, index);
}

// Function for the main menu on event
bool app_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = (App*)context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the main menu on exit
void app_scene_main_menu_on_exit(void* context) {
    App* app = (App*)context;
    submenu_reset(app->submenu);
}
