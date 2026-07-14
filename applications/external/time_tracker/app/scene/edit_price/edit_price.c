#include "../../app.h"
#include "../../csv/csv.h"
#include "../../structs.h"
#include "../../tasks/task.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/elements.h>
#include <gui/modules/number_input.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#define TAG "edit_task_price"

void number_input_callback(void* context, int32_t number) {
    App* app = context;
    app->current_task->price_per_hour = (float)number;
    view_dispatcher_send_custom_event(app->view_dispatcher, AppEvent_PriceInput);
}

void scene_on_enter_price_input(void* context) {
    App* app = context;
    NumberInput* number_input = app->number_input;

    char str[50];
    snprintf(str, sizeof(str), "Set Price (0 - 1000)");

    number_input_set_header_text(number_input, str);
    number_input_set_result_callback(
        number_input,
        number_input_callback,
        context,
        (int32_t)app->current_task->price_per_hour,
        0,
        1000);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_NumberInput);
}

bool scene_on_event_price_input(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == AppEvent_PriceInput) {
        consumed = true;
        FURI_LOG_T(TAG, "Saving input");

        if(tasks_update(app, app->current_task)) {
            find_and_replace_task_in_csv(app, app->current_task);
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, EditTask);
        }
    }

    return consumed;
}

void scene_on_exit_price_input(void* context) {
    UNUSED(context);
}
