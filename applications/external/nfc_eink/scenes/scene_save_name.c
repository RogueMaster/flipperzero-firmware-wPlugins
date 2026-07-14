#include "../nfc_eink_app_i.h"
#include "../nfc_eink_screen/nfc_eink_screen.h"
#include <path.h>

static void nfc_eink_text_input_callback(void* context) {
    NfcEinkApp* instance = context;
    view_dispatcher_send_custom_event(
        instance->view_dispatcher, NfcEinkAppCustomEventTextInputDone);
}

void nfc_eink_scene_save_name_on_enter(void* context) {
    NfcEinkApp* instance = context;
    TextInput* text_input = instance->text_input;
    FuriString* folder_path = furi_string_alloc();

    bool name_is_empty = furi_string_empty(instance->file_name);
    if(name_is_empty) {
        furi_string_set(folder_path, NFC_EINK_APP_FOLDER);
        furi_string_set(instance->file_path, NFC_EINK_APP_FOLDER);
    } else {
        path_extract_dirname(furi_string_get_cstr(instance->file_path), folder_path);
    }

    text_input_set_header_text(text_input, "Name the screen");
    text_input_set_result_callback(
        text_input,
        nfc_eink_text_input_callback,
        instance,
        instance->text_store,
        NFC_EINK_NAME_SIZE,
        name_is_empty);

    ValidatorIsFile* validator_is_file = validator_is_file_alloc_init(
        furi_string_get_cstr(folder_path),
        NFC_EINK_APP_EXTENSION,
        furi_string_get_cstr(instance->file_name));
    text_input_set_validator(text_input, validator_is_file_callback, validator_is_file);

    furi_string_free(folder_path);

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcEinkViewTextInput);
}

bool nfc_eink_scene_save_name_on_event(void* context, SceneManagerEvent event) {
    NfcEinkApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcEinkAppCustomEventTextInputDone) {
            if(!furi_string_empty(instance->file_name)) {
                nfc_eink_screen_delete(furi_string_get_cstr(instance->file_path));
                furi_string_set(instance->file_path, NFC_EINK_APP_FOLDER);
            }

            strlcat(instance->text_store, NFC_EINK_APP_EXTENSION, sizeof(instance->text_store));
            path_append(instance->file_path, instance->text_store);

            if(nfc_eink_screen_save(instance->screen, furi_string_get_cstr(instance->file_path))) {
                scene_manager_next_scene(instance->scene_manager, NfcEinkAppSceneSaveSuccess);
                dolphin_deed(DolphinDeedNfcSave);
                memset(instance->text_store, 0, sizeof(instance->text_store));
            }
            consumed = true;
        }
    }

    return consumed;
}

void nfc_eink_scene_save_name_on_exit(void* context) {
    UNUSED(context);
}
