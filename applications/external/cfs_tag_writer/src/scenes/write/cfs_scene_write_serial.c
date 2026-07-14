#include "src/core/cfs_app_i.h"

static void cfs_serial_callback(void* context, int32_t number) {
    CfsApp* app = context;
    snprintf(app->pending.serial, sizeof(app->pending.serial), "%06ld", (long)number);
    view_dispatcher_send_custom_event(app->view_dispatcher, CfsCustomEventButtonWrite);
}

void cfs_scene_write_serial_on_enter(void* context) {
    CfsApp* app = context;
    int32_t current = (int32_t)strtol(app->pending.serial, NULL, 10);
    number_input_set_header_text(app->number_input, "Serial Number");
    number_input_set_result_callback(
        app->number_input, cfs_serial_callback, app, current, 0, 999999);
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewNumberInput);
}

bool cfs_scene_write_serial_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == CfsCustomEventButtonWrite) {
        scene_manager_next_scene(app->scene_manager, CfsSceneWriteConfirm);
        return true;
    }
    return false;
}

void cfs_scene_write_serial_on_exit(void* context) {
    UNUSED(context);
}
