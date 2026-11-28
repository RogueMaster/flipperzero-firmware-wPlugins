#include "../tpms_app_i.h"
#include "../tpms_editor_tx.h"
#include "../tpms_encoder.h"
#include "../tpms_save.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TPMS_SIM_TEMP_MIN_C (-50)
#define TPMS_SIM_TEMP_MAX_C 150
#define TPMS_SIM_PRESSURE_MIN_KPA 0
#define TPMS_SIM_PRESSURE_MAX_KPA 700
#define TPMS_SIM_ID_TEXT_SIZE 11U
#define TPMS_SIM_FLAGS_TEXT_SIZE 4U
#define TPMS_SIM_LAB_NAME_SIZE 25U
#define TPMS_AUTO_TX_INTERVAL_MIN_S 1
#define TPMS_AUTO_TX_INTERVAL_MAX_S 3600
#define TPMS_AUTO_TX_PRESET_COUNT 6U

typedef enum {
    TPMSSimItemProtocol = 0,
    TPMSSimItemId,
    TPMSSimItemPressure,
    TPMSSimItemTemperature,
    TPMSSimItemFlags,
    TPMSSimItemRepeat,
    TPMSSimItemSave,
    TPMSSimItemSend,
    TPMSSimItemAutoInterval,
    TPMSSimItemAutoToggle,
} TPMSSimItem;

typedef struct {
    TpmsEditValues values;
    VariableItem* protocol_item;
    VariableItem* id_item;
    VariableItem* pressure_item;
    VariableItem* temperature_item;
    VariableItem* flags_item;
    VariableItem* repeat_item;
    VariableItem* save_item;
    VariableItem* send_item;
    VariableItem* delete_item;
    VariableItem* auto_interval_item;
    VariableItem* auto_toggle_item;
    uint16_t auto_tx_interval_s;
    uint32_t auto_tx_elapsed_ds;
    uint32_t auto_tx_count;
    bool auto_tx_enabled;
    bool auto_tx_custom;
    uint8_t lab_source;
    uint8_t lab_session_slot;
    char lab_saved_path[128];
    uint32_t delete_index;
    uint32_t auto_interval_index;
    uint32_t auto_toggle_index;
} TPMSSimulationState;

static TPMSSimulationState simulation_state;
static char tpms_sim_id_text[TPMS_SIM_ID_TEXT_SIZE];
static char tpms_sim_flags_text[TPMS_SIM_FLAGS_TEXT_SIZE];
static char tpms_sim_lab_name[TPMS_SIM_LAB_NAME_SIZE];
static bool tpms_sim_id_input_open = false;
static bool tpms_sim_flags_input_open = false;
static bool tpms_sim_number_input_open = false;
static bool tpms_sim_lab_name_input_open = false;
static uint32_t tpms_sim_number_target = TPMSSimItemPressure;

static const uint16_t tpms_auto_tx_presets_s[TPMS_AUTO_TX_PRESET_COUNT] = {1U, 2U, 5U, 10U, 30U, 60U};
static const char* const tpms_auto_tx_preset_text[TPMS_AUTO_TX_PRESET_COUNT] = {
    "1 s", "2 s", "5 s", "10 s", "30 s", "60 s"};

static void tpms_sim_set_auto_toggle_text(bool enabled) {
    if(!simulation_state.auto_toggle_item) return;
    variable_item_set_current_value_index(simulation_state.auto_toggle_item, enabled ? 1U : 0U);
    if(enabled) {
        char text[20];
        snprintf(text, sizeof(text), "ON %lu", (unsigned long)simulation_state.auto_tx_count);
        variable_item_set_current_value_text(simulation_state.auto_toggle_item, text);
    } else {
        variable_item_set_current_value_text(simulation_state.auto_toggle_item, "OFF");
    }
}

static void tpms_sim_stop_auto_tx(void) {
    simulation_state.auto_tx_enabled = false;
    simulation_state.auto_tx_elapsed_ds = 0U;
    tpms_sim_set_auto_toggle_text(false);
}

static void tpms_sim_auto_interval_changed(VariableItem* item) {
    if(!item) return;
    uint8_t index = variable_item_get_current_value_index(item);
    if(index < TPMS_AUTO_TX_PRESET_COUNT) {
        simulation_state.auto_tx_interval_s = tpms_auto_tx_presets_s[index];
        simulation_state.auto_tx_custom = false;
        variable_item_set_current_value_text(item, tpms_auto_tx_preset_text[index]);
    } else {
        simulation_state.auto_tx_custom = true;
        variable_item_set_current_value_text(item, "Custom");
    }
    if(simulation_state.auto_tx_enabled) simulation_state.auto_tx_elapsed_ds = 0U;
}

static void tpms_sim_auto_toggle_changed(VariableItem* item) {
    if(!item) return;
    uint8_t index = variable_item_get_current_value_index(item);
    if(index > 1U) index = 0U;
    simulation_state.auto_tx_enabled = index == 1U;
    simulation_state.auto_tx_count = 0U;
    simulation_state.auto_tx_elapsed_ds = simulation_state.auto_tx_enabled ?
                                               (uint32_t)simulation_state.auto_tx_interval_s * 10U :
                                               0U;
    tpms_sim_set_auto_toggle_text(simulation_state.auto_tx_enabled);
}

static int8_t tpms_sim_hex_value(char c) {
    if(c >= '0' && c <= '9') return (int8_t)(c - '0');
    if(c >= 'A' && c <= 'F') return (int8_t)(c - 'A' + 10);
    if(c >= 'a' && c <= 'f') return (int8_t)(c - 'a' + 10);
    return -1;
}

static bool tpms_sim_parse_id(const char* text, uint32_t* value) {
    if(!text || !value) return false;
    if(text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text += 2;
    size_t len = strlen(text);
    if(len == 0U || len > 8U) return false;
    uint32_t parsed = 0U;
    for(size_t i = 0; i < len; i++) {
        const int8_t nibble = tpms_sim_hex_value(text[i]);
        if(nibble < 0) return false;
        parsed = (parsed << 4U) | (uint32_t)nibble;
    }
    *value = parsed;
    return true;
}

static bool tpms_sim_id_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    uint32_t value = 0U;
    if(!tpms_sim_parse_id(text, &value)) {
        furi_string_set(error, "Enter 1-8 HEX chars: 0-9, A-F");
        return false;
    }
    UNUSED(value);
    return true;
}

static void tpms_sim_set_id_text(void) {
    if(!simulation_state.id_item) return;
    char text[16];
    snprintf(text, sizeof(text), "0x%08lX", (unsigned long)simulation_state.values.id);
    variable_item_set_current_value_text(simulation_state.id_item, text);
}

static void tpms_sim_set_pressure_text(void) {
    if(!simulation_state.pressure_item) return;
    char text[20];
    snprintf(text, sizeof(text), "%.2f bar", (double)(simulation_state.values.pressure_kpa / 100.0f));
    variable_item_set_current_value_text(simulation_state.pressure_item, text);
}

static void tpms_sim_set_temperature_text(void) {
    if(!simulation_state.temperature_item) return;
    char text[16];
    snprintf(text, sizeof(text), "%d C", (int)simulation_state.values.temperature_c);
    variable_item_set_current_value_text(simulation_state.temperature_item, text);
}

static void tpms_sim_set_flags_text(void) {
    if(!simulation_state.flags_item) return;
    char text[16];
    snprintf(text, sizeof(text), "0x%08lX", (unsigned long)simulation_state.values.flags);
    variable_item_set_current_value_text(simulation_state.flags_item, text);
}

static void tpms_sim_refresh_values(void) {
    if(simulation_state.protocol_item) {
        variable_item_set_current_value_text(
            simulation_state.protocol_item, tpms_encoder_display_name(simulation_state.values.kind));
    }
    tpms_sim_set_id_text();

    if(simulation_state.pressure_item) tpms_sim_set_pressure_text();
    if(simulation_state.temperature_item) tpms_sim_set_temperature_text();
    if(simulation_state.flags_item) tpms_sim_set_flags_text();

    if(simulation_state.repeat_item) {
        const uint8_t repeat_index = simulation_state.values.repeat_count == 10U ? 1U : 0U;
        simulation_state.values.repeat_count = repeat_index ? 10U : 3U;
        variable_item_set_current_value_index(simulation_state.repeat_item, repeat_index);
        variable_item_set_current_value_text(simulation_state.repeat_item, repeat_index ? "10x" : "3x");
    }

    if(simulation_state.save_item) variable_item_set_current_value_text(simulation_state.save_item, "OK");
    if(simulation_state.send_item) variable_item_set_current_value_text(simulation_state.send_item, "OK");
    if(simulation_state.delete_item) variable_item_set_current_value_text(simulation_state.delete_item, "OK");
}

static void tpms_sim_protocol_changed(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= tpms_encoder_test_count()) index = 0U;
    const TpmsEncoderKind kind = tpms_encoder_test_kind(index);
    tpms_encoder_default_values(kind, &simulation_state.values);
    tpms_sim_refresh_values();
}

static void tpms_sim_repeat_changed(VariableItem* item) {
    const uint8_t index = variable_item_get_current_value_index(item);
    simulation_state.values.repeat_count = index ? 10U : 3U;
    variable_item_set_current_value_text(item, index ? "10x" : "3x");
}

static void tpms_sim_id_done(void* context) {
    TPMSApp* app = context;
    if(!app) return;
    uint32_t value = simulation_state.values.id;
    if(tpms_sim_parse_id(tpms_sim_id_text, &value)) {
        simulation_state.values.id = value;
        simulation_state.values.edit_mask |= TPMS_EDIT_ID;
    }
    tpms_sim_id_input_open = false;
    tpms_sim_set_id_text();
    variable_item_list_set_selected_item(app->variable_item_list, TPMSSimItemId);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
}

static void tpms_sim_open_id_input(TPMSApp* app) {
    if(!app || !tpms_ensure_text_input(app)) return;
    tpms_sim_stop_auto_tx();
    snprintf(tpms_sim_id_text, sizeof(tpms_sim_id_text), "%08lX", (unsigned long)simulation_state.values.id);
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Sensor ID HEX (0-9 A-F)");
    text_input_set_minimum_length(app->text_input, 1U);
    text_input_set_validator(app->text_input, tpms_sim_id_validator, app);
    text_input_set_result_callback(
        app->text_input,
        tpms_sim_id_done,
        app,
        tpms_sim_id_text,
        sizeof(tpms_sim_id_text),
        false);
    tpms_sim_id_input_open = true;
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewTextInput);
}

static bool tpms_sim_flags_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    uint32_t value = 0U;
    if(!tpms_sim_parse_id(text, &value) || value > 0xFFU) {
        furi_string_set(error, "Enter HEX 00-FF");
        return false;
    }
    return true;
}

static void tpms_sim_flags_done(void* context) {
    TPMSApp* app = context;
    if(!app) return;
    uint32_t low = simulation_state.values.flags & 0xFFU;
    if(tpms_sim_parse_id(tpms_sim_flags_text, &low) && low <= 0xFFU) {
        simulation_state.values.flags =
            (simulation_state.values.flags & 0xFFFFFF00U) | (low & 0xFFU);
        simulation_state.values.edit_mask |= TPMS_EDIT_FLAGS;
    }
    tpms_sim_flags_input_open = false;
    tpms_sim_set_flags_text();
    variable_item_list_set_selected_item(app->variable_item_list, TPMSSimItemFlags);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
}

static void tpms_sim_open_flags_input(TPMSApp* app) {
    if(!app || !tpms_ensure_text_input(app)) return;
    tpms_sim_stop_auto_tx();
    snprintf(
        tpms_sim_flags_text,
        sizeof(tpms_sim_flags_text),
        "%02lX",
        (unsigned long)(simulation_state.values.flags & 0xFFU));
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Flags HEX 00-FF");
    text_input_set_minimum_length(app->text_input, 1U);
    text_input_set_validator(app->text_input, tpms_sim_flags_validator, app);
    text_input_set_result_callback(
        app->text_input,
        tpms_sim_flags_done,
        app,
        tpms_sim_flags_text,
        sizeof(tpms_sim_flags_text),
        false);
    tpms_sim_flags_input_open = true;
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewTextInput);
}

static void tpms_sim_number_done(void* context, int32_t number) {
    TPMSApp* app = context;
    if(!app) return;
    if(tpms_sim_number_target == TPMSSimItemPressure) {
        if(number < TPMS_SIM_PRESSURE_MIN_KPA) number = TPMS_SIM_PRESSURE_MIN_KPA;
        if(number > TPMS_SIM_PRESSURE_MAX_KPA) number = TPMS_SIM_PRESSURE_MAX_KPA;
        simulation_state.values.pressure_kpa = (float)number;
        simulation_state.values.edit_mask |= TPMS_EDIT_PRESSURE;
        tpms_sim_set_pressure_text();
    } else if(tpms_sim_number_target == TPMSSimItemTemperature) {
        if(number < TPMS_SIM_TEMP_MIN_C) number = TPMS_SIM_TEMP_MIN_C;
        if(number > TPMS_SIM_TEMP_MAX_C) number = TPMS_SIM_TEMP_MAX_C;
        simulation_state.values.temperature_c = (float)number;
        simulation_state.values.edit_mask |= TPMS_EDIT_TEMPERATURE;
        if(simulation_state.values.kind == TpmsEncoderSchraderEG53MA4)
            simulation_state.values.temperature_raw_f_valid = false;
        tpms_sim_set_temperature_text();
    } else if(tpms_sim_number_target == TPMSSimItemAutoInterval) {
        if(number < TPMS_AUTO_TX_INTERVAL_MIN_S) number = TPMS_AUTO_TX_INTERVAL_MIN_S;
        if(number > TPMS_AUTO_TX_INTERVAL_MAX_S) number = TPMS_AUTO_TX_INTERVAL_MAX_S;
        simulation_state.auto_tx_interval_s = (uint16_t)number;
        simulation_state.auto_tx_custom = true;
        if(simulation_state.auto_interval_item) {
            char text[20];
            variable_item_set_current_value_index(
                simulation_state.auto_interval_item, TPMS_AUTO_TX_PRESET_COUNT);
            snprintf(text, sizeof(text), "%u s", simulation_state.auto_tx_interval_s);
            variable_item_set_current_value_text(simulation_state.auto_interval_item, text);
        }
    }
    tpms_sim_number_input_open = false;
    variable_item_list_set_selected_item(
        app->variable_item_list,
        tpms_sim_number_target == TPMSSimItemAutoInterval ?
            simulation_state.auto_interval_index : tpms_sim_number_target);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
}

static void tpms_sim_open_number_input(TPMSApp* app, uint32_t target) {
    if(!app || !tpms_ensure_number_input(app)) return;
    tpms_sim_stop_auto_tx();
    tpms_sim_number_target = target;
    int32_t current = 0;
    int32_t min = 0;
    int32_t max = 0;
    const char* header = "Value";
    if(target == TPMSSimItemPressure) {
        current = (int32_t)(simulation_state.values.pressure_kpa + 0.5f);
        min = TPMS_SIM_PRESSURE_MIN_KPA;
        max = TPMS_SIM_PRESSURE_MAX_KPA;
        header = "Pressure x0.01 bar";
    } else if(target == TPMSSimItemTemperature) {
        current = (int32_t)(simulation_state.values.temperature_c +
                            (simulation_state.values.temperature_c >= 0.0f ? 0.5f : -0.5f));
        min = TPMS_SIM_TEMP_MIN_C;
        max = TPMS_SIM_TEMP_MAX_C;
        header = "Temperature C";
    } else if(target == TPMSSimItemAutoInterval) {
        current = (int32_t)simulation_state.auto_tx_interval_s;
        min = TPMS_AUTO_TX_INTERVAL_MIN_S;
        max = TPMS_AUTO_TX_INTERVAL_MAX_S;
        header = "Auto TX seconds";
    } else {
        return;
    }
    number_input_set_header_text(app->number_input, header);
    number_input_set_result_callback(
        app->number_input, tpms_sim_number_done, app, current, min, max);
    tpms_sim_number_input_open = true;
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewNumberInput);
}

static bool tpms_sim_lab_name_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    if(!text || !text[0]) {
        furi_string_set(error, "Enter name, e.g. OPEL FL");
        return false;
    }
    bool has_visible = false;
    for(size_t i = 0U; text[i]; i++) {
        const unsigned char c = (unsigned char)text[i];
        if(c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' ||
           c == '<' || c == '>' || c == '|') {
            furi_string_set(error, "Bez / \\ : * ? \" < > |");
            return false;
        }
        if(c > ' ') has_visible = true;
    }
    if(!has_visible) {
        furi_string_set(error, "Name cannot be empty");
        return false;
    }
    return true;
}

static void tpms_sim_lab_name_done(void* context) {
    TPMSApp* app = context;
    if(!app) return;
    tpms_sim_lab_name_input_open = false;

    variable_item_set_current_value_text(simulation_state.save_item, "SAVING...");
    const char* protocol = tpms_encoder_name(simulation_state.values.kind);
    bool ok = false;
    if(simulation_state.lab_source == TPMSLabSimulationSession) {
        ok = tpms_lab_saved_store_named(
            &simulation_state.values, protocol, tpms_sim_lab_name);
    } else if(simulation_state.lab_source == TPMSLabSimulationSaved &&
              simulation_state.lab_saved_path[0]) {
        ok = tpms_profile_save_to_path_named(
            simulation_state.lab_saved_path,
            &simulation_state.values,
            protocol,
            tpms_sim_lab_name);
    }

    variable_item_set_current_value_text(
        simulation_state.save_item, ok ? "ID SAVED" : "ERROR");
    notification_message(
        app->notifications, ok ? &sequence_blink_green_10 : &sequence_blink_red_10);
    variable_item_list_set_selected_item(app->variable_item_list, TPMSSimItemSave);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
}

static void tpms_sim_open_lab_name_input(TPMSApp* app) {
    if(!app || !tpms_ensure_text_input(app)) return;
    tpms_sim_stop_auto_tx();
    tpms_sim_lab_name[0] = '\0';

    if(simulation_state.lab_source == TPMSLabSimulationSaved &&
       simulation_state.lab_saved_path[0]) {
        uint32_t saved_id = 0U;
        (void)tpms_profile_read_label(
            simulation_state.lab_saved_path,
            tpms_sim_lab_name,
            sizeof(tpms_sim_lab_name),
            &saved_id);
        UNUSED(saved_id);
    }

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "ID name, e.g. OPEL FL");
    text_input_set_minimum_length(app->text_input, 1U);
    text_input_set_validator(app->text_input, tpms_sim_lab_name_validator, app);
    text_input_set_result_callback(
        app->text_input,
        tpms_sim_lab_name_done,
        app,
        tpms_sim_lab_name,
        sizeof(tpms_sim_lab_name),
        false);
    tpms_sim_lab_name_input_open = true;
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewTextInput);
}

static void tpms_sim_enter_callback(void* context, uint32_t index) {
    TPMSApp* app = context;
    if(!app) return;

    if(index == TPMSSimItemId) {
        tpms_sim_open_id_input(app);
        return;
    }
    if(index == TPMSSimItemPressure) {
        tpms_sim_open_number_input(app, TPMSSimItemPressure);
        return;
    }
    if(index == TPMSSimItemTemperature) {
        tpms_sim_open_number_input(app, TPMSSimItemTemperature);
        return;
    }
    if(index == TPMSSimItemFlags) {
        tpms_sim_open_flags_input(app);
        return;
    }
    if(index == simulation_state.auto_interval_index) {
        tpms_sim_open_number_input(app, TPMSSimItemAutoInterval);
        return;
    }

    if(index == TPMSSimItemSave) {
        if(simulation_state.lab_source != TPMSLabSimulationNone) {
            tpms_sim_open_lab_name_input(app);
            return;
        }
        variable_item_set_current_value_text(simulation_state.save_item, "SAVING...");
        const char* protocol = tpms_encoder_name(simulation_state.values.kind);
        const bool ok = tpms_save_edited_raw(app, &simulation_state.values, protocol, NULL);
        variable_item_set_current_value_text(
            simulation_state.save_item, ok ? "SAVED" : "ERROR");
        notification_message(
            app->notifications, ok ? &sequence_blink_green_10 : &sequence_blink_red_10);
        return;
    }

    if(index == TPMSSimItemSend) {
        variable_item_set_current_value_text(simulation_state.send_item, "SENDING");
        const bool ok = tpms_editor_send(app, &simulation_state.values);
        if(ok) {
            variable_item_set_current_value_text(
                simulation_state.send_item, tpms_radio_is_external(app) ? "TX EXT OK" : "TX INT OK");
        } else {
            variable_item_set_current_value_text(simulation_state.send_item, "TX ERROR");
        }
        notification_message(app->notifications, ok ? &sequence_blink_green_10 : &sequence_blink_red_10);
        return;
    }

    if(simulation_state.lab_source != TPMSLabSimulationNone &&
       index == simulation_state.delete_index) {
        bool ok = false;
        if(simulation_state.lab_source == TPMSLabSimulationSession) {
            ok = tpms_lab_session_delete(simulation_state.lab_session_slot);
        } else if(simulation_state.lab_source == TPMSLabSimulationSaved &&
                  simulation_state.lab_saved_path[0]) {
            ok = tpms_lab_saved_delete(simulation_state.lab_saved_path);
        }
        variable_item_set_current_value_text(
            simulation_state.delete_item, ok ? "DELETED" : "ERROR");
        notification_message(app->notifications, ok ? &sequence_blink_green_10 : &sequence_blink_red_10);
        if(ok) scene_manager_previous_scene(app->scene_manager);
        return;
    }
}

void tpms_scene_simulation_on_enter(void* context) {
    TPMSApp* app = context;
    if(!app || !app->variable_item_list) return;
    if(app->txrx->txrx_state == TPMSTxRxStateRx) tpms_rx_end(app);

    const bool use_prefill = app->simulation_prefill_valid;
    TpmsEditValues prefill_values = {0};
    if(use_prefill) prefill_values = app->simulation_prefill_values;
    const uint8_t lab_source = use_prefill ? app->simulation_lab_source : TPMSLabSimulationNone;
    const uint8_t lab_session_slot = app->simulation_lab_session_slot;
    char lab_saved_path[128] = {0};
    if(use_prefill && app->simulation_lab_path[0])
        snprintf(lab_saved_path, sizeof(lab_saved_path), "%s", app->simulation_lab_path);
    app->simulation_prefill_valid = false;
    app->simulation_lab_source = TPMSLabSimulationNone;
    app->simulation_lab_path[0] = '\0';

    memset(&simulation_state, 0, sizeof(simulation_state));
    simulation_state.lab_source = lab_source;
    simulation_state.lab_session_slot = lab_session_slot;
    snprintf(simulation_state.lab_saved_path, sizeof(simulation_state.lab_saved_path), "%s", lab_saved_path);
    simulation_state.delete_index = UINT32_MAX;
    simulation_state.auto_tx_interval_s = 5U;
    tpms_sim_id_input_open = false;
    tpms_sim_flags_input_open = false;
    tpms_sim_number_input_open = false;
    tpms_sim_lab_name_input_open = false;
    if(use_prefill) simulation_state.values = prefill_values;
    else tpms_encoder_default_values(tpms_encoder_test_kind(0U), &simulation_state.values);

    VariableItemList* list = app->variable_item_list;
    variable_item_list_reset(list);

    simulation_state.protocol_item = variable_item_list_add(
        list, "Protocol", (uint8_t)tpms_encoder_test_count(), tpms_sim_protocol_changed, app);
    uint8_t protocol_index = 0U;
    if(use_prefill) {
        for(size_t i = 0U; i < tpms_encoder_test_count(); i++) {
            if(tpms_encoder_test_kind(i) == simulation_state.values.kind) {
                protocol_index = (uint8_t)i;
                break;
            }
        }
    }
    variable_item_set_current_value_index(simulation_state.protocol_item, protocol_index);

    simulation_state.id_item = variable_item_list_add(list, "Sensor ID", 1U, NULL, app);

    simulation_state.pressure_item =
        variable_item_list_add(list, "Pressure", 1U, NULL, app);
    simulation_state.temperature_item =
        variable_item_list_add(list, "Temperature", 1U, NULL, app);
    simulation_state.flags_item = variable_item_list_add(list, "Flags", 1U, NULL, app);
    simulation_state.repeat_item = variable_item_list_add(list, "Repeats", 2U, tpms_sim_repeat_changed, app);
    simulation_state.save_item = variable_item_list_add(
        list, simulation_state.lab_source ? "Save ID" : "Save", 1U, NULL, app);
    simulation_state.send_item = variable_item_list_add(list, "Send", 1U, NULL, app);
    uint32_t next_index = TPMSSimItemSend + 1U;
    if(simulation_state.lab_source != TPMSLabSimulationNone) {
        simulation_state.delete_index = next_index++;
        simulation_state.delete_item = variable_item_list_add(list, "Delete", 1U, NULL, app);
    }
    simulation_state.auto_interval_index = next_index++;
    simulation_state.auto_interval_item = variable_item_list_add(
        list, "Auto TX interval", TPMS_AUTO_TX_PRESET_COUNT + 1U, tpms_sim_auto_interval_changed, app);
    variable_item_set_current_value_index(simulation_state.auto_interval_item, 2U);
    variable_item_set_current_value_text(simulation_state.auto_interval_item, "5 s");
    simulation_state.auto_toggle_index = next_index;
    simulation_state.auto_toggle_item =
        variable_item_list_add(list, "Auto TX", 2U, tpms_sim_auto_toggle_changed, app);
    tpms_sim_set_auto_toggle_text(false);

    tpms_sim_refresh_values();
    variable_item_list_set_enter_callback(list, tpms_sim_enter_callback, app);
    variable_item_list_set_selected_item(list, TPMSSimItemProtocol);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
}

bool tpms_scene_simulation_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    if(!app) return false;
    if(event.type == SceneManagerEventTypeBack && tpms_sim_lab_name_input_open) {
        tpms_sim_lab_name_input_open = false;
        variable_item_list_set_selected_item(app->variable_item_list, TPMSSimItemSave);
        view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
        return true;
    }
    if(event.type == SceneManagerEventTypeBack && tpms_sim_id_input_open) {
        tpms_sim_id_input_open = false;
        variable_item_list_set_selected_item(app->variable_item_list, TPMSSimItemId);
        view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
        return true;
    }
    if(event.type == SceneManagerEventTypeBack && tpms_sim_flags_input_open) {
        tpms_sim_flags_input_open = false;
        variable_item_list_set_selected_item(app->variable_item_list, TPMSSimItemFlags);
        view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
        return true;
    }
    if(event.type == SceneManagerEventTypeBack && tpms_sim_number_input_open) {
        tpms_sim_number_input_open = false;
        variable_item_list_set_selected_item(
            app->variable_item_list,
            tpms_sim_number_target == TPMSSimItemAutoInterval ?
                simulation_state.auto_interval_index : tpms_sim_number_target);
        view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
        return true;
    }
    if(event.type == SceneManagerEventTypeBack && simulation_state.auto_tx_enabled) {
        tpms_sim_stop_auto_tx();
        return true;
    }
    if(event.type == SceneManagerEventTypeTick && simulation_state.auto_tx_enabled) {
        const uint32_t interval_ds = (uint32_t)simulation_state.auto_tx_interval_s * 10U;
        if(simulation_state.auto_tx_elapsed_ds < interval_ds) simulation_state.auto_tx_elapsed_ds++;
        if(simulation_state.auto_tx_elapsed_ds >= interval_ds) {
            simulation_state.auto_tx_elapsed_ds = 0U;
            const bool ok = tpms_editor_send(app, &simulation_state.values);
            if(ok) {
                simulation_state.auto_tx_count++;
                tpms_sim_set_auto_toggle_text(true);
                variable_item_set_current_value_text(
                    simulation_state.send_item, tpms_radio_is_external(app) ? "AUTO EXT OK" : "AUTO INT OK");
                notification_message(app->notifications, &sequence_blink_green_10);
            } else {
                tpms_sim_stop_auto_tx();
                variable_item_set_current_value_text(simulation_state.auto_toggle_item, "ERROR");
                variable_item_set_current_value_text(simulation_state.send_item, "TX ERROR");
                notification_message(app->notifications, &sequence_blink_red_10);
            }
        }
        return true;
    }
    return false;
}

void tpms_scene_simulation_on_exit(void* context) {
    TPMSApp* app = context;
    if(!app || !app->variable_item_list) return;
    tpms_sim_stop_auto_tx();
    tpms_sim_id_input_open = false;
    tpms_sim_flags_input_open = false;
    tpms_sim_number_input_open = false;
    tpms_sim_lab_name_input_open = false;
    variable_item_list_set_selected_item(app->variable_item_list, 0U);
    variable_item_list_reset(app->variable_item_list);
    tpms_release_text_input(app);
    tpms_release_number_input(app);
    memset(&simulation_state, 0, sizeof(simulation_state));
}
