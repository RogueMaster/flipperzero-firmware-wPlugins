// Also from https://github.com/bettse/picopass

#include "../saflip.h"
#include "../util/storage.h"
#include "name_generator.h"

// For saving as an NFC file
#define NFC_APP_FOLDER    EXT_PATH("nfc")
#define NFC_APP_EXTENSION ".nfc"

enum {
    SaflipSceneSaveEventSave,
    SaflipSceneSaveEventClose,
};

void saflip_scene_save_popup_callback(void* context) {
    SaflipApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, SaflipSceneSaveEventClose);
}

void saflip_scene_save_text_input_callback(void* context) {
    SaflipApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, SaflipSceneSaveEventSave);
}

void saflip_scene_save_on_enter(void* context) {
    SaflipApp* app = context;

    // Setup view
    TextInput* text_input = app->text_input;
    bool dev_name_empty = false;

    name_generator_make_auto(app->text_store, sizeof(app->text_store), SAFLIP_APP_FILE_PREFIX);

    text_input_set_header_text(text_input, "Name the card");
    text_input_set_result_callback(
        text_input,
        saflip_scene_save_text_input_callback,
        app,
        app->text_store,
        sizeof(app->text_store),
        dev_name_empty);

    FuriString* folder_path;
    folder_path = furi_string_alloc_set(STORAGE_APP_DATA_PATH_PREFIX);

    ValidatorIsFile* validator_is_file = validator_is_file_alloc_init(
        furi_string_get_cstr(folder_path), SAFLIP_APP_EXTENSION, app->text_store);
    text_input_set_validator(text_input, validator_is_file_callback, validator_is_file);

    view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewTextInput);

    furi_string_free(folder_path);
}

bool saflip_scene_save_on_event(void* context, SceneManagerEvent event) {
    SaflipApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SaflipSceneSaveEventSave) {
            switch(scene_manager_get_scene_state(app->scene_manager, SaflipSceneSave)) {
            case SaflipSaveModeSaflip:
                FuriString* saflip_full_path = furi_string_alloc_printf(
                    "%s/%s%s", STORAGE_APP_DATA_PATH_PREFIX, app->text_store, SAFLIP_APP_EXTENSION);

                if(saflip_save_file(app, furi_string_get_cstr(saflip_full_path))) {
                    Popup* popup = app->popup;
                    popup_set_icon(popup, 36, 5, &I_DolphinDone_80x58);
                    popup_set_header(popup, "Saved!", 13, 22, AlignLeft, AlignBottom);
                    popup_set_timeout(popup, 1500);
                    popup_set_context(popup, app);
                    popup_set_callback(popup, saflip_scene_save_popup_callback);
                    popup_enable_timeout(popup);
                    view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewPopup);
                } else {
                    dialog_message_show_storage_error(app->dialogs_app, "Cannot save file");
                }
                furi_string_free(saflip_full_path);
                consumed = true;
                break;

            case SaflipSaveModeNFC:
                FuriString* nfc_full_path = furi_string_alloc();

                if(saflok_generate(app->nfc_device, app->uid, app->uid_len, app->data)) {
                    furi_string_cat_printf(
                        nfc_full_path,
                        "%s/%s%s",
                        NFC_APP_FOLDER,
                        app->text_store,
                        NFC_APP_EXTENSION);

                    if(nfc_device_save(app->nfc_device, furi_string_get_cstr(nfc_full_path))) {
                        Popup* popup = app->popup;
                        popup_set_icon(popup, 36, 5, &I_DolphinDone_80x58);
                        popup_set_header(popup, "Saved!", 13, 22, AlignLeft, AlignBottom);
                        popup_set_timeout(popup, 1500);
                        popup_set_context(popup, app);
                        popup_set_callback(popup, saflip_scene_save_popup_callback);
                        popup_enable_timeout(popup);
                        view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewPopup);
                    } else {
                        dialog_message_show_storage_error(app->dialogs_app, "Cannot save file");
                    }
                } else {
                    dialog_message_show_storage_error(app->dialogs_app, "Cannot save file");
                }

                furi_string_free(nfc_full_path);
                consumed = true;
                break;
            }
        } else if(event.event == SaflipSceneSaveEventClose) {
            if(scene_manager_has_previous_scene(app->scene_manager, SaflipSceneStart)) {
                uint32_t scenes[] = {
                    SaflipSceneOptions,
                    SaflipSceneStart,
                };
                consumed = scene_manager_search_and_switch_to_previous_scene_one_of(
                    app->scene_manager, scenes, COUNT_OF(scenes));
            }
        }
    }

    return consumed;
}

void saflip_scene_save_on_exit(void* context) {
    SaflipApp* app = context;

    // Clear view
    void* validator_context = text_input_get_validator_callback_context(app->text_input);
    text_input_set_validator(app->text_input, NULL, NULL);
    validator_is_file_free(validator_context);

    text_input_reset(app->text_input);
}
