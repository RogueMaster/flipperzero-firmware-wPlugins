#include "../nfc_magic_app_i.h"
#include "furi.h"
#include <toolbox/name_generator.h>

void nfc_magic_scene_gen1_save_name_text_input_done_callback(void* context) {
    NfcMagicApp* instance = context;

    view_dispatcher_send_custom_event(instance->view_dispatcher, NfcMagicCustomEventTextInputDone);
}

void nfc_magic_scene_gen1_save_name_on_enter(void* context) {
    NfcMagicApp* instance = context;

    FuriString* folder_path = furi_string_alloc();
    TextInput* text_input = instance->text_input;

    furi_string_set(instance->file_path, NFC_APP_FOLDER);
    name_generator_make_auto(
        instance->text_store, NFC_MAGIC_APP_TEXT_STORE_SIZE, NFC_MAGIC_APP_FILENAME_PREFIX);
    furi_string_set(folder_path, NFC_APP_FOLDER);

    text_input_set_header_text(text_input, "Name the card");
    text_input_set_result_callback(
        text_input,
        nfc_magic_scene_gen1_save_name_text_input_done_callback,
        instance,
        instance->text_store,
        NFC_MAGIC_APP_NAME_SIZE,
        false);

    ValidatorIsFile* validator_is_file = validator_is_file_alloc_init(
        furi_string_get_cstr(folder_path),
        NFC_APP_EXTENSION,
        furi_string_get_cstr(instance->file_name));
    text_input_set_validator(text_input, validator_is_file_callback, validator_is_file);

    furi_string_free(folder_path);

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewTextInput);
}

bool nfc_magic_scene_gen1_save_name_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcMagicCustomEventTextInputDone) {
            furi_string_set(instance->file_name, instance->text_store);

            furi_string_cat_printf(
                instance->file_path,
                "/%s%s",
                furi_string_get_cstr(instance->file_name),
                NFC_APP_EXTENSION);

            if(nfc_device_save(instance->source_dev, furi_string_get_cstr(instance->file_path))) {
                scene_manager_next_scene(instance->scene_manager, NfcMagicSceneSuccess);
                dolphin_deed(DolphinDeedNfcSave);
            } else {
                consumed = scene_manager_search_and_switch_to_previous_scene(
                    instance->scene_manager, NfcMagicSceneStart);
            }
        }
    }

    return consumed;
}

void nfc_magic_scene_gen1_save_name_on_exit(void* context) {
    NfcMagicApp* instance = context;

    void* validator_context = text_input_get_validator_callback_context(instance->text_input);
    text_input_set_validator(instance->text_input, NULL, NULL);
    validator_is_file_free(validator_context);

    text_input_reset(instance->text_input);
}
