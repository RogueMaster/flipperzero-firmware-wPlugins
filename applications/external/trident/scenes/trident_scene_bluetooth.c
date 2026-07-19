#include "../trident_i.h"

typedef enum {
    BtSniff,
    BtSkimmer,
    BtAirtag,
    BtSpam,
} BtIndex;

static void trident_scene_bluetooth_cb(void* context, uint32_t index) {
    TridentApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void trident_scene_bluetooth_on_enter(void* context) {
    TridentApp* app = context;
    Submenu* menu = app->submenu;

    submenu_reset(menu);
    submenu_set_header(menu, "Bluetooth");
    submenu_add_item(menu, "Sniff Bluetooth", BtSniff, trident_scene_bluetooth_cb, app);
    submenu_add_item(menu, "Detect Skimmers", BtSkimmer, trident_scene_bluetooth_cb, app);
    submenu_add_item(menu, "Sniff AirTags", BtAirtag, trident_scene_bluetooth_cb, app);
    submenu_add_item(menu, "BLE Spam", BtSpam, trident_scene_bluetooth_cb, app);

    submenu_set_selected_item(
        menu, scene_manager_get_scene_state(app->scene_manager, TridentSceneBluetooth));

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewSubmenu);
}

bool trident_scene_bluetooth_on_event(void* context, SceneManagerEvent event) {
    TridentApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, TridentSceneBluetooth, event.event);
        consumed = true;
        switch(event.event) {
        case BtSniff:
            trident_launch(app, "Sniff Bluetooth", MARAUDER_CMD_BT_SNIFF, false);
            break;
        case BtSkimmer:
            trident_launch(app, "Detect Skimmers", MARAUDER_CMD_BT_SKIMMER, false);
            break;
        case BtAirtag:
            trident_launch(app, "Sniff AirTags", MARAUDER_CMD_BT_AIRTAG, false);
            break;
        case BtSpam:
            scene_manager_next_scene(app->scene_manager, TridentSceneBlespam);
            break;
        default:
            consumed = false;
            break;
        }
    }
    return consumed;
}

void trident_scene_bluetooth_on_exit(void* context) {
    TridentApp* app = context;
    submenu_reset(app->submenu);
}
