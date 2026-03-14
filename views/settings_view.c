/*
 * settings_view.c — backlight setting only.
 * Unit settings moved to climate_settings_view.c.
 */
#include "../air_stats_i.h"
#include <gui/modules/variable_item_list.h>

static View* view;
static VariableItemList* variable_item_list;

static const char backlight_states[2][9] = {"Auto", "Infinity"};
static VariableItem* infinity_backlight_item;

#define VIEW_ID ViewSettings

static uint32_t _exit_callback(void* context) {
    UNUSED(context);
    bool new_val = (bool)variable_item_get_current_value_index(infinity_backlight_item);
    if(new_val != app->settings.infinityBacklight) {
        if(new_val) {
            notification_message(app->notifications, &sequence_display_backlight_enforce_on);
        } else {
            notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
        }
    }
    app->settings.infinityBacklight = new_val;
    unitemp_saveSettings();
    unitemp_loadSettings();
    return ViewMainMenu;
}

static void _backlight_change(VariableItem* item) {
    variable_item_set_current_value_text(
        item, backlight_states[variable_item_get_current_value_index(item)]);
}

void view_settings_alloc(void) {
    variable_item_list = variable_item_list_alloc();
    variable_item_list_reset(variable_item_list);

    infinity_backlight_item = variable_item_list_add(
        variable_item_list, "Backlight time", 2, _backlight_change, app);

    view = variable_item_list_get_view(variable_item_list);
    view_set_previous_callback(view, _exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_ID, view);
}

void view_settings_switch(void) {
    variable_item_set_current_value_index(
        infinity_backlight_item, (uint8_t)app->settings.infinityBacklight);
    variable_item_set_current_value_text(
        infinity_backlight_item,
        backlight_states[variable_item_get_current_value_index(infinity_backlight_item)]);
    variable_item_list_set_selected_item(variable_item_list, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID);
}

void view_settings_free(void) {
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID);
    variable_item_list_free(variable_item_list);
}
