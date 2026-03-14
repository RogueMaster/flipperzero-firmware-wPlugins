/*
 * main_menu_view.c — main menu with dynamic sensor list + add + settings.
 */
#include "../co2_app_i.h"
#include <gui/modules/variable_item_list.h>

static View* view;
static VariableItemList* variable_item_list;

#define VIEW_ID ViewMainMenu

static uint32_t _exit_callback(void* context) {
    UNUSED(context);
    return ViewMain;
}

static void _enter_callback(void* context, uint32_t index) {
    UNUSED(context);
    /* First sensors_count items → SensorActions for that sensor */
    if(index < app->sensors_count) {
        if(app->sensors[index]) {
            view_sensor_actions_switch(app->sensors[index]);
        }
        return;
    }
    uint32_t extra = index - app->sensors_count;
    if(extra == 0) {
        view_sensors_list_switch();   /* Add sensor */
    } else if(extra == 1) {
        view_settings_switch();       /* Settings */
    }
}

void view_main_menu_alloc(void) {
    variable_item_list = variable_item_list_alloc();
    variable_item_list_set_enter_callback(variable_item_list, _enter_callback, app);
    view = variable_item_list_get_view(variable_item_list);
    view_set_previous_callback(view, _exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_ID, view);
}

void view_main_menu_switch(void) {
    /* Rebuild list with current active sensors each time */
    variable_item_list_reset(variable_item_list);
    for(uint8_t i = 0; i < app->sensors_count; i++) {
        if(app->sensors[i]) {
            variable_item_list_add(
                variable_item_list, app->sensors[i]->name, 1, NULL, NULL);
        }
    }
    variable_item_list_add(variable_item_list, "Add sensor", 1, NULL, NULL);
    variable_item_list_add(variable_item_list, "Settings", 1, NULL, NULL);

    variable_item_list_set_selected_item(variable_item_list, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID);
}

void view_main_menu_free(void) {
    variable_item_list_free(variable_item_list);
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID);
}
