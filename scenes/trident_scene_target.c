#include "../trident_i.h"
#include <stdio.h>

typedef enum {
    TgtListAp,
    TgtSelectAp,
    TgtSelectAllAp,
    TgtClearAp,
    TgtListSta,
    TgtSelectSta,
    TgtClearSta,
} TgtIndex;

static void trident_scene_target_cb(void* context, uint32_t index) {
    TridentApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

// Fire a one-shot command, then drop into the console showing the refreshed list.
static void quick(TridentApp* app, const char* cmd, const char* list_title, const char* list_cmd) {
    char line[TRIDENT_CMD_MAX];
    trident_link_ensure(app);
    snprintf(line, sizeof(line), "%.60s\n", cmd);
    trident_link_send(app, line);
    trident_launch(app, list_title, list_cmd, false);
}

void trident_scene_target_on_enter(void* context) {
    TridentApp* app = context;
    Submenu* menu = app->submenu;

    submenu_reset(menu);
    submenu_set_header(menu, "Targets");
    submenu_add_item(menu, "List APs", TgtListAp, trident_scene_target_cb, app);
    submenu_add_item(menu, "Select AP by #", TgtSelectAp, trident_scene_target_cb, app);
    submenu_add_item(menu, "Select ALL APs", TgtSelectAllAp, trident_scene_target_cb, app);
    submenu_add_item(menu, "Clear AP list", TgtClearAp, trident_scene_target_cb, app);
    submenu_add_item(menu, "List Stations", TgtListSta, trident_scene_target_cb, app);
    submenu_add_item(menu, "Select Station by #", TgtSelectSta, trident_scene_target_cb, app);
    submenu_add_item(menu, "Clear Station list", TgtClearSta, trident_scene_target_cb, app);

    submenu_set_selected_item(
        menu, scene_manager_get_scene_state(app->scene_manager, TridentSceneTarget));

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewSubmenu);
}

bool trident_scene_target_on_event(void* context, SceneManagerEvent event) {
    TridentApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, TridentSceneTarget, event.event);
        consumed = true;
        switch(event.event) {
        case TgtListAp:
            trident_launch(app, "AP List", MARAUDER_CMD_LIST_AP, false);
            break;
        case TgtSelectAp:
            app->select_kind = 'a';
            scene_manager_next_scene(app->scene_manager, TridentSceneSelect);
            break;
        case TgtSelectAllAp:
            quick(app, MARAUDER_CMD_SELECT_AP_ALL, "AP List", MARAUDER_CMD_LIST_AP);
            break;
        case TgtClearAp:
            quick(app, MARAUDER_CMD_CLEAR_AP, "AP List", MARAUDER_CMD_LIST_AP);
            break;
        case TgtListSta:
            trident_launch(app, "Station List", MARAUDER_CMD_LIST_STA, false);
            break;
        case TgtSelectSta:
            app->select_kind = 's';
            scene_manager_next_scene(app->scene_manager, TridentSceneSelect);
            break;
        case TgtClearSta:
            quick(app, MARAUDER_CMD_CLEAR_STA, "Station List", MARAUDER_CMD_LIST_STA);
            break;
        default:
            consumed = false;
            break;
        }
    }
    return consumed;
}

void trident_scene_target_on_exit(void* context) {
    TridentApp* app = context;
    submenu_reset(app->submenu);
}
