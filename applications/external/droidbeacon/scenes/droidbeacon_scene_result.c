#include "droidbeacon_scene.h"

static void result_popup_callback(void* context) {
    furi_assert(context);
    DroidbeaconApp* app = context;
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, DroidbeaconSceneMenu);
}

void droidbeacon_scene_result_on_enter(void* context) {
    furi_assert(context);
    DroidbeaconApp* app = context;

    popup_reset(app->result_popup);
    popup_set_callback(app->result_popup, result_popup_callback);
    popup_set_context(app->result_popup, app);
    popup_enable_timeout(app->result_popup);
    popup_set_timeout(app->result_popup, 2000);

    popup_set_header(app->result_popup, "Beacon Stopped", 64, 10, AlignCenter, AlignTop);
    popup_set_text(app->result_popup,
        DROID_BEACONS[app->selected_beacon].name,
        64, 32, AlignCenter, AlignCenter);

    view_dispatcher_switch_to_view(app->view_dispatcher, DroidbeaconViewResult);
}

bool droidbeacon_scene_result_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    DroidbeaconApp* app = context;
    UNUSED(app);
    UNUSED(event);
    bool consumed = false;
    return consumed;
}

void droidbeacon_scene_result_on_exit(void* context) {
    furi_assert(context);
    DroidbeaconApp* app = context;
    popup_reset(app->result_popup);
}
