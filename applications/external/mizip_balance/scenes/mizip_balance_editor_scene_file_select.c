#include "../mizip_balance_editor_i.h"

void mizip_balance_editor_scene_file_select_on_enter(void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;

    //Resetting data
    furi_string_set(app->filePath, NFC_APP_FOLDER);
    furi_string_set(app->shadowFilePath, NFC_APP_FOLDER);
    app->is_shadow_file_exists = false;
    mf_classic_reset(app->mf_classic_data);
    app->uid[0] = 0x00;
    app->uid[1] = 0x00;
    app->uid[2] = 0x00;
    app->uid[3] = 0x00;

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, NFC_APP_EXTENSION, &I_Nfc_10px);
    browser_options.base_path = NFC_APP_FOLDER;
    browser_options.hide_dot_files = true;
    //Show file browser
    if(!dialog_file_browser_show(app->dialogs, app->filePath, app->filePath, &browser_options)) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    } else {
        //When file is selected, calculate his shadow file path
        FURI_LOG_D("MiZipBalanceEditor", "File selected: %s", furi_string_get_cstr(app->filePath));
        furi_string_set(app->shadowFilePath, app->filePath);
        furi_string_replace(
            app->shadowFilePath,
            NFC_APP_EXTENSION,
            NFC_APP_SHADOW_EXTENSION,
            furi_string_size(app->shadowFilePath) - 4);
        FURI_LOG_D(
            "MiZipBalanceEditor", "Shadow file: %s", furi_string_get_cstr(app->shadowFilePath));

        //Check if shadow file exists
        app->is_shadow_file_exists =
            storage_file_exists(app->storage, furi_string_get_cstr(app->shadowFilePath));

        //Set filePath to shadow file to load the correct data
        if(app->is_shadow_file_exists) {
            FURI_LOG_D("MiZipBalanceEditor", "Shadow file exists!");
            app->filePath = app->shadowFilePath;
        } else {
            FURI_LOG_D("MiZipBalanceEditor", "No shadow file found, will be create on write");
        }

        //Check if file is MiZip file
        if(nfc_device_load(app->nfc_device, furi_string_get_cstr(app->filePath))) {
            app->is_file_loaded = true;
            FURI_LOG_D(
                "MiZipBalanceEditor", "Loaded file: %s", furi_string_get_cstr(app->filePath));
            if(nfc_device_get_protocol(app->nfc_device) == NfcProtocolMfClassic) {
                mf_classic_copy(
                    app->mf_classic_data,
                    nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic));
                memcpy(app->uid, app->mf_classic_data->iso14443_3a_data->uid, UID_LENGTH);
                app->is_valid_mizip_data = mizip_parse(context);
            } else {
                app->is_valid_mizip_data = false;
            }
            scene_manager_next_scene(app->scene_manager, MiZipBalanceEditorViewIdShowBalance);
        } else {
            app->is_file_loaded = false;
            app->is_valid_mizip_data = false;
            FURI_LOG_D("MiZipBalanceEditor", "Unable to load file");
            scene_manager_next_scene(app->scene_manager, MiZipBalanceEditorViewIdShowBalance);
        }
    }
}

bool mizip_balance_editor_scene_file_select_on_event(void* context, SceneManagerEvent event) {
    MiZipBalanceEditorApp* app = context;
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
    }
    return false;
}

void mizip_balance_editor_scene_file_select_on_exit(void* context) {
    UNUSED(context);
}
