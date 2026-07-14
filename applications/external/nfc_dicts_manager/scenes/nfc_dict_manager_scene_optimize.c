#include "../nfc_dict_manager.h"
#include "../nfc_dict_manager_utils.h"

void nfc_dict_manager_scene_optimize_on_enter(void* context) {
    NfcDictManager* app = context;

    nfc_dict_manager_create_directories(app->storage);

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, DICT_EXTENSION, &I_file_10px);
    browser_options.base_path = DICT_FOLDER_PATH;
    browser_options.skip_assets = false;

    FuriString* path = furi_string_alloc();
    furi_string_set(path, DICT_FOLDER_PATH);

    bool success =
        dialog_file_browser_show(app->dialogs, app->current_dict, path, &browser_options);
    furi_string_free(path);

    if(success) {
        dialog_ex_reset(app->dialog_ex);

        FuriString* filename = furi_string_alloc();
        path_extract_filename(app->current_dict, filename, false);

        dialog_ex_set_header(app->dialog_ex, "Optimize Dictionary", 64, 0, AlignCenter, AlignTop);

        FuriString* display_text = furi_string_alloc();
        furi_string_printf(
            display_text, "This will create:\n%s_optimized.nfc", furi_string_get_cstr(filename));

        dialog_ex_set_text(
            app->dialog_ex, furi_string_get_cstr(display_text), 64, 20, AlignCenter, AlignTop);
        dialog_ex_set_right_button_text(app->dialog_ex, "Optimize");
        dialog_ex_set_left_button_text(app->dialog_ex, "Cancel");

        dialog_ex_set_context(app->dialog_ex, app);
        dialog_ex_set_result_callback(app->dialog_ex, nfc_dict_manager_dialog_callback);

        furi_string_free(filename);
        furi_string_free(display_text);

        view_dispatcher_switch_to_view(app->view_dispatcher, NfcDictManagerViewSelectType);

    } else {
        scene_manager_previous_scene(app->scene_manager);
    }
}

bool nfc_dict_manager_scene_optimize_on_event(void* context, SceneManagerEvent event) {
    NfcDictManager* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DialogExResultRight) {
            app->optimize_stage = 0;

            popup_set_header(app->popup, "Optimizing...", 64, 10, AlignCenter, AlignTop);
            popup_set_text(
                app->popup,
                "Removing duplicates and\ninvalid keys...",
                64,
                32,
                AlignCenter,
                AlignCenter);
            popup_set_timeout(app->popup, 0);
            popup_set_context(app->popup, app);
            popup_set_callback(app->popup, NULL);

            furi_timer_start(app->optimize_timer, 500);

            view_dispatcher_switch_to_view(app->view_dispatcher, NfcDictManagerViewPopup);

            consumed = true;

        } else if(event.event == DialogExResultLeft) {
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void nfc_dict_manager_scene_optimize_on_exit(void* context) {
    NfcDictManager* app = context;
    dialog_ex_reset(app->dialog_ex);
    popup_reset(app->popup);
}
