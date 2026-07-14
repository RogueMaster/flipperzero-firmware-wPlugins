#include "../chameleon_app_i.h"

typedef enum {
    SubmenuIndexUSB,
    SubmenuIndexBluetooth,
} SubmenuIndex;

static void chameleon_scene_connection_type_submenu_callback(void* context, uint32_t index) {
    ChameleonApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void chameleon_scene_connection_type_on_enter(void* context) {
    ChameleonApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);

    submenu_add_item(
        submenu,
        "USB Connection",
        SubmenuIndexUSB,
        chameleon_scene_connection_type_submenu_callback,
        app);

    // BLE is disabled due to Flipper Zero hardware limitation
    // The Flipper BLE stack only supports peripheral/server mode, not central/client
    // Uncomment when Flipper adds BLE central support:
    /*
    submenu_add_item(
        submenu,
        "Bluetooth Connection",
        SubmenuIndexBluetooth,
        chameleon_scene_connection_type_submenu_callback,
        app);
    */

    view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewSubmenu);
}

bool chameleon_scene_connection_type_on_event(void* context, SceneManagerEvent event) {
    ChameleonApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SubmenuIndexUSB:
            app->connection_type = ChameleonConnectionUSB;
            scene_manager_next_scene(app->scene_manager, ChameleonSceneUsbConnect);
            consumed = true;
            break;
        case SubmenuIndexBluetooth:
            app->connection_type = ChameleonConnectionBLE;
            scene_manager_next_scene(app->scene_manager, ChameleonSceneBleScan);
            consumed = true;
            break;
        }
    }

    return consumed;
}

void chameleon_scene_connection_type_on_exit(void* context) {
    ChameleonApp* app = context;
    submenu_reset(app->submenu);
}
