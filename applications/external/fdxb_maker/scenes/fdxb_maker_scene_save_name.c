#include "../fdxb_maker.h"
#include <toolbox/name_generator.h>

void fdxb_maker_scene_save_name_on_enter(void* context) {
    FdxbMaker* app = context;
    FuriString* folder_path;
    folder_path = furi_string_alloc();

    bool key_name_is_empty = furi_string_empty(app->file_name);
    if(key_name_is_empty) {
        furi_string_set(app->file_path, LFRFID_APP_FOLDER);

        name_generator_make_auto(
            app->text_store, FDXB_MAKER_TEXT_STORE_SIZE, LFRFID_APP_FILENAME_PREFIX);

        furi_string_set(folder_path, LFRFID_APP_FOLDER);
    } else {
        fdxb_maker_text_store_set(app, "%s", furi_string_get_cstr(app->file_name));
        path_extract_dirname(furi_string_get_cstr(app->file_path), folder_path);
    }

    text_input_set_header_text(app->text_input, "Name the chip");
    text_input_set_result_callback(
        app->text_input,
        fdxb_maker_text_input_callback,
        app,
        app->text_store,
        FDXB_MAKER_KEY_NAME_SIZE,
        key_name_is_empty);

    FURI_LOG_I(TAG, "%s %s", furi_string_get_cstr(folder_path), app->text_store);

    ValidatorIsFile* validator_is_file = validator_is_file_alloc_init(
        furi_string_get_cstr(folder_path),
        LFRFID_APP_FILENAME_EXTENSION,
        furi_string_get_cstr(app->file_name));
    text_input_set_validator(app->text_input, validator_is_file_callback, validator_is_file);

    furi_string_free(folder_path);

    view_dispatcher_switch_to_view(app->view_dispatcher, FdxbMakerViewTextInput);
}

bool fdxb_maker_scene_save_name_on_event(void* context, SceneManagerEvent event) {
    FdxbMaker* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == FdxbMakerEventNext) {
            consumed = true;
            if(!furi_string_empty(app->file_name)) {
                fdxb_maker_delete_key(app);
            }

            furi_string_set(app->file_name, app->text_store);

            if(fdxb_maker_save_key(app)) {
                scene_manager_next_scene(app->scene_manager, FdxbMakerSceneSaveSuccess);
            } else {
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, FdxbMakerSceneSelectKey);
            }
        }
    }

    return consumed;
}

void fdxb_maker_scene_save_name_on_exit(void* context) {
    FdxbMaker* app = context;

    void* validator_context = text_input_get_validator_callback_context(app->text_input);
    text_input_set_validator(app->text_input, NULL, NULL);
    validator_is_file_free((ValidatorIsFile*)validator_context);

    text_input_reset(app->text_input);
}
