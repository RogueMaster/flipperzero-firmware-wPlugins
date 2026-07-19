#include "../ghosttag_i.h"

static void ghosttag_scene_alert_ok_cb(void* context) {
    GhostTagApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, GhostTagCustomEventOpenDetail);
}

void ghosttag_scene_alert_on_enter(void* context) {
    GhostTagApp* app = context;
    alert_view_set_ok_callback(app->alert_view, ghosttag_scene_alert_ok_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, GhostTagViewAlert);
}

bool ghosttag_scene_alert_on_event(void* context, SceneManagerEvent event) {
    GhostTagApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        alert_view_tick(app->alert_view);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GhostTagCustomEventOpenDetail) {
            device_detail_view_set_record(app->device_detail_view, &app->selected_record);
            scene_manager_next_scene(app->scene_manager, GhostTagSceneDetail);
            consumed = true;
        }
    }
    return consumed;
}

void ghosttag_scene_alert_on_exit(void* context) {
    UNUSED(context);
}
