#include "../metroflip_i.h"

enum TextInputResult {
    TextInputResultOk,
};

static void metroflip_scene_save_text_input_callback(void* context) {
    Metroflip* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, TextInputResultOk);
}

void metroflip_scene_save_on_enter(void* context) {
    Metroflip* app = context;
    TextInput* text_input = app->text_input;

    text_input_set_header_text(text_input, "Save the NFC tag:");

    text_input_set_result_callback(
        text_input,
        metroflip_scene_save_text_input_callback,
        app,
        app->save_buf,
        sizeof(app->save_buf),
        true);

    ValidatorIsFile* validator_is_file =
        validator_is_file_alloc_init(APP_DATA_PATH(), METROFLIP_FILE_EXTENSION, NULL);
    text_input_set_validator(text_input, validator_is_file_callback, validator_is_file);

    view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewTextInput);
}

bool metroflip_scene_save_on_event(void* context, SceneManagerEvent event) {
    Metroflip* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case TextInputResultOk:
            scene_manager_next_scene(app->scene_manager, MetroflipSceneSaveResult);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void metroflip_scene_save_on_exit(void* context) {
    Metroflip* app = context;
    text_input_reset(app->text_input);
}
