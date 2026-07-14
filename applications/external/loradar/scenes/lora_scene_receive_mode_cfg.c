#include "lora_scene.h"
#include "lora_app.h"
#include "lora_receiver_i.h"
#include <gui/modules/variable_item_list.h>

static void line_sf_cb(VariableItem* item) {
    LoraApp* app = variable_item_get_context(item);
    furi_assert(app);
    char temp[4];
    uint8_t new_sf = variable_item_get_current_value_index(item) + MIN_SF;
    snprintf(temp, sizeof(temp), "%u", new_sf);
    variable_item_set_current_value_text(item, temp);
    with_view_model(
        app->receiver->view, LoraReceiverModel * model, { model->config.sf = new_sf; }, false);
    view_dispatcher_send_custom_event(app->view_dispatcher, LoraReceiverEventCfgSet);
}

static void line_bw_cb(VariableItem* item) {
    LoraApp* app = variable_item_get_context(item);
    furi_assert(app);
    char temp[4];
    uint32_t new_bw = bandwidth_list[variable_item_get_current_value_index(item)];
    snprintf(temp, sizeof(temp), "%lu", new_bw);
    variable_item_set_current_value_text(item, temp);
    with_view_model(
        app->receiver->view, LoraReceiverModel * model, { model->config.bw_idx = new_bw; }, false);
    view_dispatcher_send_custom_event(app->view_dispatcher, LoraReceiverEventCfgSet);
}

static void line_tx_preamble_cb(VariableItem* item) {
    LoraApp* app = variable_item_get_context(item);
    furi_assert(app);
    char temp[4];
    uint8_t new_preamble = variable_item_get_current_value_index(item) + MIN_PREAMBLE;
    snprintf(temp, sizeof(temp), "%u", new_preamble);
    variable_item_set_current_value_text(item, temp);
    /* *INDENT-OFF* */
    with_view_model(
        app->receiver->view,
        LoraReceiverModel * model,
        { model->config.tx_preamble = new_preamble; },
        false);
    /* *INDENT-ON* */
    view_dispatcher_send_custom_event(app->view_dispatcher, LoraReceiverEventCfgSet);
}

static void line_rx_preamble_cb(VariableItem* item) {
    LoraApp* app = variable_item_get_context(item);
    furi_assert(app);
    char temp[4];
    uint8_t new_preamble = variable_item_get_current_value_index(item) + MIN_PREAMBLE;
    snprintf(temp, sizeof(temp), "%u", new_preamble);
    variable_item_set_current_value_text(item, temp);
    /* *INDENT-OFF* */
    with_view_model(
        app->receiver->view,
        LoraReceiverModel * model,
        { model->config.rx_preamble = new_preamble; },
        false);
    /* *INDENT-ON* */
    view_dispatcher_send_custom_event(app->view_dispatcher, LoraReceiverEventCfgSet);
}

static void line_power_cb(VariableItem* item) {
    LoraApp* app = variable_item_get_context(item);
    furi_assert(app);
    char temp[4];
    uint8_t new_power = variable_item_get_current_value_index(item) + MIN_POWER;
    snprintf(temp, sizeof(temp), "%u", new_power);
    variable_item_set_current_value_text(item, temp);
    /* *INDENT-OFF* */
    with_view_model(
        app->receiver->view,
        LoraReceiverModel * model,
        { model->config.power = new_power; },
        false);
    /* *INDENT-ON* */
    view_dispatcher_send_custom_event(app->view_dispatcher, LoraReceiverEventCfgSet);
}

static void line_crc_cb(VariableItem* item) {
    LoraApp* app = variable_item_get_context(item);
    furi_assert(app);
    char temp[10];
    bool new_crc = variable_item_get_current_value_index(item);
    snprintf(temp, sizeof(temp), "%s", new_crc ? "Enabled" : "Disabled");
    variable_item_set_current_value_text(item, temp);
    with_view_model(
        app->receiver->view,
        LoraReceiverModel * model,
        { model->config.with_crc = new_crc; },
        false);
    view_dispatcher_send_custom_event(app->view_dispatcher, LoraReceiverEventCfgSet);
}

static void line_iq_inverted_cb(VariableItem* item) {
    LoraApp* app = variable_item_get_context(item);
    furi_assert(app);
    char temp[10];
    bool new_iq_inverted = variable_item_get_current_value_index(item);
    snprintf(temp, sizeof(temp), "%s", new_iq_inverted ? "Enabled" : "Disabled");
    variable_item_set_current_value_text(item, temp);
    /* *INDENT-OFF* */
    with_view_model(
        app->receiver->view,
        LoraReceiverModel * model,
        { model->config.is_iq_inverted = new_iq_inverted; },
        false);
    /* *INDENT-ON* */
    view_dispatcher_send_custom_event(app->view_dispatcher, LoraReceiverEventCfgSet);
}

static void line_public_lorawan_cb(VariableItem* item) {
    LoraApp* app = variable_item_get_context(item);
    furi_assert(app);
    char temp[10];
    bool new_with_public_lorawan = variable_item_get_current_value_index(item);
    snprintf(temp, sizeof(temp), "%s", new_with_public_lorawan ? "Enabled" : "Disabled");
    variable_item_set_current_value_text(item, temp);
    /* *INDENT-OFF* */
    with_view_model(
        app->receiver->view,
        LoraReceiverModel * model,
        { model->config.with_public_lorawan = new_with_public_lorawan; },
        false);
    /* *INDENT-ON* */
    view_dispatcher_send_custom_event(app->view_dispatcher, LoraReceiverEventCfgSet);
}

static void add_list_value_line(
    LoraApp* app,
    uint8_t value_model_idx,
    const uint32_t list[],
    uint8_t list_size,
    const char* label,
    VariableItemChangeCallback callback) {
    VariableItem* item;
    char temp[4];

    item = variable_item_list_add(app->var_item_list, label, list_size, callback, app);
    variable_item_set_current_value_index(item, value_model_idx);

    snprintf(temp, sizeof(temp), "%lu", list[value_model_idx]);
    variable_item_set_current_value_text(item, temp);
}

static void add_value_line(
    LoraApp* app,
    uint8_t value_model_field,
    uint8_t min_value,
    uint8_t max_value,
    char* label,
    VariableItemChangeCallback callback) {
    VariableItem* item;
    char temp[4];

    item = variable_item_list_add(
        app->var_item_list, label, max_value - min_value + 1, callback, app);
    variable_item_set_current_value_index(item, value_model_field - min_value);

    snprintf(temp, sizeof(temp), "%u", value_model_field);
    variable_item_set_current_value_text(item, temp);
}

static void add_boolean_line(
    LoraApp* app,
    bool boolean_model_field,
    char* boolean_label,
    VariableItemChangeCallback callback) {
    VariableItem* item;
    char temp[10];

    item = variable_item_list_add(app->var_item_list, boolean_label, 2, callback, app);
    variable_item_set_current_value_index(item, boolean_model_field);

    snprintf(temp, sizeof(temp), "%s", boolean_model_field ? "Enabled" : "Disabled");
    variable_item_set_current_value_text(item, temp);
}

void lora_scene_receive_mode_cfg_on_enter(void* context) {
    FURI_LOG_D("LoraSceneReceiveModeCfg", "Entering Lora Scene Receive Mode");
    LoraApp* app = context;
    /* *INDENT-OFF* */
    with_view_model(
        app->receiver->view,
        LoraReceiverModel * model,
        {
            add_value_line(app, model->config.sf, MIN_SF, MAX_SF, "SF", line_sf_cb);
            add_list_value_line(
                app, model->config.bw_idx, bandwidth_list, BANDWIDTH_LIST_SIZE, "BW", line_bw_cb);
            add_value_line(
                app,
                model->config.tx_preamble,
                MIN_PREAMBLE,
                MAX_PREAMBLE,
                "TX Preamble",
                line_tx_preamble_cb);
            add_value_line(
                app,
                model->config.rx_preamble,
                MIN_PREAMBLE,
                MAX_PREAMBLE,
                "RX Preamble",
                line_rx_preamble_cb);
            add_value_line(app, model->config.power, MIN_POWER, MAX_POWER, "Power", line_power_cb);
            add_boolean_line(app, model->config.with_crc, "CRC", line_crc_cb);
            add_boolean_line(
                app, model->config.is_iq_inverted, "IQ Inverted", line_iq_inverted_cb);
            add_boolean_line(
                app, model->config.with_public_lorawan, "Public LoraWan", line_public_lorawan_cb);
        },
        false);
    /* *INDENT-ON* */
    view_dispatcher_switch_to_view(app->view_dispatcher, LoraAppReceiverCfgView);
}

void lora_scene_receive_mode_cfg_on_exit(void* context) {
    LoraApp* app = context;
    variable_item_list_reset(app->var_item_list);
}

bool lora_scene_receive_mode_cfg_on_event(void* context, SceneManagerEvent event) {
    LoraApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == LoraReceiverEventCfgSet) {
            consumed = true;
            /* *INDENT-OFF* */
            with_view_model(
                app->receiver->view,
                LoraReceiverModel * model,
                {
                    // Send the new configuration to the receiver
                    LoraConfigModel config_copy;
                    config_copy = model->config;
                    lora_transmitter_set_rf_test_config(app->transmitter, &config_copy);
                },
                false);
            /* *INDENT-ON* */
        }
    }
    return consumed;
}
