#include "../trident_i.h"

typedef enum {
    DevHelp,
    DevSettings,
    DevClearAll,
    DevUpdate,
    DevReboot,
} DevIndex;

static void trident_scene_device_cb(void* context, uint32_t index) {
    TridentApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void trident_scene_device_on_enter(void* context) {
    TridentApp* app = context;
    Submenu* menu = app->submenu;

    submenu_reset(menu);
    submenu_set_header(menu, "Device");
    submenu_add_item(menu, "Show Commands", DevHelp, trident_scene_device_cb, app);
    submenu_add_item(menu, "Board Settings", DevSettings, trident_scene_device_cb, app);
    submenu_add_item(menu, "Clear All Lists", DevClearAll, trident_scene_device_cb, app);
    submenu_add_item(menu, "Update (SD)", DevUpdate, trident_scene_device_cb, app);
    submenu_add_item(menu, "Reboot Board", DevReboot, trident_scene_device_cb, app);

    submenu_set_selected_item(
        menu, scene_manager_get_scene_state(app->scene_manager, TridentSceneDevice));

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewSubmenu);
}

bool trident_scene_device_on_event(void* context, SceneManagerEvent event) {
    TridentApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, TridentSceneDevice, event.event);
        consumed = true;
        switch(event.event) {
        case DevHelp:
            trident_launch(app, "Commands", MARAUDER_CMD_HELP, false);
            break;
        case DevSettings:
            trident_launch(app, "Board Settings", MARAUDER_CMD_SETTINGS, false);
            break;
        case DevClearAll:
            trident_link_ensure(app);
            trident_link_send(app, MARAUDER_CMD_CLEAR_AP "\n");
            trident_link_send(app, MARAUDER_CMD_CLEAR_STA "\n");
            trident_link_send(app, MARAUDER_CMD_CLEAR_SSID "\n");
            trident_launch(app, "Lists Cleared", MARAUDER_CMD_LIST_AP, false);
            break;
        case DevUpdate:
            trident_launch(app, "Update (SD)", MARAUDER_CMD_UPDATE, false);
            break;
        case DevReboot:
            trident_launch(app, "Reboot", MARAUDER_CMD_REBOOT, false);
            break;
        default:
            consumed = false;
            break;
        }
    }
    return consumed;
}

void trident_scene_device_on_exit(void* context) {
    TridentApp* app = context;
    submenu_reset(app->submenu);
}
