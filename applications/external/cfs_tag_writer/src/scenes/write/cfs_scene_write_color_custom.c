#include "src/core/cfs_app_i.h"

static void cfs_color_custom_callback(void* context) {
    CfsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, CfsCustomEventButtonWrite);
}

void cfs_scene_write_color_custom_on_enter(void* context) {
    CfsApp* app = context;
    byte_input_set_header_text(app->byte_input, "Enter RGB (3 bytes)");
    byte_input_set_result_callback(
        app->byte_input, cfs_color_custom_callback, NULL, app, app->color_bytes, 3);
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewByteInput);
}

bool cfs_scene_write_color_custom_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == CfsCustomEventButtonWrite) {
        app->pending.color_r = app->color_bytes[0];
        app->pending.color_g = app->color_bytes[1];
        app->pending.color_b = app->color_bytes[2];
        scene_manager_next_scene(app->scene_manager, CfsSceneWriteWeight);
        return true;
    }
    return false;
}

void cfs_scene_write_color_custom_on_exit(void* context) {
    UNUSED(context);
}
