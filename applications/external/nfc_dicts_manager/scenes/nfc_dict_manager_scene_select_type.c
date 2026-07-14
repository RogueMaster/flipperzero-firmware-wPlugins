#include "../nfc_dict_manager.h"
#include "../nfc_dict_manager_utils.h"

void nfc_dict_manager_scene_select_type_on_enter(void* context) {
    NfcDictManager* app = context;

    dialog_ex_reset(app->dialog_ex);

    FuriString* filename = furi_string_alloc();
    path_extract_filename(app->current_dict, filename, false);

    dialog_ex_set_header(
        app->dialog_ex, furi_string_get_cstr(filename), 64, 0, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app->dialog_ex,
        "System: Replace system dict\n"
        "User: Replace user dict",
        64,
        20,
        AlignCenter,
        AlignTop);

    dialog_ex_set_left_button_text(app->dialog_ex, "System");
    dialog_ex_set_right_button_text(app->dialog_ex, "User");

    dialog_ex_set_context(app->dialog_ex, app);
    dialog_ex_set_result_callback(app->dialog_ex, nfc_dict_manager_dialog_callback);

    furi_string_free(filename);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcDictManagerViewSelectType);
}

bool nfc_dict_manager_scene_select_type_on_event(void* context, SceneManagerEvent event) {
    NfcDictManager* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DialogExResultLeft) {
            // system dict
            bool success = nfc_dict_manager_copy_file(
                app->storage, furi_string_get_cstr(app->current_dict), SYSTEM_DICT_PATH);
            if(success) {
                notification_message(app->notification, &sequence_success);
                popup_set_header(app->popup, "Success!", 64, 10, AlignCenter, AlignTop);
                popup_set_text(
                    app->popup,
                    "System dictionary\nreplaced successfully!",
                    64,
                    32,
                    AlignCenter,
                    AlignCenter);
                popup_set_timeout(app->popup, 0);
                popup_set_context(app->popup, app);
                popup_set_callback(app->popup, NULL);

                furi_timer_start(app->success_timer, 1000);

                view_dispatcher_switch_to_view(app->view_dispatcher, NfcDictManagerViewPopup);
            } else {
                notification_message(app->notification, &sequence_error);
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, NfcDictManagerSceneStart);
            }
            consumed = true;

        } else if(event.event == DialogExResultRight) {
            // user dict
            bool success = nfc_dict_manager_copy_file(
                app->storage, furi_string_get_cstr(app->current_dict), USER_DICT_PATH);
            if(success) {
                notification_message(app->notification, &sequence_success);
                popup_set_header(app->popup, "Success!", 64, 10, AlignCenter, AlignTop);
                popup_set_text(
                    app->popup,
                    "User dictionary\nreplaced successfully!",
                    64,
                    32,
                    AlignCenter,
                    AlignCenter);
                popup_set_timeout(app->popup, 0);
                popup_set_context(app->popup, app);
                popup_set_callback(app->popup, NULL);

                furi_timer_start(app->success_timer, 1000);

                view_dispatcher_switch_to_view(app->view_dispatcher, NfcDictManagerViewPopup);
            } else {
                notification_message(app->notification, &sequence_error);
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, NfcDictManagerSceneStart);
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void nfc_dict_manager_scene_select_type_on_exit(void* context) {
    NfcDictManager* app = context;
    dialog_ex_reset(app->dialog_ex);
}
