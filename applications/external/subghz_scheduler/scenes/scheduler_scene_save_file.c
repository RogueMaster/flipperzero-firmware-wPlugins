#include "helpers/scheduler_custom_file_types.h"
#include "helpers/scheduler_popup.h"
#include "helpers/scheduler_settings_io.h"
#include "src/scheduler_app_i.h"

#include <gui/modules/validators.h>

#define TAG "SchedulerSaveSettings"

static void scheduler_scene_save_name_text_input_callback(void* context) {
    furi_assert(context);
    SchedulerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SchedulerStartEventSaveSchedule);
}

void scheduler_scene_save_name_on_enter(void* context) {
    furi_assert(context);
    SchedulerApp* app = context;

    TextInput* text_input = app->text_input;

    text_input_set_header_text(text_input, "Save schedule as");

    // Provide a default name if empty
    if(app->save_name_tmp[0] == '\0') {
        strncpy(app->save_name_tmp, "schedule", sizeof(app->save_name_tmp) - 1);
    }

    text_input_set_result_callback(
        text_input,
        scheduler_scene_save_name_text_input_callback,
        app,
        app->save_name_tmp,
        sizeof(app->save_name_tmp),
        false);

    ValidatorIsFile* validator_is_file = validator_is_file_alloc_init(
        furi_string_get_cstr(app->save_dir), SCHEDULER_FILE_EXTENSION, "");

    text_input_set_validator(text_input, validator_is_file_callback, validator_is_file);

    view_dispatcher_switch_to_view(app->view_dispatcher, SchedulerAppViewTextInput);
}

bool scheduler_scene_save_name_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    SchedulerApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SchedulerStartEventSaveSchedule) {
            if(app->save_name_tmp[0] == '\0') {
                // TODO: error message?
                return true;
            }

            furi_string_set(app->save_dir, SCHEDULER_APP_FOLDER);
            furi_string_cat_printf(
                app->save_dir, "/%s%s", app->save_name_tmp, SCHEDULER_SETTINGS_FILE_EXTENSION);

            bool ok = scheduler_settings_save_to_path(app, furi_string_get_cstr(app->save_dir));
            if(!ok) {
                dialog_message_show_storage_error(app->dialogs, "Failed to save settings!");
            } else {
                const char* full = furi_string_get_cstr(app->save_dir);
                snprintf(
                    app->popup_msg,
                    sizeof(app->popup_msg),
                    "File saved as:\n%s",
                    path_basename(full));
                scheduler_popup_success_show(app, app->popup_msg);
            }
            return true;
        } else if(event.event == SchedulerEventPopupDone) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, SchedulerSceneStart);
            return true;
        }
    }

    return false;
}

void scheduler_scene_save_name_on_exit(void* context) {
    furi_assert(context);
    SchedulerApp* app = context;

    void* validator_context = text_input_get_validator_callback_context(app->text_input);
    text_input_set_validator(app->text_input, NULL, NULL);
    if(validator_context) validator_is_file_free(validator_context);

    text_input_reset(app->text_input);
}
