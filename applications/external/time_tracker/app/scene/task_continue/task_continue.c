#include "../../app.h"
#include "../../csv/csv.h"
#include "../../datetime/datetime.h"
#include "../../tasks/task.h"
#include <core/timer.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/modules/dialog_ex.h>
#include <gui/view_dispatcher.h>
#include <locale/locale.h>
#include <time_tracker_icons.h>

#define TAG "tracker_app"

static void task_continue_scene_dialog_callback(DialogExResult result, void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

static void task_continue_update(App* app) {
    DialogEx* dialog_ex = app->dialog;
    char buffer_text[256];
    char total_time_formatted[64];
    DateTime current_time = {0};
    furi_hal_rtc_get_datetime(&current_time);
    int32_t elapsed_seconds =
        calculate_time_difference_in_seconds(&app->current_task->last_start_time, &current_time);
    int32_t elapsed_minutes = elapsed_seconds / 60;
    int32_t elapsed_remaining_seconds = elapsed_seconds % 60;

    switch(app->current_task->status) {
    case TaskStatus_Running:
        if(app->current_task->total_time_minutes > 0) {
            format_time_string(
                total_time_formatted,
                sizeof(total_time_formatted),
                app->current_task->total_time_minutes + elapsed_minutes);
            snprintf(
                buffer_text,
                sizeof(buffer_text),
                "%ld min %ld sec\n%s total",
                (long)elapsed_minutes,
                (long)elapsed_remaining_seconds,
                total_time_formatted);
            dialog_ex_set_icon(dialog_ex, 1, 1, &I_dolphinWait_59x54);
        } else {
            snprintf(
                buffer_text,
                sizeof(buffer_text),
                "%ld min %ld sec",
                (long)elapsed_minutes,
                (long)elapsed_remaining_seconds);
            dialog_ex_set_icon(dialog_ex, 1, 1, &I_dolphinWait_59x54);
        }
        dialog_ex_set_center_button_text(dialog_ex, "Stop");
        break;

    case TaskStatus_Stopped:
        if(app->current_task->completed) {
            format_time_string(
                total_time_formatted,
                sizeof(total_time_formatted),
                app->current_task->total_time_minutes);
            snprintf(buffer_text, sizeof(buffer_text), "\n\n\n\n%s total", total_time_formatted);
            dialog_ex_set_icon(dialog_ex, -40, 1, &I_dolphinMafia_119x62);
        } else {
            if(app->current_task->total_time_minutes == 0) {
                snprintf(buffer_text, sizeof(buffer_text), "Start the task !");
                dialog_ex_set_icon(dialog_ex, 1, 1, &I_DolphinReadingSuccess_59x63);
            } else {
                format_time_string(
                    total_time_formatted,
                    sizeof(total_time_formatted),
                    app->current_task->total_time_minutes);
                snprintf(buffer_text, sizeof(buffer_text), "Stopped at\n%s", total_time_formatted);
                dialog_ex_set_icon(dialog_ex, -20, 1, &I_DolphinDone_80x58);
            }
            dialog_ex_set_center_button_text(dialog_ex, "Start");
        }
        break;
    }

    dialog_ex_set_text(dialog_ex, buffer_text, 64, 22, AlignLeft, AlignCenter);
    dialog_ex_set_left_button_text(dialog_ex, "Exit");
    dialog_ex_set_result_callback(dialog_ex, task_continue_scene_dialog_callback);
    dialog_ex_set_context(dialog_ex, app);
}

void task_continue_timer_callback(void* context) {
    App* app = context;
    if(app->current_task->status == TaskStatus_Running) {
        task_continue_update(app);
    }
}

void scene_on_enter_task_continue(void* context) {
    FURI_LOG_T(TAG, "scene_on_enter_task_actions");
    App* app = context;

    task_continue_update(app);
    furi_timer_start(app->timer, 1000);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_TaskContinue);
}

bool scene_on_event_task_continue(void* context, SceneManagerEvent event) {
    App* app = context;
    DialogEx* dialog_ex = app->dialog;
    bool consumed = false;
    Task* task_copy = (Task*)malloc(sizeof(Task));

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case DialogExResultCenter: {
            // Allocate memory for a copy of current_task
            if(task_copy == NULL) {
                FURI_LOG_E(TAG, "Failed to allocate memory for task_copy");
                return false;
            }

            // Copy the contents of current_task to task_copy
            memcpy(task_copy, app->current_task, sizeof(Task));

            // Task is running -> stop it
            if(task_copy->status == TaskStatus_Running) {
                // Change status to stopped
                task_copy->status = TaskStatus_Stopped;

                // Get current date and time, update the end time
                DateTime datetime_end_time = {0};
                furi_hal_rtc_get_datetime(&datetime_end_time);
                task_copy->end_time = datetime_end_time;

                // Calculate the time difference
                DateTime datetime_start_time = task_copy->last_start_time;

                int32_t time_difference_minutes =
                    calculate_time_difference_in_minutes(&datetime_start_time, &datetime_end_time);
                task_copy->total_time_minutes += time_difference_minutes;

                // Update UI
                dialog_ex_set_center_button_text(dialog_ex, "Start");

            } else {
                // Task is stopped -> start it
                task_copy->status = TaskStatus_Running;

                // Get current date and time, update the last start time
                DateTime datetime_start_time = {0};
                furi_hal_rtc_get_datetime(&datetime_start_time);
                task_copy->last_start_time = datetime_start_time;

                // Update UI
                dialog_ex_set_center_button_text(dialog_ex, "Stop");
            }

            // Update the original current_task with the modified copy
            memcpy(app->current_task, task_copy, sizeof(Task));
            // Find and update the task in Tasks array
            if(tasks_update(app, task_copy)) {
                // Write the updated task to the CSV file
                find_and_replace_task_in_csv(app, task_copy);
            }

            // Free the allocated memory for task_copy
            free(task_copy);
            // Update the scene
            task_continue_update(app);
            consumed = true;
            break;
        }
        case DialogExResultLeft:
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, TaskActions);
            consumed = true;
            break;
        case DialogExResultRight:
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, TaskActions);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void scene_on_exit_task_continue(void* context) {
    FURI_LOG_T(TAG, "scene_on_exit_task_continue");
    App* app = context;
    dialog_ex_reset(app->dialog);

    // Stop and free the timer
    furi_timer_stop(app->timer);
}
