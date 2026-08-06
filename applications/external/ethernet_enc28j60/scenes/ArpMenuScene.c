#include "../app_user.h"

// List for the menu options
enum {
    VIEW_IP_LIST_OPTION,
    ARP_SPOOFING_OPTION,
#if DEV_MODE
    ARP_SPOOFING_TO_SPECIFIC_IP_OPTION,
#endif
} arp_action_menu_options;

//  Callback for the Options on the main menu
void arp_actions_menu_callback(void* context, uint32_t index) {
    App* app = (App*)context;

    switch(index) {
    case VIEW_IP_LIST_OPTION:

        scene_manager_set_scene_state(
            app->scene_manager, app_scene_arp_scanner_option, ARP_STATE_SHOW_LIST);

        scene_manager_next_scene(app->scene_manager, app_scene_arp_scanner_option);

        break;

    case ARP_SPOOFING_OPTION:
        scene_manager_next_scene(app->scene_manager, app_scene_arp_spoofing_option);
        break;

#if DEV_MODE
    case ARP_SPOOFING_TO_SPECIFIC_IP_OPTION:
        scene_manager_next_scene(
            app->scene_manager, app_scene_arp_spoofing_specific_ip_menu_option);
        break;
#endif

    default:
        break;
    }
}

// Function for the main menu on enter
void app_scene_arp_actions_menu_on_enter(void* context) {
    App* app = (App*)context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "ARP ACTIONS");

    submenu_add_item(
        app->submenu, "View scanned IPs", VIEW_IP_LIST_OPTION, arp_actions_menu_callback, app);

    submenu_add_item(
        app->submenu, "Spoof All Hosts", ARP_SPOOFING_OPTION, arp_actions_menu_callback, app);

#if DEV_MODE
    submenu_add_item(
        app->submenu,
        "Arp Spoofing Specific IP",
        ARP_SPOOFING_TO_SPECIFIC_IP_OPTION,
        arp_actions_menu_callback,
        app);
#endif

    submenu_set_selected_item(app->submenu, 0);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuView);
}

// Function for the main menu on event
bool app_scene_arp_actions_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = (App*)context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the main menu on exit
void app_scene_arp_actions_menu_on_exit(void* context) {
    App* app = (App*)context;
    submenu_reset(app->submenu);
}
