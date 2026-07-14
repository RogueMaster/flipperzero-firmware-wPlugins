#include "droidbeacon_scene.h"

static void menu_callback(void* context, uint32_t index) {
    furi_assert(context);
    DroidbeaconApp* app = context;
    app->selected_beacon = (uint8_t)index;
    scene_manager_handle_custom_event(app->scene_manager, DroidbeaconEventMenuSelect);
}

void droidbeacon_scene_menu_on_enter(void* context) {
    furi_assert(context);
    DroidbeaconApp* app = context;

    submenu_reset(app->menu);
    submenu_set_header(app->menu, "Select Location");

    for(uint8_t i = 0; i < DROID_BEACON_COUNT; i++) {
        submenu_add_item(app->menu, DROID_BEACONS[i].name, i, menu_callback, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, DroidbeaconViewMenu);
}

bool droidbeacon_scene_menu_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    DroidbeaconApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DroidbeaconEventMenuSelect) {
            scene_manager_next_scene(app->scene_manager, DroidbeaconSceneBeaming);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void droidbeacon_scene_menu_on_exit(void* context) {
    furi_assert(context);
    DroidbeaconApp* app = context;
    submenu_reset(app->menu);
}
