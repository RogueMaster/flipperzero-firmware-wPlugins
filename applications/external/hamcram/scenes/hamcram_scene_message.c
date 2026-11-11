#include "../hamcram.h"

static void message_dialog_callback(DialogExResult result, void* context) {
    HamCram* app = context;
    UNUSED(result);
    view_dispatcher_send_custom_event(app->view_dispatcher, HamCramEventCenter);
}

void hamcram_scene_message_on_enter(void* context) {
    HamCram* app = context;
    DialogEx* dialog = app->dialog;

    dialog_ex_set_header(dialog, app->message_header, 64, 2, AlignCenter, AlignTop);
    dialog_ex_set_text(dialog, app->message_text, 64, 30, AlignCenter, AlignCenter);
    dialog_ex_set_center_button_text(dialog, "OK");
    dialog_ex_set_context(dialog, app);
    dialog_ex_set_result_callback(dialog, message_dialog_callback);
    view_dispatcher_switch_to_view(app->view_dispatcher, HamCramViewDialog);
}

bool hamcram_scene_message_on_event(void* context, SceneManagerEvent event) {
    HamCram* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    scene_manager_previous_scene(app->scene_manager);
    return true;
}

void hamcram_scene_message_on_exit(void* context) {
    HamCram* app = context;
    dialog_ex_reset(app->dialog);
}
