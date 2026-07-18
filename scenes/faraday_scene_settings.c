#include "../faraday_i.h"

static const char* const on_off[] = {"OFF", "ON"};

static void faraday_settings_band_cb(VariableItem* item) {
    FaradayApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.band_index = idx;
    variable_item_set_current_value_text(item, fdy_bands[idx].label);
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

    item = variable_item_list_add(
        list, "Sub-GHz band", FDY_BAND_COUNT, faraday_settings_band_cb, app);
    variable_item_set_current_value_index(item, app->settings.band_index);
    variable_item_set_current_value_text(item, fdy_bands[app->settings.band_index].label);

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
    variable_item_list_reset(app->var_item_list);
}
