#include "../vk_thermo.h"
#include "../helpers/vk_thermo_storage.h"
#include <lib/toolbox/value_index.h>

const char* const haptic_text[2] = {
    "OFF",
    "ON",
};
const uint32_t haptic_value[2] = {
    VkThermoHapticOff,
    VkThermoHapticOn,
};

const char* const speaker_text[2] = {
    "OFF",
    "ON",
};
const uint32_t speaker_value[2] = {
    VkThermoSpeakerOff,
    VkThermoSpeakerOn,
};

const char* const led_text[2] = {
    "OFF",
    "ON",
};
const uint32_t led_value[2] = {
    VkThermoLedOff,
    VkThermoLedOn,
};

const char* const temp_unit_text[3] = {
    "Celsius",
    "Fahrenheit",
    "Kelvin",
};
const uint32_t temp_unit_value[3] = {
    VkThermoTempUnitCelsius,
    VkThermoTempUnitFahrenheit,
    VkThermoTempUnitKelvin,
};

static void vk_thermo_scene_settings_set_haptic(VariableItem* item) {
    VkThermo* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, haptic_text[index]);
    app->haptic = haptic_value[index];
}

static void vk_thermo_scene_settings_set_speaker(VariableItem* item) {
    VkThermo* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, speaker_text[index]);
    app->speaker = speaker_value[index];
}

static void vk_thermo_scene_settings_set_led(VariableItem* item) {
    VkThermo* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, led_text[index]);
    app->led = led_value[index];
}

static void vk_thermo_scene_settings_set_temp_unit(VariableItem* item) {
    VkThermo* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, temp_unit_text[index]);
    app->temp_unit = temp_unit_value[index];
}

const char* const eh_timeout_text[VkThermoEhTimeoutCount] = {
    "1s",
    "2s",
    "5s",
    "10s",
    "30s",
    "None",
};
const uint32_t eh_timeout_value[VkThermoEhTimeoutCount] = {
    VkThermoEhTimeout1s,
    VkThermoEhTimeout2s,
    VkThermoEhTimeout5s,
    VkThermoEhTimeout10s,
    VkThermoEhTimeout30s,
    VkThermoEhTimeoutNone,
};

static void vk_thermo_scene_settings_set_eh_timeout(VariableItem* item) {
    VkThermo* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, eh_timeout_text[index]);
    app->eh_timeout = eh_timeout_value[index];
}

const char* const debug_text[2] = {
    "OFF",
    "ON",
};
const uint32_t debug_value[2] = {
    VkThermoDebugOff,
    VkThermoDebugOn,
};

static void vk_thermo_scene_settings_set_debug(VariableItem* item) {
    VkThermo* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, debug_text[index]);
    app->debug = debug_value[index];
}

void vk_thermo_scene_settings_on_enter(void* context) {
    VkThermo* app = context;
    VariableItem* item;
    uint8_t value_index;

    // Temperature Unit
    item = variable_item_list_add(
        app->variable_item_list, "Temp Unit:", 3, vk_thermo_scene_settings_set_temp_unit, app);
    value_index = value_index_uint32(app->temp_unit, temp_unit_value, 3);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, temp_unit_text[value_index]);

    // EH Timeout
    item = variable_item_list_add(
        app->variable_item_list,
        "EH Timeout:",
        VkThermoEhTimeoutCount,
        vk_thermo_scene_settings_set_eh_timeout,
        app);
    value_index = value_index_uint32(app->eh_timeout, eh_timeout_value, VkThermoEhTimeoutCount);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, eh_timeout_text[value_index]);

    // Vibro on/off
    item = variable_item_list_add(
        app->variable_item_list, "Vibro/Haptic:", 2, vk_thermo_scene_settings_set_haptic, app);
    value_index = value_index_uint32(app->haptic, haptic_value, 2);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, haptic_text[value_index]);

    // Sound on/off
    item = variable_item_list_add(
        app->variable_item_list, "Sound:", 2, vk_thermo_scene_settings_set_speaker, app);
    value_index = value_index_uint32(app->speaker, speaker_value, 2);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, speaker_text[value_index]);

    // LED Effects on/off
    item = variable_item_list_add(
        app->variable_item_list, "LED FX:", 2, vk_thermo_scene_settings_set_led, app);
    value_index = value_index_uint32(app->led, led_value, 2);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, led_text[value_index]);

    // Debug diagnostics
    item = variable_item_list_add(
        app->variable_item_list, "Debug:", 2, vk_thermo_scene_settings_set_debug, app);
    value_index = value_index_uint32(app->debug, debug_value, 2);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, debug_text[value_index]);

    view_dispatcher_switch_to_view(app->view_dispatcher, VkThermoViewIdSettings);
}

bool vk_thermo_scene_settings_on_event(void* context, SceneManagerEvent event) {
    VkThermo* app = context;
    UNUSED(app);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // Save settings when leaving
        vk_thermo_save_settings(app);
    }

    return consumed;
}

void vk_thermo_scene_settings_on_exit(void* context) {
    VkThermo* app = context;
    variable_item_list_set_selected_item(app->variable_item_list, 0);
    variable_item_list_reset(app->variable_item_list);
}
