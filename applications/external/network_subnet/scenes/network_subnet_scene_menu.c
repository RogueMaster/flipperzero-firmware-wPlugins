#include "network_subnet.h"
#include "scenes/scenes.h"

void network_subnet_submenu_set_callback(void* context, uint32_t index) {
    furi_assert(context);
    NetworkSubnetApp* app = context;
    switch(index) {
    case InputModeIpSubnet:
        view_dispatcher_send_custom_event(app->view_dispatcher, EventMenuIpSelected);
        break;
    case InputModeMask:
        view_dispatcher_send_custom_event(app->view_dispatcher, EventMenuMaskSelected);
        break;
    }
}

void network_subnet_scene_menu_on_enter(void* context) {
    furi_assert(context);
    NetworkSubnetApp* app = context;
    submenu_reset(app->submenu);
    submenu_add_item(
        app->submenu, "IP / CIDR", InputModeIpSubnet, network_subnet_submenu_set_callback, app);
    submenu_add_item(
        app->submenu,
        "Subnet Mask -> IP / CIDR",
        InputModeMask,
        network_subnet_submenu_set_callback,
        app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdMenu);
}

bool network_subnet_scene_menu_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    NetworkSubnetApp* app = context;
    UNUSED(app);
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case EventMenuIpSelected:
            consumed = true;
            scene_manager_next_scene(app->scene_manager, NetworkSubnetSceneIpInput);
            break;
        case EventMenuMaskSelected:
            FURI_LOG_D(
                NETWORK_SUBNET_TAG,
                "Menu mask selected and now setting next scene to NetworkSubnetSceneIpInput");
            scene_manager_next_scene(app->scene_manager, NetworkSubnetSceneMaskInput);
            consumed = true;
            break;
        }
        break;
    default:
        consumed = false;
    }
    return consumed;
}
void network_subnet_scene_menu_on_exit(void* context) {
    furi_assert(context);
    NetworkSubnetApp* app = context;
    submenu_reset(app->submenu);
}
