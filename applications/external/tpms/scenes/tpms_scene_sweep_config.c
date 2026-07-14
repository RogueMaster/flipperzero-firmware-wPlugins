#include "../tpms_app_i.h"

#define SWEEP_CYCLES_MIN  1
#define SWEEP_CYCLES_MAX  10
#define SWEEP_SECONDS_MIN 1
#define SWEEP_SECONDS_MAX 10

static const char* sweep_frequency_names[] = {
    "433.92 MHz",
    "315 MHz",
};
#define SWEEP_FREQUENCY_COUNT 2

static const char* sweep_preset_names[] = {
    "AM650",
    "AM270",
    "FM238",
    "FM12K",
    "FM476",
};
#define SWEEP_PRESET_COUNT 5

static void tpms_scene_sweep_config_frequency_changed(VariableItem* item) {
    TPMSApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->sweep_start_frequency = index;
    variable_item_set_current_value_text(item, sweep_frequency_names[index]);
}

static void tpms_scene_sweep_config_preset_changed(VariableItem* item) {
    TPMSApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->sweep_start_preset = index;
    variable_item_set_current_value_text(item, sweep_preset_names[index]);
}

static void tpms_scene_sweep_config_cycles_changed(VariableItem* item) {
    TPMSApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->sweep_max_cycles = index + SWEEP_CYCLES_MIN;

    char text[8];
    snprintf(text, sizeof(text), "%d", app->sweep_max_cycles);
    variable_item_set_current_value_text(item, text);
}

static void tpms_scene_sweep_config_seconds_changed(VariableItem* item) {
    TPMSApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->sweep_seconds_per_combo = index + SWEEP_SECONDS_MIN;

    char text[8];
    snprintf(text, sizeof(text), "%ds", app->sweep_seconds_per_combo);
    variable_item_set_current_value_text(item, text);
}

static void tpms_scene_sweep_config_enter_callback(void* context, uint32_t index) {
    TPMSApp* app = context;
    // Index 4 is "Start Sweep" button
    if(index == 4) {
        view_dispatcher_send_custom_event(app->view_dispatcher, index);
    }
}

void tpms_scene_sweep_config_on_enter(void* context) {
    TPMSApp* app = context;
    VariableItemList* var_item_list = app->variable_item_list;
    VariableItem* item;
    char text[8];

    variable_item_list_reset(var_item_list);

    // Starting Frequency
    item = variable_item_list_add(
        var_item_list,
        "Start Frequency",
        SWEEP_FREQUENCY_COUNT,
        tpms_scene_sweep_config_frequency_changed,
        app);
    variable_item_set_current_value_index(item, app->sweep_start_frequency);
    variable_item_set_current_value_text(item, sweep_frequency_names[app->sweep_start_frequency]);

    // Starting Modulation
    item = variable_item_list_add(
        var_item_list,
        "Start Modulation",
        SWEEP_PRESET_COUNT,
        tpms_scene_sweep_config_preset_changed,
        app);
    variable_item_set_current_value_index(item, app->sweep_start_preset);
    variable_item_set_current_value_text(item, sweep_preset_names[app->sweep_start_preset]);

    // Number of Cycles
    item = variable_item_list_add(
        var_item_list,
        "Cycles",
        SWEEP_CYCLES_MAX - SWEEP_CYCLES_MIN + 1,
        tpms_scene_sweep_config_cycles_changed,
        app);
    variable_item_set_current_value_index(item, app->sweep_max_cycles - SWEEP_CYCLES_MIN);
    snprintf(text, sizeof(text), "%d", app->sweep_max_cycles);
    variable_item_set_current_value_text(item, text);

    // Seconds per Combo
    item = variable_item_list_add(
        var_item_list,
        "Time per combo",
        SWEEP_SECONDS_MAX - SWEEP_SECONDS_MIN + 1,
        tpms_scene_sweep_config_seconds_changed,
        app);
    variable_item_set_current_value_index(item, app->sweep_seconds_per_combo - SWEEP_SECONDS_MIN);
    snprintf(text, sizeof(text), "%ds", app->sweep_seconds_per_combo);
    variable_item_set_current_value_text(item, text);

    // Start Sweep button
    variable_item_list_add(var_item_list, "Start Sweep", 0, NULL, app);

    variable_item_list_set_enter_callback(
        var_item_list, tpms_scene_sweep_config_enter_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
}

bool tpms_scene_sweep_config_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == 4) { // Start Sweep button
            // Set up sweep with user config
            app->scan_mode = TPMSScanModeSweep;
            app->protocol_filter = TPMSProtocolFilterAll;
            app->sweep_frequency_index = app->sweep_start_frequency;
            app->sweep_preset_index = app->sweep_start_preset;
            app->sweep_cycle_count = 0;
            app->sweep_tick_count = 0;
            app->sweep_found_signal = false;

            scene_manager_next_scene(app->scene_manager, TPMSSceneReceiver);
            consumed = true;
        }
    }

    return consumed;
}

void tpms_scene_sweep_config_on_exit(void* context) {
    TPMSApp* app = context;
    variable_item_list_reset(app->variable_item_list);
}
