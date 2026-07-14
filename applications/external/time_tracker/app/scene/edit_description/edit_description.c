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

#define TAG "edit_task_description"

void description_input_callback(void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, AppEvent_TaskDescriptionInput);
}

bool validator_is_description_callback(const char* text, FuriString* error, void* context) {
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

void scene_on_enter_task_description_input(void* context) {
    App* app = context;
    TextInput* text_input = app->text_input;

    FuriString* task_description_string = furi_string_alloc();
    furi_string_set_str(task_description_string, app->current_task->description);

    // Copy the task description to text_store
    strncpy(app->text_store, furi_string_get_cstr(task_description_string), KEY_NAME_SIZE);
    app->text_store[KEY_NAME_SIZE] = '\0'; // Ensure null-termination

    const char* description = furi_string_get_cstr(task_description_string);
    text_input_set_header_text(text_input, "Edit Task Description");

    text_input_set_result_callback(
        text_input, description_input_callback, app, app->text_store, KEY_NAME_SIZE, description);

    text_input_set_validator(text_input, validator_is_description_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_TaskDescriptionInput);

    furi_string_free(task_description_string);
}

bool scene_on_event_task_description_input(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    // Log the event
    FURI_LOG_T(TAG, "scene_on_event_task_description_input");
    // Check if the event is of type SceneManagerEventTypeCustom

    // Log event and AppEvent_TaskDescriptionInput

    FURI_LOG_T(
        TAG,
        "event type: %ld, event: %ld, %d",
        (long)event.type,
        (long)event.event,
        AppEvent_TaskDescriptionInput);

    if(event.event == AppEvent_TaskDescriptionInput) {
        consumed = true;
        FURI_LOG_T(TAG, "Saving input");

        // Get the new task description
        const char* new_task_description = app->text_store;

        // Copy the new task description to the current task
        strncpy(
            app->current_task->description,
            new_task_description,
            sizeof(app->current_task->description) - 1);
        app->current_task->description[sizeof(app->current_task->description) - 1] =
            '\0'; // Ensure null-termination

        if(tasks_update(app, app->current_task)) {
            find_and_replace_task_in_csv(app, app->current_task);
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, EditTask);
        }
    }

    return consumed;
}

void scene_on_exit_task_description_input(void* context) {
    App* app = context;
    TextInput* text_input = app->text_input;

    text_input_reset(text_input);
    text_input_set_validator(text_input, NULL, NULL);
    app->text_store[0] = '\0';
}
