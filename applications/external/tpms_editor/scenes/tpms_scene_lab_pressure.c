#include "../tpms_app_i.h"
#include <stdio.h>

enum {
    TpmsLabConfigPressure = 0,
    TpmsLabConfigBand,
    TpmsLabConfigModulation,
    TpmsLabConfigMax,
    TpmsLabConfigCycle,
    TpmsLabConfigSessionIds,
    TpmsLabConfigSavedIds,
    TpmsLabConfigStart,
};

static const char* const lab_band_text[] = {"AUTO", "315.00", "433.92"};
static const char* const lab_mod_text[] = {"AUTO", "FSK", "OOK", "GFSK"};

static void tpms_lab_band_changed(VariableItem* item) {
    TPMSApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= COUNT_OF(lab_band_text)) index = 0U;
    app->lab_band_index = index;
    variable_item_set_current_value_text(item, lab_band_text[index]);
}

static void tpms_lab_mod_changed(VariableItem* item) {
    TPMSApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= COUNT_OF(lab_mod_text)) index = 0U;
    app->lab_modulation_index = index;
    variable_item_set_current_value_text(item, lab_mod_text[index]);
}

#define TPMS_LAB_PRESSURE_MIN_KPA 0
#define TPMS_LAB_PRESSURE_MAX_KPA 700

static VariableItem* tpms_lab_pressure_item = NULL;
static bool tpms_lab_pressure_input_open = false;

static void tpms_lab_pressure_set_text(TPMSApp* app) {
    if(!app || !tpms_lab_pressure_item) return;
    char text[20];
    snprintf(
        text,
        sizeof(text),
        "%.2f bar",
        (double)(app->lab_target_pressure_kpa / 100.0f));
    variable_item_set_current_value_text(tpms_lab_pressure_item, text);
}

static void tpms_lab_pressure_done(void* context, int32_t number) {
    TPMSApp* app = context;
    if(!app) return;

    if(number < TPMS_LAB_PRESSURE_MIN_KPA) number = TPMS_LAB_PRESSURE_MIN_KPA;
    if(number > TPMS_LAB_PRESSURE_MAX_KPA) number = TPMS_LAB_PRESSURE_MAX_KPA;
    app->lab_target_pressure_kpa = (float)number;
    tpms_lab_pressure_set_text(app);

    tpms_lab_pressure_input_open = false;
    variable_item_list_set_selected_item(app->variable_item_list, TpmsLabConfigPressure);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
}

static void tpms_lab_open_pressure_input(TPMSApp* app) {
    if(!app || !tpms_ensure_number_input(app)) return;

    int32_t current = (int32_t)(app->lab_target_pressure_kpa + 0.5f);
    if(current < TPMS_LAB_PRESSURE_MIN_KPA) current = TPMS_LAB_PRESSURE_MIN_KPA;
    if(current > TPMS_LAB_PRESSURE_MAX_KPA) current = TPMS_LAB_PRESSURE_MAX_KPA;

    number_input_set_header_text(app->number_input, "Pressure x0.01 bar");
    number_input_set_result_callback(
        app->number_input,
        tpms_lab_pressure_done,
        app,
        current,
        TPMS_LAB_PRESSURE_MIN_KPA,
        TPMS_LAB_PRESSURE_MAX_KPA);
    tpms_lab_pressure_input_open = true;
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewNumberInput);
}

static void tpms_lab_config_enter(void* context, uint32_t index) {
    TPMSApp* app = context;
    if(!app) return;

    if(index == TpmsLabConfigPressure) {
        tpms_lab_open_pressure_input(app);
        return;
    }
    if(index == TpmsLabConfigSessionIds) {
        app->lab_sensor_list_source = TPMSLabListSession;
        scene_manager_next_scene(app->scene_manager, TPMSSceneLabSensors);
        return;
    }
    if(index == TpmsLabConfigSavedIds) {
        app->lab_sensor_list_source = TPMSLabListSaved;
        scene_manager_next_scene(app->scene_manager, TPMSSceneLabSensors);
        return;
    }
    if(index == TpmsLabConfigStart) {
        scene_manager_next_scene(app->scene_manager, TPMSSceneLabRun);
    }
}

void tpms_scene_lab_pressure_on_enter(void* context) {
    TPMSApp* app = context;
    if(!app || !app->lab_unlocked || !app->variable_item_list) {
        if(app) scene_manager_previous_scene(app->scene_manager);
        return;
    }

    variable_item_list_reset(app->variable_item_list);

    app->lab_target_pressure_kpa = 0.0f;
    tpms_lab_pressure_input_open = false;
    tpms_lab_pressure_item = variable_item_list_add(
        app->variable_item_list, "TX pressure", 1U, NULL, app);
    tpms_lab_pressure_set_text(app);
    VariableItem* item = NULL;

    item = variable_item_list_add(
        app->variable_item_list, "RX band", COUNT_OF(lab_band_text), tpms_lab_band_changed, app);
    if(app->lab_band_index >= COUNT_OF(lab_band_text)) app->lab_band_index = 0U;
    variable_item_set_current_value_index(item, app->lab_band_index);
    tpms_lab_band_changed(item);

    item = variable_item_list_add(
        app->variable_item_list, "RX modulation", COUNT_OF(lab_mod_text), tpms_lab_mod_changed, app);
    if(app->lab_modulation_index >= COUNT_OF(lab_mod_text)) app->lab_modulation_index = 0U;
    variable_item_set_current_value_index(item, app->lab_modulation_index);
    tpms_lab_mod_changed(item);

    item = variable_item_list_add(app->variable_item_list, "Max sensors", 1U, NULL, app);
    variable_item_set_current_value_text(item, "10");

    item = variable_item_list_add(app->variable_item_list, "Cycle", 1U, NULL, app);
    variable_item_set_current_value_text(item, "RX 5s / BURST / 1s");

    item = variable_item_list_add(app->variable_item_list, "Captured", 1U, NULL, app);
    char session_text[16];
    snprintf(session_text, sizeof(session_text), "%u/10", tpms_lab_session_count());
    variable_item_set_current_value_text(item, session_text);

    item = variable_item_list_add(app->variable_item_list, "Saved IDs", 1U, NULL, app);
    char saved_text[16];
    snprintf(saved_text, sizeof(saved_text), "%u", (unsigned)tpms_lab_saved_count());
    variable_item_set_current_value_text(item, saved_text);

    item = variable_item_list_add(app->variable_item_list, "START", 1U, NULL, app);
    variable_item_set_current_value_text(item, "OK");

    variable_item_list_set_enter_callback(app->variable_item_list, tpms_lab_config_enter, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
}

bool tpms_scene_lab_pressure_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    if(!app) return false;

    if(event.type == SceneManagerEventTypeBack && tpms_lab_pressure_input_open) {
        tpms_lab_pressure_input_open = false;
        variable_item_list_set_selected_item(app->variable_item_list, TpmsLabConfigPressure);
        view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
        return true;
    }
    return false;
}

void tpms_scene_lab_pressure_on_exit(void* context) {
    TPMSApp* app = context;
    tpms_lab_pressure_input_open = false;
    tpms_lab_pressure_item = NULL;
    if(app && app->variable_item_list) variable_item_list_reset(app->variable_item_list);
}
