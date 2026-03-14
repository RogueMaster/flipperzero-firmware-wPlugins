/*
 * settings_view.c — VariableItemList with unit settings.
 * Adapted from _ref_unitemp/views/Settings_view.c
 */
#include "../co2_app_i.h"
#include <gui/modules/variable_item_list.h>

static View* view;
static VariableItemList* variable_item_list;

static const char states[2][9]                          = {"Auto", "Infinity"};
static const char temp_units[UT_TEMP_COUNT][3]          = {"*C", "*F"};
static const char humidity_units[UT_HUMIDITY_COUNT][12] = {"Relative", "Dewpoint"};
static const char pressure_units[UT_PRESSURE_COUNT][6]  = {"mmHg", "inHg", "kPa", "hPa"};
static const char heat_index_bool[2][4]                 = {"OFF", "ON"};

static VariableItem* infinity_backlight_item;
static VariableItem* temperature_unit_item;
static VariableItem* humidity_unit_item;
static VariableItem* pressure_unit_item;
static VariableItem* heat_index_item;

#define VIEW_ID ViewSettings

static uint32_t _exit_callback(void* context) {
    UNUSED(context);
    /* Apply backlight setting */
    if((bool)variable_item_get_current_value_index(infinity_backlight_item) !=
       app->settings.infinityBacklight) {
        if((bool)variable_item_get_current_value_index(infinity_backlight_item)) {
            notification_message(app->notifications, &sequence_display_backlight_enforce_on);
        } else {
            notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
        }
    }

    app->settings.infinityBacklight =
        (bool)variable_item_get_current_value_index(infinity_backlight_item);
    app->settings.temp_unit     = variable_item_get_current_value_index(temperature_unit_item);
    app->settings.humidity_unit = variable_item_get_current_value_index(humidity_unit_item);
    app->settings.pressure_unit = variable_item_get_current_value_index(pressure_unit_item);
    app->settings.heat_index    = variable_item_get_current_value_index(heat_index_item);
    unitemp_saveSettings();
    unitemp_loadSettings();

    return ViewMainMenu;
}

static void _enter_callback(void* context, uint32_t index) {
    UNUSED(context);
    UNUSED(index);
}

static void _setting_change_callback(VariableItem* item) {
    if(item == infinity_backlight_item) {
        variable_item_set_current_value_text(
            infinity_backlight_item,
            states[variable_item_get_current_value_index(infinity_backlight_item)]);
    }
    if(item == temperature_unit_item) {
        variable_item_set_current_value_text(
            temperature_unit_item,
            temp_units[variable_item_get_current_value_index(temperature_unit_item)]);
    }
    if(item == humidity_unit_item) {
        variable_item_set_current_value_text(
            humidity_unit_item,
            humidity_units[variable_item_get_current_value_index(humidity_unit_item)]);
    }
    if(item == pressure_unit_item) {
        variable_item_set_current_value_text(
            pressure_unit_item,
            pressure_units[variable_item_get_current_value_index(pressure_unit_item)]);
    }
    if(item == heat_index_item) {
        variable_item_set_current_value_text(
            heat_index_item,
            heat_index_bool[variable_item_get_current_value_index(heat_index_item)]);
    }
}

void view_settings_alloc(void) {
    variable_item_list = variable_item_list_alloc();
    variable_item_list_reset(variable_item_list);

    infinity_backlight_item = variable_item_list_add(
        variable_item_list, "Backlight time", 2, _setting_change_callback, app);
    temperature_unit_item = variable_item_list_add(
        variable_item_list, "Temp. unit", UT_TEMP_COUNT, _setting_change_callback, app);
    humidity_unit_item = variable_item_list_add(
        variable_item_list, "Humidity unit", UT_HUMIDITY_COUNT, _setting_change_callback, app);
    pressure_unit_item = variable_item_list_add(
        variable_item_list, "Press. unit", UT_PRESSURE_COUNT, _setting_change_callback, app);
    heat_index_item = variable_item_list_add(
        variable_item_list, "Calc. heat index", 2, _setting_change_callback, app);

    variable_item_list_set_enter_callback(variable_item_list, _enter_callback, app);

    view = variable_item_list_get_view(variable_item_list);
    view_set_previous_callback(view, _exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_ID, view);
}

void view_settings_switch(void) {
    variable_item_list_set_selected_item(variable_item_list, 0);

    variable_item_set_current_value_index(
        infinity_backlight_item, (uint8_t)app->settings.infinityBacklight);
    variable_item_set_current_value_text(
        infinity_backlight_item,
        states[variable_item_get_current_value_index(infinity_backlight_item)]);

    variable_item_set_current_value_index(temperature_unit_item, (uint8_t)app->settings.temp_unit);
    variable_item_set_current_value_text(
        temperature_unit_item,
        temp_units[variable_item_get_current_value_index(temperature_unit_item)]);

    variable_item_set_current_value_index(humidity_unit_item, app->settings.humidity_unit);
    variable_item_set_current_value_text(
        humidity_unit_item,
        humidity_units[variable_item_get_current_value_index(humidity_unit_item)]);

    variable_item_set_current_value_index(
        pressure_unit_item, (uint8_t)app->settings.pressure_unit);
    variable_item_set_current_value_text(
        pressure_unit_item,
        pressure_units[variable_item_get_current_value_index(pressure_unit_item)]);

    variable_item_set_current_value_index(heat_index_item, (uint8_t)app->settings.heat_index);
    variable_item_set_current_value_text(
        heat_index_item, heat_index_bool[variable_item_get_current_value_index(heat_index_item)]);

    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID);
}

void view_settings_free(void) {
    variable_item_list_free(variable_item_list);
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID);
}
