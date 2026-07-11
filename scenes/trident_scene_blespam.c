#include "../trident_i.h"

typedef enum {
    BleSpamApple,
    BleSpamSamsung,
    BleSpamGoogle,
    BleSpamWindows,
    BleSpamAll,
    BleSourApple,
} BleSpamIndex;

static void trident_scene_blespam_cb(void* context, uint32_t index) {
    TridentApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void trident_scene_blespam_on_enter(void* context) {
    TridentApp* app = context;
    Submenu* menu = app->submenu;

    submenu_reset(menu);
    submenu_set_header(menu, "BLE Spam");
    submenu_add_item(menu, "Apple", BleSpamApple, trident_scene_blespam_cb, app);
    submenu_add_item(menu, "Samsung", BleSpamSamsung, trident_scene_blespam_cb, app);
    submenu_add_item(menu, "Google", BleSpamGoogle, trident_scene_blespam_cb, app);
    submenu_add_item(menu, "Windows", BleSpamWindows, trident_scene_blespam_cb, app);
    submenu_add_item(menu, "All Brands", BleSpamAll, trident_scene_blespam_cb, app);
    submenu_add_item(menu, "Sour Apple", BleSourApple, trident_scene_blespam_cb, app);

    submenu_set_selected_item(
        menu, scene_manager_get_scene_state(app->scene_manager, TridentSceneBlespam));

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewSubmenu);
}

bool trident_scene_blespam_on_event(void* context, SceneManagerEvent event) {
    TridentApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, TridentSceneBlespam, event.event);
        consumed = true;
        switch(event.event) {
        case BleSpamApple:
            trident_launch(app, "BLE Spam Apple", MARAUDER_CMD_BLE_SPAM_APPLE, true);
            break;
        case BleSpamSamsung:
            trident_launch(app, "BLE Spam Samsung", MARAUDER_CMD_BLE_SPAM_SAMSUNG, true);
            break;
        case BleSpamGoogle:
            trident_launch(app, "BLE Spam Google", MARAUDER_CMD_BLE_SPAM_GOOGLE, true);
            break;
        case BleSpamWindows:
            trident_launch(app, "BLE Spam Windows", MARAUDER_CMD_BLE_SPAM_WINDOWS, true);
            break;
        case BleSpamAll:
            trident_launch(app, "BLE Spam All", MARAUDER_CMD_BLE_SPAM_ALL, true);
            break;
        case BleSourApple:
            trident_launch(app, "Sour Apple", MARAUDER_CMD_BLE_SOURAPPLE, true);
            break;
        default:
            consumed = false;
            break;
        }
    }
    return consumed;
}

void trident_scene_blespam_on_exit(void* context) {
    TridentApp* app = context;
    submenu_reset(app->submenu);
}
