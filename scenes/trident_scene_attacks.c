#include "../trident_i.h"

typedef enum {
    AtkDeauth,
    AtkBeaconList,
    AtkBeaconRandom,
    AtkBeaconAp,
    AtkProbe,
    AtkRickroll,
} AtkIndex;

static void trident_scene_attacks_cb(void* context, uint32_t index) {
    TridentApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void trident_scene_attacks_on_enter(void* context) {
    TridentApp* app = context;
    Submenu* menu = app->submenu;

    submenu_reset(menu);
    submenu_set_header(menu, "Attacks");
    submenu_add_item(menu, "Deauth Flood", AtkDeauth, trident_scene_attacks_cb, app);
    submenu_add_item(menu, "Beacon Spam (list)", AtkBeaconList, trident_scene_attacks_cb, app);
    submenu_add_item(menu, "Beacon Spam (random)", AtkBeaconRandom, trident_scene_attacks_cb, app);
    submenu_add_item(menu, "Beacon Spam (AP clone)", AtkBeaconAp, trident_scene_attacks_cb, app);
    submenu_add_item(menu, "Probe Flood", AtkProbe, trident_scene_attacks_cb, app);
    submenu_add_item(menu, "Rickroll Beacon", AtkRickroll, trident_scene_attacks_cb, app);

    submenu_set_selected_item(
        menu, scene_manager_get_scene_state(app->scene_manager, TridentSceneAttacks));

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewSubmenu);
}

bool trident_scene_attacks_on_event(void* context, SceneManagerEvent event) {
    TridentApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, TridentSceneAttacks, event.event);
        consumed = true;
        switch(event.event) {
        case AtkDeauth:
            trident_launch(app, "Deauth Flood", MARAUDER_CMD_ATTACK_DEAUTH, true);
            break;
        case AtkBeaconList:
            trident_launch(app, "Beacon (list)", MARAUDER_CMD_ATTACK_BEACON_L, true);
            break;
        case AtkBeaconRandom:
            trident_launch(app, "Beacon (random)", MARAUDER_CMD_ATTACK_BEACON_R, true);
            break;
        case AtkBeaconAp:
            trident_launch(app, "Beacon (AP clone)", MARAUDER_CMD_ATTACK_BEACON_AP, true);
            break;
        case AtkProbe:
            trident_launch(app, "Probe Flood", MARAUDER_CMD_ATTACK_PROBE, true);
            break;
        case AtkRickroll:
            trident_launch(app, "Rickroll Beacon", MARAUDER_CMD_ATTACK_RICKROLL, true);
            break;
        default:
            consumed = false;
            break;
        }
    }
    return consumed;
}

void trident_scene_attacks_on_exit(void* context) {
    TridentApp* app = context;
    submenu_reset(app->submenu);
}
