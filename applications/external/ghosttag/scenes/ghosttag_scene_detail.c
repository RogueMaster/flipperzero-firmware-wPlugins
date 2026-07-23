#include "../ghosttag_i.h"

void ghosttag_scene_detail_on_enter(void* context) {
    GhostTagApp* app = context;
    device_detail_view_set_record(app->device_detail_view, &app->selected_record);
    view_dispatcher_switch_to_view(app->view_dispatcher, GhostTagViewDeviceDetail);
}

bool ghosttag_scene_detail_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void ghosttag_scene_detail_on_exit(void* context) {
    UNUSED(context);
}
