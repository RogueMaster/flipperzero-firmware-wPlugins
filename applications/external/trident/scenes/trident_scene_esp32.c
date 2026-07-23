#include "../trident_i.h"

typedef enum {
    Esp32Wifi,
    Esp32Bluetooth,
    Esp32Gps,
    Esp32Device,
    Esp32Console,
} Esp32Index;

static void trident_scene_esp32_cb(void* context, uint32_t index) {
    TridentApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void trident_scene_esp32_on_enter(void* context) {
    TridentApp* app = context;
    Submenu* menu = app->submenu;

    submenu_reset(menu);
    submenu_set_header(menu, "ESP32 - Wi-Fi / BT");
    submenu_add_item(menu, "Wi-Fi", Esp32Wifi, trident_scene_esp32_cb, app);
    submenu_add_item(menu, "Bluetooth", Esp32Bluetooth, trident_scene_esp32_cb, app);
    submenu_add_item(menu, "GPS / Wardrive", Esp32Gps, trident_scene_esp32_cb, app);
    submenu_add_item(menu, "Device", Esp32Device, trident_scene_esp32_cb, app);
    submenu_add_item(menu, "Console", Esp32Console, trident_scene_esp32_cb, app);

    submenu_set_selected_item(
        menu, scene_manager_get_scene_state(app->scene_manager, TridentSceneEsp32));

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewSubmenu);
}

bool trident_scene_esp32_on_event(void* context, SceneManagerEvent event) {
    TridentApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, TridentSceneEsp32, event.event);
        consumed = true;
        switch(event.event) {
        case Esp32Wifi:
            scene_manager_next_scene(app->scene_manager, TridentSceneWifi);
            break;
        case Esp32Bluetooth:
            scene_manager_next_scene(app->scene_manager, TridentSceneBluetooth);
            break;
        case Esp32Gps:
            scene_manager_next_scene(app->scene_manager, TridentSceneGps);
            break;
        case Esp32Device:
            scene_manager_next_scene(app->scene_manager, TridentSceneDevice);
            break;
        case Esp32Console:
            app->pending_cmd[0] = '\0';
            strncpy(app->pending_title, "Console", sizeof(app->pending_title) - 1);
            scene_manager_next_scene(app->scene_manager, TridentSceneConsole);
            break;
        default:
            consumed = false;
            break;
        }
    }
    return consumed;
}

void trident_scene_esp32_on_exit(void* context) {
    TridentApp* app = context;
    submenu_reset(app->submenu);
}
