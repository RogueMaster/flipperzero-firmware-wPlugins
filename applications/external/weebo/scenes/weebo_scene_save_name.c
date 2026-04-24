#include "../weebo_i.h"
#include <lib/toolbox/name_generator.h>
#include <gui/modules/validators.h>
#include <toolbox/path.h>

#define NFC_APP_PATH_PREFIX   "/ext/nfc"
#define WEEBO_APP_FILE_PREFIX "weebo_"

#define TAG "WeeboSceneSaveName"

void weebo_scene_save_name_text_input_callback(void* context) {
    Weebo* weebo = context;

    view_dispatcher_send_custom_event(weebo->view_dispatcher, WeeboCustomEventTextInputDone);
}

void weebo_scene_save_name_on_enter(void* context) {
    Weebo* weebo = context;

    // Setup view
    TextInput* text_input = weebo->text_input;
    bool file_name_empty = false;
    if(!strcmp(weebo->file_name, "")) {
        name_generator_make_auto(
            weebo->text_store, sizeof(weebo->text_store), WEEBO_APP_FILE_PREFIX);
        file_name_empty = true;
    } else {
        // Add "_" suffix to make name unique
        weebo->file_name[strlen(weebo->file_name)] = '_';
        weebo->file_name[strlen(weebo->file_name)] = '\0';
        weebo_text_store_set(weebo, weebo->file_name);
    }
    text_input_set_header_text(text_input, "Name the card");
    text_input_set_result_callback(
        text_input,
        weebo_scene_save_name_text_input_callback,
        weebo,
        weebo->text_store,
        sizeof(weebo->text_store),
        file_name_empty);

    FuriString* folder_path;
    folder_path = furi_string_alloc_set(NFC_APP_PATH_PREFIX);

    if(furi_string_end_with(weebo->load_path, NFC_APP_EXTENSION)) {
        path_extract_dirname(furi_string_get_cstr(weebo->load_path), folder_path);
    }

    ValidatorIsFile* validator_is_file = validator_is_file_alloc_init(
        furi_string_get_cstr(folder_path), NFC_APP_EXTENSION, weebo->file_name);
    text_input_set_validator(text_input, validator_is_file_callback, validator_is_file);

    view_dispatcher_switch_to_view(weebo->view_dispatcher, WeeboViewTextInput);

    furi_string_free(folder_path);
}

bool weebo_scene_save_name_on_event(void* context, SceneManagerEvent event) {
    Weebo* weebo = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WeeboCustomEventTextInputDone) {
            strlcpy(weebo->file_name, weebo->text_store, strlen(weebo->text_store) + 1);

            FuriString* path = furi_string_alloc_set(NFC_APP_PATH_PREFIX);
            if(furi_string_end_with(weebo->load_path, NFC_APP_EXTENSION)) {
                path_extract_dirname(furi_string_get_cstr(weebo->load_path), path);
            }
            furi_string_cat_printf(path, "/%s%s", weebo->file_name, NFC_APP_EXTENSION);
            FURI_LOG_D(TAG, "Saving to %s", furi_string_get_cstr(path));

            if(nfc_device_save(weebo->nfc_device, furi_string_get_cstr(path))) {
                scene_manager_next_scene(weebo->scene_manager, WeeboSceneSaveSuccess);
                consumed = true;
            } else {
                consumed = scene_manager_search_and_switch_to_previous_scene(
                    weebo->scene_manager, WeeboSceneMainMenu);
            }

            furi_string_free(path);
        }
    }
    return consumed;
}

void weebo_scene_save_name_on_exit(void* context) {
    Weebo* weebo = context;

    // Clear view
    void* validator_context = text_input_get_validator_callback_context(weebo->text_input);
    text_input_set_validator(weebo->text_input, NULL, NULL);
    validator_is_file_free(validator_context);

    text_input_reset(weebo->text_input);
}
