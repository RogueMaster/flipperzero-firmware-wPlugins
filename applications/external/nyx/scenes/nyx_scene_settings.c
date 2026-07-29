#include "../nyx_i.h"

const char* nyx_settings_mode_label(uint8_t index) {
    switch(index) {
    case IrSenseModeOnboard:
        return "Onboard";
    case IrSenseModeProbe:
        return "Probe";
    default:
        return "Auto";
    }
}

const char* nyx_settings_sensitivity_label(uint8_t index) {
    switch(index) {
    case 0:
        return "High";
    case 2:
        return "Low";
    default:
        return "Medium";
    }
}

static void nyx_settings_mode_cb(VariableItem* item) {
    NyxApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.mode_index = idx;
    variable_item_set_current_value_text(item, nyx_settings_mode_label(idx));
}

static void nyx_settings_sensitivity_cb(VariableItem* item) {
    NyxApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.sensitivity_index = idx;
    variable_item_set_current_value_text(item, nyx_settings_sensitivity_label(idx));
}

static void nyx_settings_probe_pin_cb(VariableItem* item) {
    NyxApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.probe_pin_index = idx;
    variable_item_set_current_value_text(item, ir_sense_probe_pins()[idx].name);
}

static void nyx_settings_sound_cb(VariableItem* item) {
    NyxApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.sound = idx;
    variable_item_set_current_value_text(item, idx ? "ON" : "OFF");
}

static void nyx_settings_vibro_cb(VariableItem* item) {
    NyxApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.vibro = idx;
    variable_item_set_current_value_text(item, idx ? "ON" : "OFF");
}

static void nyx_settings_led_cb(VariableItem* item) {
    NyxApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.led = idx;
    variable_item_set_current_value_text(item, idx ? "ON" : "OFF");
}

void nyx_scene_settings_on_enter(void* context) {
    NyxApp* app = context;
    VariableItemList* list = app->var_item_list;
    VariableItem* item;

    variable_item_list_reset(list);

    item = variable_item_list_add(list, "Mode", 3, nyx_settings_mode_cb, app);
    variable_item_set_current_value_index(item, app->settings.mode_index);
    variable_item_set_current_value_text(item, nyx_settings_mode_label(app->settings.mode_index));

    item = variable_item_list_add(list, "Sensitivity", 3, nyx_settings_sensitivity_cb, app);
    variable_item_set_current_value_index(item, app->settings.sensitivity_index);
    variable_item_set_current_value_text(
        item, nyx_settings_sensitivity_label(app->settings.sensitivity_index));

    uint8_t pin_count = ir_sense_probe_pin_count();
    item = variable_item_list_add(list, "Probe pin", pin_count, nyx_settings_probe_pin_cb, app);
    variable_item_set_current_value_index(item, app->settings.probe_pin_index);
    variable_item_set_current_value_text(
        item, ir_sense_probe_pins()[app->settings.probe_pin_index].name);

    item = variable_item_list_add(list, "Sound", 2, nyx_settings_sound_cb, app);
    variable_item_set_current_value_index(item, app->settings.sound);
    variable_item_set_current_value_text(item, app->settings.sound ? "ON" : "OFF");

    item = variable_item_list_add(list, "Vibro", 2, nyx_settings_vibro_cb, app);
    variable_item_set_current_value_index(item, app->settings.vibro);
    variable_item_set_current_value_text(item, app->settings.vibro ? "ON" : "OFF");

    item = variable_item_list_add(list, "LED", 2, nyx_settings_led_cb, app);
    variable_item_set_current_value_index(item, app->settings.led);
    variable_item_set_current_value_text(item, app->settings.led ? "ON" : "OFF");

    view_dispatcher_switch_to_view(app->view_dispatcher, NyxViewSettings);
}

bool nyx_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void nyx_scene_settings_on_exit(void* context) {
    NyxApp* app = context;
    /* Persist on the way out, so the next launch comes up the way you left it. */
    nyx_store_settings_save(&app->settings);
    variable_item_list_reset(app->var_item_list);
}
