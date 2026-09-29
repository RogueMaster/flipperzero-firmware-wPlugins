#include "../faraday_i.h"
#include <stdio.h>

static const char* const on_off[] = {"OFF", "ON"};

/* "433.92" alone is not a frequency, but the unit does NOT fit in the value
 * column - variable_item_list truncated "433.92 MHz" to "33.92 M" between the
 * arrows. It goes in the label instead, where there is room. */
static const char* faraday_band_text(uint8_t idx) {
    if(idx >= FDY_BAND_COUNT) idx = 1;
    return fdy_bands[idx].label;
}

static void faraday_settings_band_cb(VariableItem* item) {
    FaradayApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.band_index = idx;
    variable_item_set_current_value_text(item, faraday_band_text(idx));
}

static void faraday_settings_sound_cb(VariableItem* item) {
    FaradayApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.sound = idx > 0;
    variable_item_set_current_value_text(item, on_off[idx]);
}

static void faraday_settings_led_cb(VariableItem* item) {
    FaradayApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.led = idx > 0;
    variable_item_set_current_value_text(item, on_off[idx]);
}

void faraday_scene_settings_on_enter(void* context) {
    FaradayApp* app = context;
    VariableItemList* list = app->var_item_list;
    VariableItem* item;

    variable_item_list_reset(list);

    item =
        variable_item_list_add(list, "Band (MHz)", FDY_BAND_COUNT, faraday_settings_band_cb, app);
    variable_item_set_current_value_index(item, app->settings.band_index);
    variable_item_set_current_value_text(item, faraday_band_text(app->settings.band_index));

    item = variable_item_list_add(list, "Sound", 2, faraday_settings_sound_cb, app);
    variable_item_set_current_value_index(item, app->settings.sound ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.sound ? 1 : 0]);

    item = variable_item_list_add(list, "LED", 2, faraday_settings_led_cb, app);
    variable_item_set_current_value_index(item, app->settings.led ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.led ? 1 : 0]);

    view_dispatcher_switch_to_view(app->view_dispatcher, FaradayViewSettings);
}

bool faraday_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void faraday_scene_settings_on_exit(void* context) {
    FaradayApp* app = context;
    /* Persist as soon as the user leaves, not just at app exit, so a battery
     * pull on the next screen doesn't lose the change. */
    fdy_store_settings_save(&app->settings);
    variable_item_list_reset(app->var_item_list);
}
