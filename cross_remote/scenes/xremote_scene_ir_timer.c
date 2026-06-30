#include "../xremote.h"
#include "../models/cross/xremote_cross_remote.h"

void xremote_scene_ir_timer_callback(void* context, int32_t number) {
    XRemote* app = context;
    CrossRemoteItem* item = xremote_cross_remote_get_item(app->cross_remote, app->edit_item);
    item->time = number;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void xremote_scene_ir_timer_on_enter(void* context) {
    furi_assert(context);
    XRemote* app = context;
    NumberInput* number_input = app->number_input;

    char str[50];
    int32_t min_value = 0;
    int32_t max_value = 9999;
    snprintf(str, sizeof(str), "Transmit in ms (%ld - %ld)", min_value, max_value);
    CrossRemoteItem* item = xremote_cross_remote_get_item(app->cross_remote, app->edit_item);

    number_input_set_header_text(number_input, str);
    number_input_set_result_callback(
        number_input, xremote_scene_ir_timer_callback, context, item->time, min_value, max_value);

    view_dispatcher_switch_to_view(app->view_dispatcher, XRemoteViewIdNumberInput);
}

bool xremote_scene_ir_timer_on_event(void* context, SceneManagerEvent event) {
    XRemote* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return consumed;
}

void xremote_scene_ir_timer_on_exit(void* context) {
    XRemote* app = context;
    UNUSED(app);
}