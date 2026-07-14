#include "app_user.h"

#define TAG "DIALOG LOG"

void frames_dialog_callback(DialogExResult result, void* context) {
    App* app = context;

    switch(result) {
    case DialogExResultLeft:
        app->file_active->frame_index--;
        app_scene_dialog_on_enter(app);
        break;
    case DialogExResultRight:
        app->file_active->frame_index++;
        app_scene_dialog_on_enter(app);
        break;
    case DialogExResultCenter:
        SLCAN_send_frame(app->frame_active, app->send_timestamp);
        break;
    default:
        break;
    }
}

void app_scene_dialog_on_enter(void* context) {
    furi_assert(context);
    App* app = context;

    view_dispatcher_switch_to_view(app->view_dispatcher, LoadingView);

    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_context(app->dialog_ex, app);
    dialog_ex_set_result_callback(app->dialog_ex, frames_dialog_callback);

    frame_extractor(
        app->storage,
        furi_string_get_cstr(app->file_active->path),
        app->frame_active,
        app->file_active->frame_index);

    dialog_ex_set_header(
        app->dialog_ex,
        furi_string_get_cstr(app->frame_active->can_id),
        64,
        12,
        AlignCenter,
        AlignCenter);

    dialog_ex_set_text(
        app->dialog_ex,
        furi_string_get_cstr(app->frame_active->dlc),
        64,
        32,
        AlignCenter,
        AlignCenter);

    if(app->file_active->frame_index > 0) {
        dialog_ex_set_left_button_text(app->dialog_ex, "Prev");
    }

    if(app->file_active->frame_index < app->file_active->frames_count - 1) {
        dialog_ex_set_right_button_text(app->dialog_ex, "Next");
    }

    if(*app->can_send_frame) dialog_ex_set_center_button_text(app->dialog_ex, "Send");

    view_dispatcher_switch_to_view(app->view_dispatcher, DialogView);
}

bool app_scene_dialog_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    bool consumed = false;

    return consumed;
}

void app_scene_dialog_on_exit(void* context) {
    furi_assert(context);
    App* app = context;
    *app->can_send_frame = false;
    app->file_active->frame_index = 0;
    dialog_ex_reset(app->dialog_ex);
}
