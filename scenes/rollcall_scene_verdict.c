#include "../rollcall_i.h"

static void rollcall_scene_verdict_cb(void* context, VerdictEvent event) {
    RollCallApp* app = context;
    if(event == VerdictEventDetails) {
        view_dispatcher_send_custom_event(app->view_dispatcher, RollCallCustomEventDetails);
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, RollCallCustomEventRescan);
    }
}

void rollcall_scene_verdict_on_enter(void* context) {
    RollCallApp* app = context;

    verdict_view_set_verdict(app->verdict_view, &app->verdict);
    verdict_view_set_callback(app->verdict_view, rollcall_scene_verdict_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, RollCallViewVerdict);
}

bool rollcall_scene_verdict_on_event(void* context, SceneManagerEvent event) {
    RollCallApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case RollCallCustomEventDetails:
            scene_manager_next_scene(app->scene_manager, RollCallSceneDetails);
            consumed = true;
            break;
        case RollCallCustomEventRescan:
            /* pop back to the capture scene and start a fresh run */
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, RollCallSceneCapture);
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        /* skip the capture scene, land back on the main menu */
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, RollCallSceneStart);
        consumed = true;
    }
    return consumed;
}

void rollcall_scene_verdict_on_exit(void* context) {
    UNUSED(context);
}
