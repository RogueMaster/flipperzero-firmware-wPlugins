/*
 * settings_view.c — backlight + notification settings.
 */
#include "../air_stats_i.h"
#include <gui/modules/variable_item_list.h>

static View* view;
static VariableItemList* variable_item_list;

static const char* backlight_labels[] = {"5s", "Auto", "1m", "5m", "10m", "20m", "60m", "Inf"};
#define BACKLIGHT_COUNT 8
static const char onoff_states[2][4] = {"Off", "On"};

static VariableItem* backlight_item;
static VariableItem* led_notify_item;
static VariableItem* sound_notify_item;
static VariableItem* sound_volume_item;
static VariableItem* debug_mode_item;
static VariableItem* show_status_item;
static char volume_buf[4];

#define VIEW_ID ViewSettings

static uint32_t _exit_callback(void* context) {
    UNUSED(context);
    app->settings.backlight_mode = (uint8_t)variable_item_get_current_value_index(backlight_item);
    app->settings.led_notify = (bool)variable_item_get_current_value_index(led_notify_item);
    app->settings.sound_notify = (bool)variable_item_get_current_value_index(sound_notify_item);
    app->settings.sound_volume =
        (uint8_t)(variable_item_get_current_value_index(sound_volume_item) + 1);
    app->settings.debug_mode = (bool)variable_item_get_current_value_index(debug_mode_item);
    app->settings.show_status = (bool)variable_item_get_current_value_index(show_status_item);
    air_stats_apply_backlight();
    unitemp_saveSettings();
    unitemp_loadSettings();
    return ViewMainMenu;
}

static void _backlight_change(VariableItem* item) {
    uint8_t idx = (uint8_t)variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, backlight_labels[idx]);
    app->settings.backlight_mode = idx;
    air_stats_apply_backlight();
}

static void _led_notify_change(VariableItem* item) {
    variable_item_set_current_value_text(
        item, onoff_states[variable_item_get_current_value_index(item)]);
}

static void _sound_notify_change(VariableItem* item) {
    variable_item_set_current_value_text(
        item, onoff_states[variable_item_get_current_value_index(item)]);
}

static void _sound_volume_change(VariableItem* item) {
    uint8_t idx = (uint8_t)variable_item_get_current_value_index(item);
    snprintf(volume_buf, sizeof(volume_buf), "%d", idx + 1);
    variable_item_set_current_value_text(item, volume_buf);
}

static void _debug_mode_change(VariableItem* item) {
    variable_item_set_current_value_text(
        item, onoff_states[variable_item_get_current_value_index(item)]);
}

static void _show_status_change(VariableItem* item) {
    variable_item_set_current_value_text(
        item, onoff_states[variable_item_get_current_value_index(item)]);
}

void view_settings_alloc(void) {
    variable_item_list = variable_item_list_alloc();
    variable_item_list_reset(variable_item_list);

    backlight_item = variable_item_list_add(
        variable_item_list, "Backlight", BACKLIGHT_COUNT, _backlight_change, app);

    led_notify_item =
        variable_item_list_add(variable_item_list, "LED Notify", 2, _led_notify_change, app);

    sound_notify_item =
        variable_item_list_add(variable_item_list, "Sound Alert", 2, _sound_notify_change, app);

    sound_volume_item =
        variable_item_list_add(variable_item_list, "Sound Volume", 10, _sound_volume_change, app);

    debug_mode_item =
        variable_item_list_add(variable_item_list, "Debug Mode", 2, _debug_mode_change, app);

    show_status_item =
        variable_item_list_add(variable_item_list, "Clock/Battery", 2, _show_status_change, app);

    view = variable_item_list_get_view(variable_item_list);
    view_set_previous_callback(view, _exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_ID, view);
}

void view_settings_switch(void) {
    variable_item_set_current_value_index(backlight_item, app->settings.backlight_mode);
    variable_item_set_current_value_text(
        backlight_item, backlight_labels[app->settings.backlight_mode]);

    variable_item_set_current_value_index(led_notify_item, (uint8_t)app->settings.led_notify);
    variable_item_set_current_value_text(
        led_notify_item, onoff_states[variable_item_get_current_value_index(led_notify_item)]);

    variable_item_set_current_value_index(sound_notify_item, (uint8_t)app->settings.sound_notify);
    variable_item_set_current_value_text(
        sound_notify_item, onoff_states[variable_item_get_current_value_index(sound_notify_item)]);

    uint8_t vol_idx =
        (app->settings.sound_volume > 0) ? (uint8_t)(app->settings.sound_volume - 1) : 0;
    variable_item_set_current_value_index(sound_volume_item, vol_idx);
    snprintf(volume_buf, sizeof(volume_buf), "%d", app->settings.sound_volume);
    variable_item_set_current_value_text(sound_volume_item, volume_buf);

    variable_item_set_current_value_index(debug_mode_item, (uint8_t)app->settings.debug_mode);
    variable_item_set_current_value_text(
        debug_mode_item, onoff_states[variable_item_get_current_value_index(debug_mode_item)]);

    variable_item_set_current_value_index(show_status_item, (uint8_t)app->settings.show_status);
    variable_item_set_current_value_text(
        show_status_item, onoff_states[variable_item_get_current_value_index(show_status_item)]);

    variable_item_list_set_selected_item(variable_item_list, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID);
}

void view_settings_free(void) {
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID);
    variable_item_list_free(variable_item_list);
}
