#include "../../app.h"
#include "../../csv/csv.h"
#include "../../datetime/datetime.h"
#include "../../structs.h"
#include "../../tasks/task.h"
#include <datetime/datetime.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/modules/submenu.h>

#define TAG "tracker_app_view_task"

void submenu_callback_task_actions(void* context, uint32_t index) {
    FURI_LOG_T(TAG, "submenu_callback_task_actions");
    furi_assert(context);
    App* app = context;
    DateTime current_time = {0};
    DateTime datetime_start_time = app->current_task->last_start_time;

    // Ensure app and its members are properly initialized
    if(!app || !app->current_task) {
        FURI_LOG_E(TAG, "Invalid app or uninitialized current_task");
        return;
    }

    switch(index) {
    case TaskAction_Continue:
        // Handle "Continue" action
        scene_manager_handle_custom_event(app->scene_manager, AppEvent_TaskContinue);
        // Add your logic here
        break;
    case TaskAction_Edit:
        // Handle "Edit" action
        scene_manager_handle_custom_event(app->scene_manager, AppEvent_EditTask);
        break;
    case TaskAction_Stats:
        // Handle "Stats" action
        scene_manager_handle_custom_event(app->scene_manager, AppEvent_ViewStats);
        // Add your logic here
        break;
    case TaskAction_ToggleCompleted:

        furi_hal_rtc_get_datetime(&current_time);

        // Handle "Toggle Completed" action
        app->current_task->completed = !app->current_task->completed;

        if(app->current_task->status == TaskStatus_Running) {
            app->current_task->status = TaskStatus_Stopped;
            app->current_task->end_time = current_time;

            int32_t time_difference_minutes =
                calculate_time_difference_in_minutes(&datetime_start_time, &current_time);
            app->current_task->total_time_minutes += time_difference_minutes;
        }
        // Update the submenu to reflect the new state
        submenu_change_item_label(
            app->submenu_task_actions,
            TaskAction_ToggleCompleted,
            app->current_task->completed ? "Unmark as done" : "Mark as done");

        // Update the task in the CSV file
        if(tasks_update(app, app->current_task)) {
            find_and_replace_task_in_csv(app, app->current_task);
        }

        break;
    case TaskAction_Delete:
        // Handle "Delete" action
        if(tasks_remove(app, app->current_task)) {
            delete_task_from_csv(app, app->current_task->id);
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, ViewTasks);
        };

        break;
    default:
        break;
    }
}

void scene_on_enter_task_actions(void* context) {
    FURI_LOG_T(TAG, "scene_on_enter_task_actions");
    App* app = context;

    // Ensure app and its members are properly initialized
    if(!app || !app->current_task) {
        FURI_LOG_E(TAG, "Invalid app or uninitialized current_task");
        return;
    }

    submenu_reset(app->submenu_task_actions);

    // Set the header to the current task name
    Task* task = app->current_task;
    char header[128];

    snprintf(header, sizeof(header), "Task: %s", task->name);
    submenu_set_header(app->submenu_task_actions, header);
    if(task->status == TaskStatus_Running) {
        submenu_add_item(
            app->submenu_task_actions,
            "Stop",
            TaskAction_Continue,
            submenu_callback_task_actions,
            app);
    } else {
        submenu_add_item(
            app->submenu_task_actions,
            "Start",
            TaskAction_Continue,
            submenu_callback_task_actions,
            app);
    }
    submenu_add_item(
        app->submenu_task_actions, "Edit", TaskAction_Edit, submenu_callback_task_actions, app);
    submenu_add_item(
        app->submenu_task_actions, "Stats", TaskAction_Stats, submenu_callback_task_actions, app);

    // Add the "Mark as done" or "Unmark as done" item based on the completed flag
    if(task->completed) {
        submenu_add_item(
            app->submenu_task_actions,
            "Unmark as done",
            TaskAction_ToggleCompleted,
            submenu_callback_task_actions,
            app);
    } else {
        submenu_add_item(
            app->submenu_task_actions,
            "Mark as done",
            TaskAction_ToggleCompleted,
            submenu_callback_task_actions,
            app);
    }

    submenu_add_item(
        app->submenu_task_actions,
        "Delete !",
        TaskAction_Delete,
        submenu_callback_task_actions,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_TaskActions);
}

bool scene_on_event_task_actions(void* context, SceneManagerEvent event) {
    FURI_LOG_T(TAG, "scene_on_event_task_actions");
    App* app = context;
    bool consumed = false;

    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case AppEvent_TaskContinue:
            scene_manager_next_scene(app->scene_manager, TaskContinue);
            consumed = true;
            break;
        case AppEvent_EditTask:
            scene_manager_next_scene(app->scene_manager, EditTask);
            consumed = true;
            break;
        case AppEvent_ViewStats:
            scene_manager_next_scene(app->scene_manager, ViewStats);
            consumed = true;
            break;
        }
        break;

    default: // eg. SceneManagerEventTypeBack, SceneManagerEventTypeTick
        consumed = false;
        break;
    }

    return consumed;
}

void scene_on_exit_task_actions(void* context) {
    FURI_LOG_T(TAG, "scene_on_exit_task_actions");
    App* app = context;
    submenu_reset(app->submenu_task_actions);
}
