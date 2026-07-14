#include "../../app.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#define TAG "edit_task_app"

enum EditTaskIndex {
    EditTaskIndexName,
    EditTaskIndexDescription,
    EditTaskIndexPricePerHour,
};

static void edit_task_scene_select_callback(void* context, uint32_t index) {
    App* app = context;
    furi_assert(app);
    furi_assert(app->view_dispatcher);
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void edit_task_scene_name_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    furi_assert(app);
    // Handle name editing logic here
}

static void edit_task_scene_description_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    furi_assert(app);
    // Handle description editing logic here
}

static void edit_task_scene_price_per_hour_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    furi_assert(app);
    // Handle price per hour editing logic here
}

static void draw_edit_task_menu(App* app) {
    furi_assert(app);
    furi_assert(app->variable_item_list);
    VariableItemList* variable_item_list = app->variable_item_list;

    variable_item_list_reset(variable_item_list);

    // Retrieve values from current_task
    const char* name = app->current_task->name;
    const char* description = app->current_task->description;

    char price_per_hour_str[16];
    snprintf(
        price_per_hour_str,
        sizeof(price_per_hour_str),
        "%.2f",
        (double)app->current_task->price_per_hour);

    // Add items to the variable item list and set their values
    VariableItem* item;

    item =
        variable_item_list_add(variable_item_list, "Name", 1, edit_task_scene_name_callback, app);
    variable_item_set_current_value_text(item, name);

    item = variable_item_list_add(
        variable_item_list, "Description", 1, edit_task_scene_description_callback, app);
    variable_item_set_current_value_text(item, description);

    item = variable_item_list_add(
        variable_item_list, "Price per Hour", 1, edit_task_scene_price_per_hour_callback, app);
    variable_item_set_current_value_text(item, price_per_hour_str);
}

void scene_on_enter_edit_task(void* context) {
    App* app = context;
    furi_assert(app);
    furi_assert(app->variable_item_list);
    VariableItemList* variable_item_list = app->variable_item_list;

    variable_item_list_set_enter_callback(
        variable_item_list, edit_task_scene_select_callback, app);
    draw_edit_task_menu(app);
    variable_item_list_set_selected_item(variable_item_list, 0);

    furi_assert(app->view_dispatcher);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_EditTask);
}

bool scene_on_event_edit_task(void* context, SceneManagerEvent event) {
    App* app = context;
    furi_assert(app);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case EditTaskIndexName:
            scene_manager_next_scene(app->scene_manager, TaskNameInput);
            break;
        case EditTaskIndexDescription:
            scene_manager_next_scene(app->scene_manager, TaskDescriptionInput);
            break;
        case EditTaskIndexPricePerHour:
            scene_manager_next_scene(app->scene_manager, PriceInput);
            break;
        default:
            furi_crash("Unknown key type");
            break;
        }
    }

    return consumed;
}

void scene_on_exit_edit_task(void* context) {
    App* app = context;
    furi_assert(app);
    furi_assert(app->variable_item_list);
    variable_item_list_reset(app->variable_item_list);
}
