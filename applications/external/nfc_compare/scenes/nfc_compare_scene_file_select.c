#include "../nfc_compare.h"

void nfc_compare_scene_file_select_on_enter(void* context) {
    furi_assert(context);
    NfcCompareApp* app = context;

    FuriString* folder = furi_string_alloc();
    furi_string_set(folder, NFC_COMPARE_NFC_FOLDER);

    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, NFC_COMPARE_NFC_EXT, NULL);
    options.base_path = NFC_COMPARE_NFC_FOLDER;
    options.hide_ext = true;

    bool ok = dialog_file_browser_show(app->dialogs, app->file_path, folder, &options);
    furi_string_free(folder);

    if(!ok) {
        // User cancelled the browser: leave the app.
        view_dispatcher_stop(app->view_dispatcher);
        return;
    }

    app->saved_uid_len = 0;
    if(nfc_device_load(app->saved_device, furi_string_get_cstr(app->file_path))) {
        size_t len = 0;
        const uint8_t* uid = nfc_device_get_uid(app->saved_device, &len);
        if(uid && len > 0) {
            if(len > NFC_COMPARE_UID_MAX_LEN) len = NFC_COMPARE_UID_MAX_LEN;
            memcpy(app->saved_uid, uid, len);
            app->saved_uid_len = len;
        }
    }

    if(app->saved_uid_len == 0) {
        // File could not be loaded or has no UID: report and exit.
        dialog_message_show_storage_error(app->dialogs, "Cannot read UID\nfrom this file");
        view_dispatcher_stop(app->view_dispatcher);
        return;
    }

    scene_manager_next_scene(app->scene_manager, NfcCompareSceneRead);
}

bool nfc_compare_scene_file_select_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void nfc_compare_scene_file_select_on_exit(void* context) {
    UNUSED(context);
}
