#include "../../app.h"
#include "../../csv/csv.h"
#include "../../structs.h"
#include "../../tasks/task.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/elements.h>
#include <gui/modules/text_input.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#define TAG "edit_task_name"

void text_input_callback(void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, AppView_TaskNameInput);
}

bool validator_is_text_callback(const char* text, FuriString* error, void* context) {
    furi_check(context);

    // Check if the text is empty
    if(text == NULL || strlen(text) == 0) {
        furi_string_set_str(error, "Input cannot be empty.");
        return false;
    }

    // Check if the text length exceeds the size of text_store
    if(strlen(text) > KEY_NAME_SIZE) {
        furi_string_set_str(error, "Input is too long.");
        return false;
    }

    // If all checks pass, return true
    return true;
}

void scene_on_enter_task_name_input(void* context) {
    App* app = context;
    TextInput* text_input = app->text_input;

    FuriString* task_name_string = furi_string_alloc();
    furi_string_set_str(task_name_string, app->current_task->name);

    // Copy the task name to text_store
    strncpy(app->text_store, furi_string_get_cstr(task_name_string), KEY_NAME_SIZE);
    app->text_store[KEY_NAME_SIZE] = '\0'; // Ensure null-termination

    const char* key_name = furi_string_get_cstr(task_name_string);
    text_input_set_header_text(text_input, "Edit Task Name");

    text_input_set_result_callback(
        text_input, text_input_callback, app, app->text_store, KEY_NAME_SIZE, key_name);

    text_input_set_validator(text_input, validator_is_text_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_TaskNameInput);

    furi_string_free(task_name_string);
}
bool scene_on_event_task_name_input(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    // Log the event
    FURI_LOG_T(TAG, "scene_on_event_task_name_input");
    // Check if the event is of type SceneManagerEventTypeCustom

    // Log event
    FURI_LOG_T(
        TAG,
        "event type: %ld, event: %ld, %d",
        (long)event.type,
        (long)event.event,
        AppEvent_TaskNameInput);

    if(event.event == AppEvent_TaskNameInput) {
        consumed = true;
        FURI_LOG_T(TAG, "Saving input");

        // Get the new task name
        const char* new_task_name = app->text_store;

        // Copy the new task name to the current task

        strncpy(app->current_task->name, new_task_name, sizeof(app->current_task->name) - 1);
        app->current_task->name[sizeof(app->current_task->name) - 1] =
            '\0'; // Ensure null-termination

        // Log new name

        FURI_LOG_T(TAG, "New task name: %s", app->current_task->name);

        if(tasks_update(app, app->current_task)) {
            find_and_replace_task_in_csv(app, app->current_task);
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, EditTask);
        }
    }

    return consumed;
}

void scene_on_exit_task_name_input(void* context) {
    App* app = context;
    TextInput* text_input = app->text_input;

    text_input_reset(text_input);
    text_input_set_validator(text_input, NULL, NULL);
    app->text_store[0] = '\0';
}
