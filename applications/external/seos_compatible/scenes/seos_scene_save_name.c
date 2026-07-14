#include "../seos_i.h"
#include <lib/toolbox/name_generator.h>
#include <gui/modules/validators.h>
#include <toolbox/path.h>

#define SEOS_APP_FILE_PREFIX "Seos"

void seos_scene_save_name_text_input_callback(void* context) {
    Seos* seos = context;

    view_dispatcher_send_custom_event(seos->view_dispatcher, SeosCustomEventTextInputDone);
}

void seos_scene_save_name_on_enter(void* context) {
    Seos* seos = context;

    // Setup view
    TextInput* text_input = seos->text_input;
    bool dev_name_empty = false;
    if(!strcmp(seos->credential->name, "")) {
        name_generator_make_auto(seos->text_store, sizeof(seos->text_store), SEOS_APP_FILE_PREFIX);
        dev_name_empty = true;
    } else {
        seos_text_store_set(seos, seos->credential->name);
    }
    text_input_set_header_text(text_input, "Name the card");
    text_input_set_result_callback(
        text_input,
        seos_scene_save_name_text_input_callback,
        seos,
        seos->text_store,
        sizeof(seos->text_store),
        dev_name_empty);

    FuriString* folder_path;
    folder_path = furi_string_alloc_set(STORAGE_APP_DATA_PATH_PREFIX);

    if(furi_string_end_with(seos->credential->load_path, SEOS_APP_EXTENSION)) {
        path_extract_dirname(furi_string_get_cstr(seos->credential->load_path), folder_path);
    }

    ValidatorIsFile* validator_is_file = validator_is_file_alloc_init(
        furi_string_get_cstr(folder_path), SEOS_APP_EXTENSION, seos->credential->name);
    text_input_set_validator(text_input, validator_is_file_callback, validator_is_file);

    view_dispatcher_switch_to_view(seos->view_dispatcher, SeosViewTextInput);

    furi_string_free(folder_path);
}

bool seos_scene_save_name_on_event(void* context, SceneManagerEvent event) {
    Seos* seos = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SeosCustomEventTextInputDone) {
            strlcpy(seos->credential->name, seos->text_store, strlen(seos->text_store) + 1);
            if(seos_credential_save(seos->credential, seos->text_store)) {
                scene_manager_next_scene(seos->scene_manager, SeosSceneSaveSuccess);
                consumed = true;
            } else {
                consumed = scene_manager_search_and_switch_to_previous_scene(
                    seos->scene_manager, SeosSceneMainMenu);
            }
        }
    }
    return consumed;
}

void seos_scene_save_name_on_exit(void* context) {
    Seos* seos = context;

    // Clear view
    void* validator_context = text_input_get_validator_callback_context(seos->text_input);
    text_input_set_validator(seos->text_input, NULL, NULL);
    validator_is_file_free(validator_context);

    text_input_reset(seos->text_input);
}
