#include "../ibutton_converter_i.h"

#include <toolbox/name_generator.h>
#include <toolbox/path.h>

static void ibutton_converter_scene_save_name_text_input_callback(void* context) {
    iButtonConverter* ibutton_converter = context;
    view_dispatcher_send_custom_event(
        ibutton_converter->view_dispatcher, iButtonConverterCustomEventTextEditResult);
}

void ibutton_converter_scene_save_name_on_enter(void* context) {
    iButtonConverter* ibutton_converter = context;
    TextInput* text_input = ibutton_converter->text_input;

    name_generator_make_auto(
        ibutton_converter->key_name, IBUTTON_KEY_NAME_SIZE, IBUTTON_APP_FILENAME_PREFIX);

    text_input_set_header_text(text_input, "Name the key");
    text_input_set_result_callback(
        text_input,
        ibutton_converter_scene_save_name_text_input_callback,
        ibutton_converter,
        ibutton_converter->key_name,
        IBUTTON_KEY_NAME_SIZE,
        true);

    ValidatorIsFile* validator_is_file = validator_is_file_alloc_init(
        IBUTTON_APP_FOLDER, IBUTTON_APP_FILENAME_EXTENSION, ibutton_converter->key_name);
    text_input_set_validator(text_input, validator_is_file_callback, validator_is_file);

    view_dispatcher_switch_to_view(
        ibutton_converter->view_dispatcher, iButtonConverterViewTextInput);
}

bool ibutton_converter_scene_save_name_on_event(void* context, SceneManagerEvent event) {
    iButtonConverter* ibutton_converter = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == iButtonConverterCustomEventTextEditResult) {
            furi_string_printf(
                ibutton_converter->file_path,
                "%s/%s%s",
                IBUTTON_APP_FOLDER,
                ibutton_converter->key_name,
                IBUTTON_APP_FILENAME_EXTENSION);

            if(ibutton_converter_save_key(ibutton_converter)) {
                scene_manager_next_scene(
                    ibutton_converter->scene_manager, iButtonConverterSceneSaveSuccess);
            } else {
                furi_crash("Error while converted file saving");
            }
        }
    }

    return consumed;
}

void ibutton_converter_scene_save_name_on_exit(void* context) {
    iButtonConverter* ibutton_converter = context;
    TextInput* text_input = ibutton_converter->text_input;

    void* validator_context = text_input_get_validator_callback_context(text_input);
    text_input_set_validator(text_input, NULL, NULL);
    validator_is_file_free((ValidatorIsFile*)validator_context);

    text_input_reset(text_input);
}
