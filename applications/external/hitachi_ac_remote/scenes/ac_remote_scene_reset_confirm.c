#include "../ac_remote_app_i.h"

static void dex_reset_confirm_on_result(DialogExResult result, void* context) {
    furi_assert(context);

    AC_RemoteApp* app = context;
    switch(result) {
    case DialogExResultRight:
    case DialogExReleaseRight: {
        uint32_t event = ac_remote_custom_event_pack(AC_RemoteCustomEventTypeResetSettings, 0);
        view_dispatcher_send_custom_event(app->view_dispatcher, event);
        break;
    }
    case DialogExResultLeft:
    case DialogExReleaseLeft: {
        uint32_t event = ac_remote_custom_event_pack(AC_RemoteCustomEventTypeCallResetDialog, 0);
        view_dispatcher_send_custom_event(app->view_dispatcher, event);
        break;
    }
    default:
        break;
    }
}
void ac_remote_scene_reset_confirm_on_enter(void* context) {
    furi_assert(context);

    AC_RemoteApp* app = context;
    DialogEx* dex_reset_confirm = app->dex_reset_confirm;

    dialog_ex_set_header(dex_reset_confirm, "Reset settings?", 63, 0, AlignCenter, AlignTop);
    dialog_ex_set_text(
        dex_reset_confirm,
        "All settings will be reset to a\nsafe default.",
        63,
        31,
        AlignCenter,
        AlignCenter);
    dialog_ex_set_left_button_text(dex_reset_confirm, "Back");
    dialog_ex_set_right_button_text(dex_reset_confirm, "Reset");
    dialog_ex_set_context(dex_reset_confirm, app);
    dialog_ex_set_result_callback(dex_reset_confirm, dex_reset_confirm_on_result);

    view_dispatcher_switch_to_view(app->view_dispatcher, AC_RemoteAppViewResetConfirm);
}

bool ac_remote_scene_reset_confirm_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);

    AC_RemoteApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        uint16_t event_type;
        int16_t event_value;
        ac_remote_custom_event_unpack(event.event, &event_type, &event_value);
        if(event_type == AC_RemoteCustomEventTypeResetSettings) {
            ac_remote_reset_settings(app);
            scene_manager_search_and_switch_to_another_scene(
                app->scene_manager, AC_RemoteSceneHitachi);
            consumed = true;
        } else if(event_type == AC_RemoteCustomEventTypeCallResetDialog) {
            if(event_value == 0) {
                scene_manager_previous_scene(app->scene_manager);
            }
            consumed = true;
        }
    }
    return consumed;
}

void ac_remote_scene_reset_confirm_on_exit(void* context) {
    furi_assert(context);

    AC_RemoteApp* app = context;
    dialog_ex_reset(app->dex_reset_confirm);
}
