#include "../warden_i.h"

static void warden_scene_result_cb(void* context, ResultEvent event) {
    WardenApp* app = context;
    if(event == ResultEventDetails) {
        view_dispatcher_send_custom_event(app->view_dispatcher, WardenCustomEventDetails);
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, WardenCustomEventRescan);
    }
}

void warden_scene_result_on_enter(void* context) {
    WardenApp* app = context;

    result_view_set_grade(app->result_view, &app->grade);
    result_view_set_callback(app->result_view, warden_scene_result_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, WardenViewResult);
}

bool warden_scene_result_on_event(void* context, SceneManagerEvent event) {
    WardenApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case WardenCustomEventDetails:
            scene_manager_next_scene(app->scene_manager, WardenSceneDetails);
            consumed = true;
            break;
        case WardenCustomEventRescan:
            /* pop back to the (transient) scan scene and re-arm the field */
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, WardenSceneScan);
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        /* skip the transient scan scene, land back on the main menu */
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, WardenSceneStart);
        consumed = true;
    }
    return consumed;
}

void warden_scene_result_on_exit(void* context) {
    UNUSED(context);
}
