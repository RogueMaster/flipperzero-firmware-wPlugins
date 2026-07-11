#include "../trident_i.h"

typedef enum {
    StartIndexEsp32,
    StartIndexNrf24,
    StartIndexSubghz,
    StartIndexConsole,
    StartIndexSettings,
    StartIndexAbout,
} StartIndex;

static void trident_scene_start_submenu_cb(void* context, uint32_t index) {
    TridentApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void trident_scene_start_on_enter(void* context) {
    TridentApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Trident");
    submenu_add_item(
        submenu, "ESP32  Wi-Fi / BT", StartIndexEsp32, trident_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "NRF24  2.4 GHz", StartIndexNrf24, trident_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "CC1101  Sub-GHz", StartIndexSubghz, trident_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "ESP32 Console", StartIndexConsole, trident_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Settings", StartIndexSettings, trident_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "About", StartIndexAbout, trident_scene_start_submenu_cb, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, TridentSceneStart));

    // Home = idle. Drop the ESP32 link so the UART pins are free for other radios.
    trident_link_disarm(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewSubmenu);
}

bool trident_scene_start_on_event(void* context, SceneManagerEvent event) {
    TridentApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, TridentSceneStart, event.event);
        switch(event.event) {
        case StartIndexEsp32:
            scene_manager_next_scene(app->scene_manager, TridentSceneEsp32);
            consumed = true;
            break;
        case StartIndexNrf24:
            scene_manager_next_scene(app->scene_manager, TridentSceneNrf24scan);
            consumed = true;
            break;
        case StartIndexSubghz:
            scene_manager_next_scene(app->scene_manager, TridentSceneSubghz);
            consumed = true;
            break;
        case StartIndexConsole:
            // Open the console with no staged command: a free ESP32 terminal.
            app->pending_cmd[0] = '\0';
            strncpy(app->pending_title, "Console", sizeof(app->pending_title) - 1);
            scene_manager_next_scene(app->scene_manager, TridentSceneConsole);
            consumed = true;
            break;
        case StartIndexSettings:
            scene_manager_next_scene(app->scene_manager, TridentSceneSettings);
            consumed = true;
            break;
        case StartIndexAbout:
            scene_manager_next_scene(app->scene_manager, TridentSceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void trident_scene_start_on_exit(void* context) {
    TridentApp* app = context;
    submenu_reset(app->submenu);
}
