#include "../tpms_app_i.h"
#include "../protocols/tpms_generic.h"
#include "../tpms_editor_tx.h"
#include "../tpms_encoder.h"
#include "../tpms_save.h"

#include <flipper_format/flipper_format_i.h>
#include <stdio.h>
#include <string.h>

#define TAG "TPMSEditor6"

#define TPMS_EDITOR_ID_TEXT_SIZE 11U
#define TPMS_EDITOR_FLAGS_TEXT_SIZE 4U
#define TPMS_EDITOR_SAVE_NAME_SIZE 40U
#define TPMS_EDITOR_PRESSURE_MIN_KPA 0
#define TPMS_EDITOR_PRESSURE_MAX_KPA 700
#define TPMS_EDITOR_TEMP_MIN_C (-50)
#define TPMS_EDITOR_TEMP_MAX_C 150
#define TPMS_AUTO_TX_INTERVAL_MIN_S 1
#define TPMS_AUTO_TX_INTERVAL_MAX_S 3600
#define TPMS_AUTO_TX_PRESET_COUNT 6U

typedef enum {
    TPMSEditorItemProtocol = 0,
    TPMSEditorItemId,
    TPMSEditorItemPressure,
    TPMSEditorItemTemperature,
    TPMSEditorItemFlags,
    TPMSEditorItemRepeat,
    TPMSEditorItemSaveSignal,
    TPMSEditorItemSaveModified,
    TPMSEditorItemSend,
    TPMSEditorItemAutoInterval,
    TPMSEditorItemAutoToggle,
} TPMSEditorItem;

typedef struct {
    TpmsEditValues values;
    TpmsEditValues original_values;
    char protocol[48];
    VariableItem* id_item;
    VariableItem* pressure_item;
    VariableItem* temperature_item;
    VariableItem* flags_item;
    VariableItem* save_signal_item;
    VariableItem* save_modified_item;
    VariableItem* send_item;
    VariableItem* auto_interval_item;
    VariableItem* auto_toggle_item;
    uint16_t auto_tx_interval_s;
    uint32_t auto_tx_elapsed_ds;
    uint32_t auto_tx_count;
    bool auto_tx_enabled;
    bool auto_tx_custom;
    bool data_loaded;
    bool encoder_available;
    bool source_profile;
} TPMSEditorState;

static TPMSEditorState editor_state;
static char tpms_editor_id_text[TPMS_EDITOR_ID_TEXT_SIZE];
static char tpms_editor_flags_text[TPMS_EDITOR_FLAGS_TEXT_SIZE];
static char tpms_editor_save_name[TPMS_EDITOR_SAVE_NAME_SIZE];
static bool tpms_editor_id_input_open = false;
static bool tpms_editor_flags_input_open = false;
static bool tpms_editor_save_name_input_open = false;
static bool tpms_editor_number_input_open = false;
static uint32_t tpms_editor_number_target = TPMSEditorItemPressure;

static const uint16_t tpms_auto_tx_presets_s[TPMS_AUTO_TX_PRESET_COUNT] = {1U, 2U, 5U, 10U, 30U, 60U};
static const char* const tpms_auto_tx_preset_text[TPMS_AUTO_TX_PRESET_COUNT] = {
    "1 s", "2 s", "5 s", "10 s", "30 s", "60 s"};

/*
 * TPMS 4.0 quick-save keyboard helper.
 *
 * The public TextInput API intentionally exposes no selected-key setter. Both
 * ARF and Momentum currently use the standard Flipper TextInput model prefix.
 * After text_input_set_result_callback() sees a non-empty default it focuses
 * Save; changing row 2/column 8 to row 1/column 9 selects Backspace.
 *
 * clear_default_text=true is used below, so one OK on Backspace clears the
 * entire generated default name, not only its last character.
 */
typedef struct {
    const char* header;
    char* text_buffer;
    size_t text_buffer_size;
    size_t minimum_length;
    bool clear_default_text;
    TextInputCallback callback;
    void* callback_context;
    uint8_t selected_row;
    uint8_t selected_column;
} TPMSTextInputModelPrefix;

static void tpms_editor_text_input_focus_backspace(TextInput* text_input) {
    if(!text_input) return;
    View* view = text_input_get_view(text_input);
    if(!view) return;
    TPMSTextInputModelPrefix* model = view_get_model(view);
    if(!model) return;
    model->selected_row = 1U;
    model->selected_column = 9U;
    view_commit_model(view, true);
}

static void tpms_editor_make_default_save_name(char* out, size_t out_size) {
    if(!out || out_size < 2U) return;
    out[0] = '\0';

    const char* source = tpms_protocol_list_name(editor_state.protocol);
    size_t j = 0U;
    bool last_sep = false;
    for(size_t i = 0U; source && source[i] && j + 10U < out_size; i++) {
        unsigned char c = (unsigned char)source[i];
        if(c >= 'a' && c <= 'z') c = (unsigned char)(c - ('a' - 'A'));
        if((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            out[j++] = (char)c;
            last_sep = false;
        } else if(j > 0U && !last_sep) {
            out[j++] = '_';
            last_sep = true;
        }
    }
    while(j > 0U && out[j - 1U] == '_') j--;
    if(j == 0U) {
        const char fallback[] = "TPMS";
        for(size_t i = 0U; fallback[i] && j + 1U < out_size; i++) out[j++] = fallback[i];
    }
    if(j + 9U < out_size) {
        out[j++] = '_';
        snprintf(out + j, out_size - j, "%08lX", (unsigned long)editor_state.original_values.id);
    } else {
        out[j] = '\0';
    }
}

static void tpms_editor_set_auto_toggle_text(bool enabled) {
    if(!editor_state.auto_toggle_item) return;
    variable_item_set_current_value_index(editor_state.auto_toggle_item, enabled ? 1U : 0U);
    if(enabled) {
        char text[20];
        snprintf(text, sizeof(text), "ON %lu", (unsigned long)editor_state.auto_tx_count);
        variable_item_set_current_value_text(editor_state.auto_toggle_item, text);
    } else {
        variable_item_set_current_value_text(editor_state.auto_toggle_item, "OFF");
    }
}

static void tpms_editor_stop_auto_tx(void) {
    editor_state.auto_tx_enabled = false;
    editor_state.auto_tx_elapsed_ds = 0U;
    tpms_editor_set_auto_toggle_text(false);
}

static void tpms_editor_auto_interval_changed(VariableItem* item) {
    if(!item) return;
    uint8_t index = variable_item_get_current_value_index(item);
    if(index < TPMS_AUTO_TX_PRESET_COUNT) {
        editor_state.auto_tx_interval_s = tpms_auto_tx_presets_s[index];
        editor_state.auto_tx_custom = false;
        variable_item_set_current_value_text(item, tpms_auto_tx_preset_text[index]);
    } else {
        editor_state.auto_tx_custom = true;
        variable_item_set_current_value_text(item, "Custom");
    }
    if(editor_state.auto_tx_enabled) editor_state.auto_tx_elapsed_ds = 0U;
}

static void tpms_editor_auto_toggle_changed(VariableItem* item) {
    if(!item) return;
    uint8_t index = variable_item_get_current_value_index(item);
    if(index > 1U || !editor_state.encoder_available) index = 0U;
    editor_state.auto_tx_enabled = index == 1U;
    editor_state.auto_tx_count = 0U;
    editor_state.auto_tx_elapsed_ds = editor_state.auto_tx_enabled ?
                                           (uint32_t)editor_state.auto_tx_interval_s * 10U :
                                           0U;
    tpms_editor_set_auto_toggle_text(editor_state.auto_tx_enabled);
}

static void tpms_editor_format_pressure(VariableItem* item, float pressure_kpa) {
    char text[20];
    snprintf(text, sizeof(text), "%.2f bar", (double)(pressure_kpa / 100.0f));
    variable_item_set_current_value_text(item, text);
}

static void tpms_editor_format_temperature(VariableItem* item, float temperature_c) {
    char text[16];
    snprintf(text, sizeof(text), "%d C", (int)temperature_c);
    variable_item_set_current_value_text(item, text);
}

static void tpms_editor_repeat_changed(VariableItem* item) {
    const uint8_t index = variable_item_get_current_value_index(item);
    editor_state.values.repeat_count = index ? 10U : 3U;
    variable_item_set_current_value_text(item, index ? "10x" : "3x");
}

static int8_t tpms_editor_hex_value(char c) {
    if(c >= '0' && c <= '9') return (int8_t)(c - '0');
    if(c >= 'A' && c <= 'F') return (int8_t)(c - 'A' + 10);
    if(c >= 'a' && c <= 'f') return (int8_t)(c - 'a' + 10);
    return -1;
}

static bool tpms_editor_parse_id(const char* text, uint32_t* value) {
    if(!text || !value) return false;
    if(text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text += 2;
    const size_t len = strlen(text);
    if(len == 0U || len > 8U) return false;
    uint32_t parsed = 0U;
    for(size_t i = 0U; i < len; i++) {
        const int8_t nibble = tpms_editor_hex_value(text[i]);
        if(nibble < 0) return false;
        parsed = (parsed << 4U) | (uint32_t)nibble;
    }
    *value = parsed;
    return true;
}

static bool tpms_editor_id_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    uint32_t value = 0U;
    if(!tpms_editor_parse_id(text, &value)) {
        furi_string_set(error, "Enter 1-8 HEX chars: 0-9, A-F");
        return false;
    }
    return true;
}

static void tpms_editor_set_id_text(void) {
    if(!editor_state.id_item) return;
    char text[16];
    snprintf(text, sizeof(text), "0x%08lX", (unsigned long)editor_state.values.id);
    variable_item_set_current_value_text(editor_state.id_item, text);
}

static void tpms_editor_id_done(void* context) {
    TPMSApp* app = context;
    if(!app) return;
    uint32_t value = editor_state.values.id;
    if(tpms_editor_parse_id(tpms_editor_id_text, &value)) {
        if(value != editor_state.values.id) editor_state.values.edit_mask |= TPMS_EDIT_ID;
        editor_state.values.id = value;
    }
    tpms_editor_id_input_open = false;
    tpms_editor_set_id_text();
    variable_item_list_set_selected_item(app->variable_item_list, TPMSEditorItemId);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
}

static void tpms_editor_open_id_input(TPMSApp* app) {
    if(!app || !editor_state.encoder_available || !tpms_ensure_text_input(app)) return;
    tpms_editor_stop_auto_tx();
    snprintf(tpms_editor_id_text, sizeof(tpms_editor_id_text), "%08lX", (unsigned long)editor_state.values.id);
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Sensor ID HEX (0-9 A-F)");
    text_input_set_minimum_length(app->text_input, 1U);
    text_input_set_validator(app->text_input, tpms_editor_id_validator, app);
    text_input_set_result_callback(
        app->text_input,
        tpms_editor_id_done,
        app,
        tpms_editor_id_text,
        sizeof(tpms_editor_id_text),
        false);
    tpms_editor_id_input_open = true;
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewTextInput);
}

static bool tpms_editor_flags_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    uint32_t value = 0U;
    if(!tpms_editor_parse_id(text, &value) || value > 0xFFU) {
        furi_string_set(error, "Enter HEX 00-FF");
        return false;
    }
    return true;
}

static void tpms_editor_flags_done(void* context) {
    TPMSApp* app = context;
    if(!app) return;
    uint32_t low = editor_state.values.flags & 0xFFU;
    if(tpms_editor_parse_id(tpms_editor_flags_text, &low) && low <= 0xFFU) {
        editor_state.values.flags =
            (editor_state.values.flags & 0xFFFFFF00U) | (low & 0xFFU);
        editor_state.values.edit_mask |= TPMS_EDIT_FLAGS;
    }
    tpms_editor_flags_input_open = false;
    if(editor_state.flags_item) {
        char text[16];
        snprintf(text, sizeof(text), "0x%08lX", (unsigned long)editor_state.values.flags);
        variable_item_set_current_value_text(editor_state.flags_item, text);
    }
    variable_item_list_set_selected_item(app->variable_item_list, TPMSEditorItemFlags);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
}

static void tpms_editor_open_flags_input(TPMSApp* app) {
    if(!app || !editor_state.encoder_available || !editor_state.flags_item ||
       !tpms_ensure_text_input(app)) return;
    tpms_editor_stop_auto_tx();
    snprintf(
        tpms_editor_flags_text,
        sizeof(tpms_editor_flags_text),
        "%02lX",
        (unsigned long)(editor_state.values.flags & 0xFFU));
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Flags HEX 00-FF");
    text_input_set_minimum_length(app->text_input, 1U);
    text_input_set_validator(app->text_input, tpms_editor_flags_validator, app);
    text_input_set_result_callback(
        app->text_input,
        tpms_editor_flags_done,
        app,
        tpms_editor_flags_text,
        sizeof(tpms_editor_flags_text),
        false);
    tpms_editor_flags_input_open = true;
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewTextInput);
}

static void tpms_editor_number_done(void* context, int32_t number) {
    TPMSApp* app = context;
    if(!app) return;
    if(tpms_editor_number_target == TPMSEditorItemPressure) {
        if(number < TPMS_EDITOR_PRESSURE_MIN_KPA) number = TPMS_EDITOR_PRESSURE_MIN_KPA;
        if(number > TPMS_EDITOR_PRESSURE_MAX_KPA) number = TPMS_EDITOR_PRESSURE_MAX_KPA;
        editor_state.values.pressure_kpa = (float)number;
        editor_state.values.edit_mask |= TPMS_EDIT_PRESSURE;
        if(editor_state.pressure_item)
            tpms_editor_format_pressure(editor_state.pressure_item, editor_state.values.pressure_kpa);
    } else if(tpms_editor_number_target == TPMSEditorItemTemperature) {
        if(number < TPMS_EDITOR_TEMP_MIN_C) number = TPMS_EDITOR_TEMP_MIN_C;
        if(number > TPMS_EDITOR_TEMP_MAX_C) number = TPMS_EDITOR_TEMP_MAX_C;
        editor_state.values.temperature_c = (float)number;
        editor_state.values.edit_mask |= TPMS_EDIT_TEMPERATURE;
        if(editor_state.values.kind == TpmsEncoderSchraderEG53MA4)
            editor_state.values.temperature_raw_f_valid = false;
        if(editor_state.temperature_item)
            tpms_editor_format_temperature(
                editor_state.temperature_item, editor_state.values.temperature_c);
    } else if(tpms_editor_number_target == TPMSEditorItemAutoInterval) {
        if(number < TPMS_AUTO_TX_INTERVAL_MIN_S) number = TPMS_AUTO_TX_INTERVAL_MIN_S;
        if(number > TPMS_AUTO_TX_INTERVAL_MAX_S) number = TPMS_AUTO_TX_INTERVAL_MAX_S;
        editor_state.auto_tx_interval_s = (uint16_t)number;
        editor_state.auto_tx_custom = true;
        if(editor_state.auto_interval_item) {
            char text[20];
            variable_item_set_current_value_index(
                editor_state.auto_interval_item, TPMS_AUTO_TX_PRESET_COUNT);
            snprintf(text, sizeof(text), "%u s", editor_state.auto_tx_interval_s);
            variable_item_set_current_value_text(editor_state.auto_interval_item, text);
        }
    }
    tpms_editor_number_input_open = false;
    variable_item_list_set_selected_item(app->variable_item_list, tpms_editor_number_target);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
}

static void tpms_editor_open_number_input(TPMSApp* app, uint32_t target) {
    if(!app || !editor_state.encoder_available || !tpms_ensure_number_input(app)) return;
    tpms_editor_stop_auto_tx();
    tpms_editor_number_target = target;
    int32_t current = 0;
    int32_t min = 0;
    int32_t max = 0;
    const char* header = "Value";
    if(target == TPMSEditorItemPressure) {
        current = (int32_t)(editor_state.values.pressure_kpa + 0.5f);
        min = TPMS_EDITOR_PRESSURE_MIN_KPA;
        max = TPMS_EDITOR_PRESSURE_MAX_KPA;
        header = "Pressure x0.01 bar";
    } else if(target == TPMSEditorItemTemperature) {
        current = (int32_t)(editor_state.values.temperature_c +
                            (editor_state.values.temperature_c >= 0.0f ? 0.5f : -0.5f));
        min = TPMS_EDITOR_TEMP_MIN_C;
        max = TPMS_EDITOR_TEMP_MAX_C;
        header = "Temperature C";
    } else if(target == TPMSEditorItemAutoInterval) {
        current = (int32_t)editor_state.auto_tx_interval_s;
        min = TPMS_AUTO_TX_INTERVAL_MIN_S;
        max = TPMS_AUTO_TX_INTERVAL_MAX_S;
        header = "Auto TX seconds";
    } else {
        return;
    }
    number_input_set_header_text(app->number_input, header);
    number_input_set_result_callback(
        app->number_input, tpms_editor_number_done, app, current, min, max);
    tpms_editor_number_input_open = true;
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewNumberInput);
}

static bool tpms_editor_save_name_validator(const char* text, FuriString* error, void* context) {
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

static void tpms_editor_save_named_done(void* context) {
    TPMSApp* app = context;
    if(!app) return;
    tpms_editor_save_name_input_open = false;

    variable_item_set_current_value_text(editor_state.save_signal_item, "SAVING...");
    bool truncated = false;
    bool ok = tpms_save_original_raw_named(
        app,
        app->txrx->idx_menu_chosen,
        editor_state.protocol,
        editor_state.original_values.id,
        tpms_editor_save_name,
        NULL,
        &truncated);
    if(ok)
        ok = tpms_profile_save_named(
            &editor_state.original_values,
            editor_state.protocol,
            false,
            tpms_editor_save_name,
            NULL);

    variable_item_set_current_value_text(
        editor_state.save_signal_item,
        ok ? (truncated ? "SAVED TRUNC" : "SAVED") : "ERROR");
    notification_message(
        app->notifications, ok ? &sequence_blink_green_10 : &sequence_blink_red_10);
    variable_item_list_set_selected_item(app->variable_item_list, TPMSEditorItemSaveSignal);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
}

static void tpms_editor_open_save_name_input(TPMSApp* app) {
    if(!app || !tpms_ensure_text_input(app)) return;
    tpms_editor_stop_auto_tx();
    tpms_editor_make_default_save_name(tpms_editor_save_name, sizeof(tpms_editor_save_name));
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Save name");
    text_input_set_minimum_length(app->text_input, 1U);
    text_input_set_validator(app->text_input, tpms_editor_save_name_validator, app);
    text_input_set_result_callback(
        app->text_input,
        tpms_editor_save_named_done,
        app,
        tpms_editor_save_name,
        sizeof(tpms_editor_save_name),
        true);
    tpms_editor_text_input_focus_backspace(app->text_input);
    tpms_editor_save_name_input_open = true;
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewTextInput);
}

static void tpms_editor_enter_callback(void* context, uint32_t index) {
    TPMSApp* app = context;

    if(index == TPMSEditorItemId) {
        tpms_editor_open_id_input(app);
        return;
    }
    if(index == TPMSEditorItemPressure) {
        tpms_editor_open_number_input(app, TPMSEditorItemPressure);
        return;
    }
    if(index == TPMSEditorItemTemperature && editor_state.encoder_available &&
       tpms_encoder_has_temperature(editor_state.values.kind)) {
        tpms_editor_open_number_input(app, TPMSEditorItemTemperature);
        return;
    }
    if(index == TPMSEditorItemFlags && editor_state.encoder_available &&
       tpms_encoder_flags_editable(editor_state.values.kind)) {
        tpms_editor_open_flags_input(app);
        return;
    }
    if(index == TPMSEditorItemAutoInterval && editor_state.encoder_available) {
        tpms_editor_open_number_input(app, TPMSEditorItemAutoInterval);
        return;
    }

    if(index == TPMSEditorItemSaveSignal) {
        if(editor_state.source_profile) {
            variable_item_set_current_value_text(editor_state.save_signal_item, "NO RX");
            notification_message(app->notifications, &sequence_blink_red_10);
            return;
        }
        tpms_editor_open_save_name_input(app);
        return;
    }

    if(index == TPMSEditorItemSaveModified) {
        if(!editor_state.encoder_available) {
            variable_item_set_current_value_text(editor_state.save_modified_item, "RX ONLY");
            notification_message(app->notifications, &sequence_blink_red_10);
            return;
        }
        variable_item_set_current_value_text(editor_state.save_modified_item, "SAVING...");
        const bool ok = tpms_save_edited_raw(
            app, &editor_state.values, editor_state.protocol, NULL);
        variable_item_set_current_value_text(
            editor_state.save_modified_item, ok ? "SAVED" : "ERROR");
        notification_message(
            app->notifications, ok ? &sequence_blink_green_10 : &sequence_blink_red_10);
        return;
    }

    if(index != TPMSEditorItemSend) return;

    if(!editor_state.encoder_available) {
        variable_item_set_current_value_text(editor_state.send_item, "RX ONLY");
        notification_message(app->notifications, &sequence_blink_red_10);
        return;
    }

    variable_item_set_current_value_text(editor_state.send_item, "SENDING");
    const bool ok = tpms_editor_send(app, &editor_state.values);
    if(ok) {
        variable_item_set_current_value_text(
            editor_state.send_item,
            tpms_radio_is_external(app) ? "TX EXT OK" : "TX INT OK");
    } else {
        variable_item_set_current_value_text(editor_state.send_item, "TX ERROR");
    }
    notification_message(
        app->notifications, ok ? &sequence_blink_green_10 : &sequence_blink_red_10);
}

static bool tpms_editor_load_selected(TPMSApp* app) {
    if(app->editor_profile_loaded) {
        editor_state.values = app->editor_profile_values;
        editor_state.original_values = app->editor_profile_values;
        snprintf(editor_state.protocol, sizeof(editor_state.protocol), "%s", app->editor_profile_protocol);
        editor_state.encoder_available = editor_state.values.kind != TpmsEncoderNone;
        editor_state.source_profile = true;
        return editor_state.encoder_available;
    }
    if(app->txrx->idx_menu_chosen >= tpms_history_get_item(app->txrx->history)) return false;

    FlipperFormat* fff = tpms_history_get_raw_data(app->txrx->history, app->txrx->idx_menu_chosen);
    if(!fff) return false;

    FuriString* protocol = furi_string_alloc();
    bool ok = false;
    TPMSBlockGeneric generic = {0};

    do {
        if(!flipper_format_rewind(fff)) break;
        if(!flipper_format_read_string(fff, "Protocol", protocol)) break;
        if(tpms_block_generic_deserialize(&generic, fff) != SubGhzProtocolStatusOk) break;

        snprintf(
            editor_state.protocol,
            sizeof(editor_state.protocol),
            "%s",
            furi_string_get_cstr(protocol));
        editor_state.values.kind = tpms_encoder_for_model(furi_string_get_cstr(protocol));
        editor_state.encoder_available = editor_state.values.kind != TpmsEncoderNone;
        editor_state.values.id = generic.id;
        editor_state.values.pressure_kpa = generic.pressure * 100.0f;
        editor_state.values.temperature_c = generic.temperature;
        editor_state.values.frequency_hz =
            tpms_history_get_frequency(app->txrx->history, app->txrx->idx_menu_chosen);
        editor_state.values.radio_preset = tpms_radio_preset_from_name(
            tpms_history_get_preset(app->txrx->history, app->txrx->idx_menu_chosen));
        editor_state.values.rssi_dbm = -127;
        editor_state.values.rssi_valid = tpms_history_get_frame_rssi(
            app->txrx->history,
            app->txrx->idx_menu_chosen,
            &editor_state.values.rssi_dbm);
        editor_state.values.wake_profile = TpmsWakeProfileNone;
        editor_state.values.wake_profile_valid = tpms_history_get_wake_profile(
            app->txrx->history,
            app->txrx->idx_menu_chosen,
            &editor_state.values.wake_profile);
        editor_state.values.repeat_count = 3U;
        if(editor_state.values.kind == TpmsEncoderSchraderEG53MA4) {
            editor_state.values.flags = (uint32_t)(generic.data >> 32U);
            editor_state.values.temperature_raw_f = (uint8_t)((generic.data >> 24U) & 0xFFU);
            editor_state.values.temperature_raw_f_valid = true;
            editor_state.values.temperature_c =
                ((float)editor_state.values.temperature_raw_f - 32.0f) * (5.0f / 9.0f);
        } else {
            editor_state.values.flags = (uint32_t)((generic.data >> 48U) & 0xFFFFU);
            editor_state.values.temperature_raw_f = 0U;
            editor_state.values.temperature_raw_f_valid = false;
        }
        editor_state.values.edit_mask = 0U;
        editor_state.values.raw_payload_size = generic.raw_payload_size;
        editor_state.values.raw_payload_bits = generic.raw_payload_bits;
        editor_state.values.raw_payload_valid = generic.raw_payload_valid;
        if(generic.raw_payload_valid && generic.raw_payload_size > 0U) {
            memcpy(
                editor_state.values.raw_payload,
                generic.raw_payload,
                generic.raw_payload_size);
        }
        editor_state.original_values = editor_state.values;
        editor_state.source_profile = false;
        ok = true;
    } while(false);

    furi_string_free(protocol);
    return ok;
}

void tpms_scene_editor_on_enter(void* context) {
    TPMSApp* app = context;
    if(app->txrx->txrx_state == TPMSTxRxStateRx) tpms_rx_end(app);

    memset(&editor_state, 0, sizeof(editor_state));
    editor_state.auto_tx_interval_s = 5U;
    tpms_editor_id_input_open = false;
    furi_mutex_acquire(app->history_mutex, FuriWaitForever);
    editor_state.data_loaded = tpms_editor_load_selected(app);
    furi_mutex_release(app->history_mutex);
    if(!editor_state.data_loaded) {
        snprintf(editor_state.protocol, sizeof(editor_state.protocol), "RX DATA ERROR");
        editor_state.values.kind = TpmsEncoderNone;
        editor_state.encoder_available = false;
    }

    VariableItem* item;
    char text[24];

    item = variable_item_list_add(app->variable_item_list, "Protocol", 1U, NULL, app);
    variable_item_set_current_value_text(item, tpms_protocol_display_name(editor_state.protocol));

    editor_state.id_item = variable_item_list_add(app->variable_item_list, "Sensor ID", 1U, NULL, app);
    tpms_editor_set_id_text();

    editor_state.pressure_item =
        variable_item_list_add(app->variable_item_list, "Pressure", 1U, NULL, app);
    tpms_editor_format_pressure(editor_state.pressure_item, editor_state.values.pressure_kpa);

    editor_state.temperature_item =
        variable_item_list_add(app->variable_item_list, "Temperature", 1U, NULL, app);
    if(editor_state.values.kind == TpmsEncoderFord &&
       editor_state.values.raw_payload_valid && editor_state.values.raw_payload_size >= 6U) {
        /* TPMS 4.4: Ford TT/FF observations show two states where a numeric
           temperature must not be presented: TT MSB=1 or FF ending in 0xB.
           Raw TT/FF stay visible below, so no diagnostic information is lost. */
        const uint8_t tt = editor_state.values.raw_payload[5];
        const uint8_t ff = editor_state.values.raw_payload_size >= 7U ?
                               editor_state.values.raw_payload[6] : 0U;
        const bool temperature_valid =
            ((tt & 0x80U) == 0U) && ((ff & 0x0FU) != 0x0BU);
        if(!temperature_valid) {
            variable_item_set_current_value_text(editor_state.temperature_item, "--");
        } else {
            const int temp_c = (int)tt - 56;
            snprintf(text, sizeof(text), temp_c < -40 ? "%d C ?" : "%d C", temp_c);
            variable_item_set_current_value_text(editor_state.temperature_item, text);
        }
    } else if(editor_state.encoder_available && tpms_encoder_has_temperature(editor_state.values.kind)) {
        tpms_editor_format_temperature(
            editor_state.temperature_item, editor_state.values.temperature_c);
    } else if(editor_state.encoder_available) {
        variable_item_set_current_value_text(editor_state.temperature_item, "none");
    } else {
        tpms_editor_format_temperature(
            editor_state.temperature_item, editor_state.values.temperature_c);
    }

    const char* diagnostic_label = "Flags";
    if(editor_state.values.kind == TpmsEncoderFord) diagnostic_label = "TT / FF";
    if(editor_state.values.kind == TpmsEncoderCitroen) diagnostic_label = "State / scale";
    if(editor_state.values.kind == TpmsEncoderNissan) diagnostic_label = "Mode";
    editor_state.flags_item = variable_item_list_add(
        app->variable_item_list, diagnostic_label, 1U, NULL, app);
    if(tpms_encoder_flags_editable(editor_state.values.kind)) {
        // for editable flags show all eight characters... unless it makes no real sense
        if(editor_state.values.kind == TpmsEncoderNissan) {
            snprintf(text, sizeof(text), "%u%u%u",
               (unsigned)(editor_state.values.flags>>2)&1U,
               (unsigned)(editor_state.values.flags>>1)&1U,
               (unsigned)(editor_state.values.flags)&1U);
        } else {
                snprintf(text, sizeof(text), "0x%08lX", (unsigned long)editor_state.values.flags);
        }
    // non-editable
    } else if (editor_state.values.kind == TpmsEncoderSchraderEG53MA4) {
        snprintf(text, sizeof(text), "0x%08lX", (unsigned long)editor_state.values.flags);
    } else if(editor_state.values.kind == TpmsEncoderFord &&
              editor_state.values.raw_payload_valid &&
              editor_state.values.raw_payload_size >= 7U) {
        snprintf(
            text,
            sizeof(text),
            "%02X%02X",
            editor_state.values.raw_payload[5],
            editor_state.values.raw_payload[6]);
    } else if(editor_state.values.kind == TpmsEncoderFord) {
        snprintf(text, sizeof(text), "0x%04lX", (unsigned long)(editor_state.values.flags & 0xFFFFU));
    } else if(editor_state.values.kind == TpmsEncoderCitroen &&
              editor_state.values.raw_payload_valid &&
              editor_state.values.raw_payload_size >= 1U) {
        snprintf(
            text,
            sizeof(text),
            "%02X  x%u",
            editor_state.values.raw_payload[0],
            editor_state.values.raw_payload[0] == 0xDCU ? 2U : 1U);
    } else if(editor_state.values.kind == TpmsEncoderNissan) {
        snprintf(text, sizeof(text), "%u%u%u",
           (unsigned)(editor_state.values.flags>>2)&1U,
           (unsigned)(editor_state.values.flags>>1)&1U,
           (unsigned)(editor_state.values.flags)&1U);
    } else {
        snprintf(text, sizeof(text), "0x%02X", (unsigned)(editor_state.values.flags & 0xFFU));
    }
    variable_item_set_current_value_text(editor_state.flags_item, text);

    item = variable_item_list_add(
        app->variable_item_list,
        "Repeats",
        editor_state.encoder_available ? 2U : 1U,
        editor_state.encoder_available ? tpms_editor_repeat_changed : NULL,
        app);
    const uint8_t repeat_index = editor_state.values.repeat_count == 10U ? 1U : 0U;
    if(editor_state.encoder_available) editor_state.values.repeat_count = repeat_index ? 10U : 3U;
    variable_item_set_current_value_index(item, repeat_index);
    variable_item_set_current_value_text(
        item, editor_state.encoder_available ? (repeat_index ? "10x" : "3x") : "-");

    editor_state.save_signal_item =
        variable_item_list_add(app->variable_item_list, "Save signal", 1U, NULL, app);
    variable_item_set_current_value_text(
        editor_state.save_signal_item,
        (editor_state.data_loaded && !editor_state.source_profile) ? "OK" : "NO RX");

    editor_state.save_modified_item = variable_item_list_add(
        app->variable_item_list, "Save changes", 1U, NULL, app);
    variable_item_set_current_value_text(
        editor_state.save_modified_item, editor_state.encoder_available ? "OK" : "RX ONLY");

    editor_state.send_item =
        variable_item_list_add(app->variable_item_list, "Send edited", 1U, NULL, app);
    variable_item_set_current_value_text(
        editor_state.send_item, editor_state.encoder_available ? "OK" : "RX ONLY");

    editor_state.auto_interval_item = variable_item_list_add(
        app->variable_item_list,
        "Auto TX interval",
        editor_state.encoder_available ? (TPMS_AUTO_TX_PRESET_COUNT + 1U) : 1U,
        editor_state.encoder_available ? tpms_editor_auto_interval_changed : NULL,
        app);
    if(editor_state.encoder_available) {
        variable_item_set_current_value_index(editor_state.auto_interval_item, 2U);
        variable_item_set_current_value_text(editor_state.auto_interval_item, "5 s");
    } else {
        variable_item_set_current_value_text(editor_state.auto_interval_item, "RX ONLY");
    }

    editor_state.auto_toggle_item = variable_item_list_add(
        app->variable_item_list,
        "Auto TX",
        editor_state.encoder_available ? 2U : 1U,
        editor_state.encoder_available ? tpms_editor_auto_toggle_changed : NULL,
        app);
    tpms_editor_set_auto_toggle_text(false);
    if(!editor_state.encoder_available)
        variable_item_set_current_value_text(editor_state.auto_toggle_item, "RX ONLY");

    /* TPMS 4.5 FIX2: do not append six extra VariableItem rows for Ford.
       Real Ford profiles supplied by the tester showed that the 17-row
       editor could exhaust/fragment the small GUI heap and crash while Back
       destroyed the list. The compact TT/FF row above remains visible; all
       individual bits and TT mode remain losslessly available in RawPayload,
       the saved Ford* profile fields and FORD_DIAG CSV. */

    variable_item_list_set_enter_callback(
        app->variable_item_list, tpms_editor_enter_callback, app);
    variable_item_list_set_selected_item(app->variable_item_list, TPMSEditorItemPressure);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
}

bool tpms_scene_editor_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    if(!app) return false;
    if(event.type == SceneManagerEventTypeBack && tpms_editor_id_input_open) {
        tpms_editor_id_input_open = false;
        variable_item_list_set_selected_item(app->variable_item_list, TPMSEditorItemId);
        view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
        return true;
    }
    if(event.type == SceneManagerEventTypeBack && tpms_editor_save_name_input_open) {
        tpms_editor_save_name_input_open = false;
        variable_item_list_set_selected_item(app->variable_item_list, TPMSEditorItemSaveSignal);
        view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
        return true;
    }
    if(event.type == SceneManagerEventTypeBack && tpms_editor_flags_input_open) {
        tpms_editor_flags_input_open = false;
        variable_item_list_set_selected_item(app->variable_item_list, TPMSEditorItemFlags);
        view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
        return true;
    }
    if(event.type == SceneManagerEventTypeBack && tpms_editor_number_input_open) {
        tpms_editor_number_input_open = false;
        variable_item_list_set_selected_item(app->variable_item_list, tpms_editor_number_target);
        view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
        return true;
    }
    if(event.type == SceneManagerEventTypeBack && editor_state.auto_tx_enabled) {
        tpms_editor_stop_auto_tx();
        return true;
    }
    if(event.type == SceneManagerEventTypeTick && editor_state.auto_tx_enabled) {
        const uint32_t interval_ds = (uint32_t)editor_state.auto_tx_interval_s * 10U;
        if(editor_state.auto_tx_elapsed_ds < interval_ds) editor_state.auto_tx_elapsed_ds++;
        if(editor_state.auto_tx_elapsed_ds >= interval_ds) {
            editor_state.auto_tx_elapsed_ds = 0U;
            const bool ok = tpms_editor_send(app, &editor_state.values);
            if(ok) {
                editor_state.auto_tx_count++;
                tpms_editor_set_auto_toggle_text(true);
                variable_item_set_current_value_text(
                    editor_state.send_item, tpms_radio_is_external(app) ? "AUTO EXT OK" : "AUTO INT OK");
                notification_message(app->notifications, &sequence_blink_green_10);
            } else {
                tpms_editor_stop_auto_tx();
                variable_item_set_current_value_text(editor_state.auto_toggle_item, "ERROR");
                variable_item_set_current_value_text(editor_state.send_item, "TX ERROR");
                notification_message(app->notifications, &sequence_blink_red_10);
            }
        }
        return true;
    }
    return false;
}

void tpms_scene_editor_on_exit(void* context) {
    TPMSApp* app = context;
    tpms_editor_stop_auto_tx();
    tpms_editor_id_input_open = false;
    tpms_editor_flags_input_open = false;
    tpms_editor_save_name_input_open = false;
    tpms_editor_number_input_open = false;
    editor_state.id_item = NULL;
    editor_state.pressure_item = NULL;
    editor_state.temperature_item = NULL;
    editor_state.flags_item = NULL;
    editor_state.save_signal_item = NULL;
    editor_state.save_modified_item = NULL;
    editor_state.send_item = NULL;
    editor_state.auto_interval_item = NULL;
    editor_state.auto_toggle_item = NULL;
    variable_item_list_set_selected_item(app->variable_item_list, 0U);
    variable_item_list_reset(app->variable_item_list);
    tpms_release_text_input(app);
    tpms_release_number_input(app);
    app->editor_profile_loaded = false;
}
