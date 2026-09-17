#include "../dfc_i.h"
#include "../dfc_credential_storage.h"
#include <lib/toolbox/name_generator.h>
#include <gui/modules/validators.h>
#include <toolbox/path.h>

#define DFC_APP_FILE_PREFIX "DFC"

void dfc_scene_save_name_text_input_callback(void* context) {
    Dfc* dfc = context;

    view_dispatcher_send_custom_event(dfc->view_dispatcher, DfcCustomEventTextInputDone);
}

void dfc_scene_save_name_on_enter(void* context) {
    Dfc* dfc = context;

    // Setup view
    TextInput* text_input = dfc->text_input;
    bool dev_name_empty = false;
    if(!strcmp(dfc->credential->name, "")) {
        name_generator_make_auto(dfc->text_store, sizeof(dfc->text_store), DFC_APP_FILE_PREFIX);
        dev_name_empty = true;
    } else {
        dfc_text_store_set(dfc, dfc->credential->name);
    }
    text_input_set_header_text(text_input, "Name the card");
    text_input_set_result_callback(
        text_input,
        dfc_scene_save_name_text_input_callback,
        dfc,
        dfc->text_store,
        sizeof(dfc->text_store),
        dev_name_empty);

    FuriString* folder_path;
    folder_path = furi_string_alloc_set(STORAGE_APP_DATA_PATH_PREFIX);

    if(furi_string_end_with(dfc_credential_storage_load_path(), DFC_APP_EXTENSION) ||
       furi_string_end_with(dfc_credential_storage_load_path(), DFC_BINARY_EXTENSION)) {
        path_extract_dirname(
            furi_string_get_cstr(dfc_credential_storage_load_path()), folder_path);
    }

    ValidatorIsFile* validator_is_file = validator_is_file_alloc_init(
        furi_string_get_cstr(folder_path), DFC_APP_EXTENSION, dfc->credential->name);
    text_input_set_validator(text_input, validator_is_file_callback, validator_is_file);

    view_dispatcher_switch_to_view(dfc->view_dispatcher, DfcViewTextInput);

    furi_string_free(folder_path);
}

bool dfc_scene_save_name_on_event(void* context, SceneManagerEvent event) {
    Dfc* dfc = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DfcCustomEventTextInputDone) {
            strlcpy(dfc->credential->name, dfc->text_store, strlen(dfc->text_store) + 1);
            if(dfc_credential_save(dfc->credential, dfc->text_store)) {
                scene_manager_next_scene(dfc->scene_manager, DfcSceneSaveSuccess);
                consumed = true;
            } else {
                consumed = scene_manager_search_and_switch_to_previous_scene(
                    dfc->scene_manager, DfcSceneMainMenu);
            }
        }
    }
    return consumed;
}

void dfc_scene_save_name_on_exit(void* context) {
    Dfc* dfc = context;

    // Clear view
    void* validator_context = text_input_get_validator_callback_context(dfc->text_input);
    text_input_set_validator(dfc->text_input, NULL, NULL);
    validator_is_file_free(validator_context);

    text_input_reset(dfc->text_input);
}
