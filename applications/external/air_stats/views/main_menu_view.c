/*
 * main_menu_view.c — main menu with dynamic sensor list + add + settings.
 */
#include "../air_stats_i.h"
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
    if(index == 0) {
        /* CO2 Sensor: first MHZ19C or MHZ19C_UART */
        for(uint8_t i = 0; i < app->sensors_count; i++) {
            if(app->sensors[i] &&
               (app->sensors[i]->type == &MHZ19C || app->sensors[i]->type == &MHZ19C_UART)) {
                view_sensor_actions_switch(app->sensors[i]);
                return;
            }
        }
    } else if(index == 1) {
        /* Climate Sensor: disabled in UART CO2 mode */
        if(app->settings.co2_type == CO2_TYPE_UART) return;
        for(uint8_t i = 0; i < app->sensors_count; i++) {
            if(app->sensors[i] && !(app->sensors[i]->type->datatype & UT_CO2)) {
                view_sensor_actions_switch(app->sensors[i]);
                return;
            }
        }
    } else if(index == 2) {
        view_settings_switch();
    } else if(index == 3) {
        view_widget_about_switch();
    }
}

/* Rebuild list every time the view becomes active (called by ViewDispatcher) */
static void _view_enter(void* context) {
    UNUSED(context);
    variable_item_list_reset(variable_item_list);
    variable_item_list_add(variable_item_list, "CO2 Sensor", 1, NULL, NULL);
    VariableItem* climate_item =
        variable_item_list_add(variable_item_list, "Climate Sensor", 1, NULL, NULL);
    if(app->settings.co2_type == CO2_TYPE_UART) {
        variable_item_set_current_value_text(climate_item, "N/A (UART)");
    }
    variable_item_list_add(variable_item_list, "Settings", 1, NULL, NULL);
    variable_item_list_add(variable_item_list, "About", 1, NULL, NULL);
    variable_item_list_set_selected_item(variable_item_list, 0);
}

void view_main_menu_alloc(void) {
    variable_item_list = variable_item_list_alloc();
    variable_item_list_set_enter_callback(variable_item_list, _enter_callback, app);
    view = variable_item_list_get_view(variable_item_list);
    view_set_previous_callback(view, _exit_callback);
    view_set_enter_callback(view, _view_enter);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_ID, view);
}

void view_main_menu_switch(void) {
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID);
}

void view_main_menu_free(void) {
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID);
    variable_item_list_free(variable_item_list);
}
