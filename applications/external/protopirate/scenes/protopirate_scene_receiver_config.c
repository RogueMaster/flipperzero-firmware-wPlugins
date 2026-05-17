// scenes/protopirate_scene_receiver_config.c
#include "../protopirate_app_i.h"

enum ProtoPirateSettingIndex {
    ProtoPirateSettingIndexFrequency,
    ProtoPirateSettingIndexHopping,
    ProtoPirateSettingIndexModulation,
#ifdef ENABLE_EMULATE_FEATURE
    ProtoPirateSettingIndexTXPower,
#endif
    ProtoPirateSettingIndexAutoSave,
    ProtoPirateSettingIndexLock,
};

#define HOPPING_COUNT 2
const char* const hopping_text[HOPPING_COUNT] = {
    "OFF",
    "ON",
};

const uint32_t hopping_value[HOPPING_COUNT] = {
    ProtoPirateHopperStateOFF,
    ProtoPirateHopperStateRunning,
};

#define AUTO_SAVE_COUNT 2
const char* const auto_save_text[AUTO_SAVE_COUNT] = {
    "OFF",
    "ON",
};

#ifdef ENABLE_EMULATE_FEATURE
#define TX_POWER_COUNT 9
const char* const tx_power_text[TX_POWER_COUNT] = {
    "Preset",
    "10dBm +",
    "7dBm",
    "5dBm",
    "0dBm",
    "-10dBm",
    "-15dBm",
    "-20dBm",
    "-30dBm",
};
#endif

uint8_t protopirate_scene_receiver_config_next_frequency(const uint32_t value, void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;
    uint8_t index = 0;
    for(uint8_t i = 0; i < subghz_setting_get_frequency_count(app->setting); i++) {
        if(value == subghz_setting_get_frequency(app->setting, i)) {
            index = i;
            break;
        } else {
            index = subghz_setting_get_frequency_default_index(app->setting);
        }
    }
    return index;
}

uint8_t protopirate_scene_receiver_config_next_preset(const char* preset_name, void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;
    uint8_t index = 0;
    for(uint8_t i = 0; i < subghz_setting_get_preset_count(app->setting); i++) {
        if(!strcmp(subghz_setting_get_preset_name(app->setting, i), preset_name)) {
            index = i;
            break;
        }
    }
    return index;
}

uint8_t protopirate_scene_receiver_config_hopper_value_index(
    const uint32_t value,
    const uint32_t values[],
    uint8_t values_count,
    void* context) {
    furi_check(context);
    UNUSED(values_count);
    ProtoPirateApp* app = context;

    if(value == values[0]) {
        return 0;
    } else {
        variable_item_set_current_value_text(
            (VariableItem*)scene_manager_get_scene_state(
                app->scene_manager, ProtoPirateSceneReceiverConfig),
            " -----");
        return 1;
    }
}

static void protopirate_scene_receiver_config_set_frequency(VariableItem* item) {
    ProtoPirateApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    if(app->txrx->hopper_state == ProtoPirateHopperStateOFF) {
        char text_buf[10] = {0};
        snprintf(
            text_buf,
            sizeof(text_buf),
            "%lu.%02lu",
            subghz_setting_get_frequency(app->setting, index) / 1000000,
            (subghz_setting_get_frequency(app->setting, index) % 1000000) / 10000);
        variable_item_set_current_value_text(item, text_buf);
        app->txrx->preset->frequency = subghz_setting_get_frequency(app->setting, index);
    } else {
        variable_item_set_current_value_index(
            item, subghz_setting_get_frequency_default_index(app->setting));
    }
}

static void protopirate_scene_receiver_config_set_preset(VariableItem* item) {
    ProtoPirateApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(
        item, subghz_setting_get_preset_name(app->setting, index));
    protopirate_preset_init(
        app,
        subghz_setting_get_preset_name(app->setting, index),
        app->txrx->preset->frequency,
        subghz_setting_get_preset_data(app->setting, index),
        subghz_setting_get_preset_data_size(app->setting, index));

    if(!protopirate_refresh_protocol_registry(app, false)) {
        notification_message(app->notifications, &sequence_error);
    }
}

static void protopirate_scene_receiver_config_set_hopping_running(VariableItem* item) {
    ProtoPirateApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, hopping_text[index]);
    if(hopping_value[index] == ProtoPirateHopperStateOFF) {
        char text_buf[10] = {0};
        snprintf(
            text_buf,
            sizeof(text_buf),
            "%lu.%02lu",
            subghz_setting_get_default_frequency(app->setting) / 1000000,
            (subghz_setting_get_default_frequency(app->setting) % 1000000) / 10000);
        variable_item_set_current_value_text(
            (VariableItem*)scene_manager_get_scene_state(
                app->scene_manager, ProtoPirateSceneReceiverConfig),
            text_buf);
        app->txrx->preset->frequency = subghz_setting_get_default_frequency(app->setting);
        variable_item_set_current_value_index(
            (VariableItem*)scene_manager_get_scene_state(
                app->scene_manager, ProtoPirateSceneReceiverConfig),
            subghz_setting_get_frequency_default_index(app->setting));
    } else {
        variable_item_set_current_value_text(
            (VariableItem*)scene_manager_get_scene_state(
                app->scene_manager, ProtoPirateSceneReceiverConfig),
            " -----");
        variable_item_set_current_value_index(
            (VariableItem*)scene_manager_get_scene_state(
                app->scene_manager, ProtoPirateSceneReceiverConfig),
            subghz_setting_get_frequency_default_index(app->setting));
    }

    app->txrx->hopper_state = hopping_value[index];
}

static void protopirate_scene_receiver_config_set_auto_save(VariableItem* item) {
    ProtoPirateApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->auto_save = (index == 1);
    variable_item_set_current_value_text(item, auto_save_text[index]);
}

#ifdef ENABLE_EMULATE_FEATURE
static void protopirate_scene_receiver_config_set_tx_power(VariableItem* item) {
    ProtoPirateApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->tx_power = index;
    variable_item_set_current_value_text(item, tx_power_text[index]);
}
#endif

static void
    protopirate_scene_receiver_config_var_list_enter_callback(void* context, uint32_t index) {
    furi_check(context);
    ProtoPirateApp* app = context;
    if(index == ProtoPirateSettingIndexLock) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, ProtoPirateCustomEventSceneSettingLock);
    }
}

void protopirate_scene_receiver_config_on_enter(void* context) {
    ProtoPirateApp* app = context;
    VariableItem* item;
    uint8_t value_index;

    if(!protopirate_ensure_variable_item_list(app)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    item = variable_item_list_add(
        app->variable_item_list,
        "Frequency:",
        subghz_setting_get_frequency_count(app->setting),
        protopirate_scene_receiver_config_set_frequency,
        app);
    value_index =
        protopirate_scene_receiver_config_next_frequency(app->txrx->preset->frequency, app);
    scene_manager_set_scene_state(
        app->scene_manager, ProtoPirateSceneReceiverConfig, (uint32_t)item);
    variable_item_set_current_value_index(item, value_index);
    char text_buf[10] = {0};
    snprintf(
        text_buf,
        sizeof(text_buf),
        "%lu.%02lu",
        subghz_setting_get_frequency(app->setting, value_index) / 1000000,
        (subghz_setting_get_frequency(app->setting, value_index) % 1000000) / 10000);
    variable_item_set_current_value_text(item, text_buf);

    item = variable_item_list_add(
        app->variable_item_list,
        "Hopping:",
        HOPPING_COUNT,
        protopirate_scene_receiver_config_set_hopping_running,
        app);
    value_index = protopirate_scene_receiver_config_hopper_value_index(
        app->txrx->hopper_state, hopping_value, HOPPING_COUNT, app);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, hopping_text[value_index]);

    item = variable_item_list_add(
        app->variable_item_list,
        "Modulation:",
        subghz_setting_get_preset_count(app->setting),
        protopirate_scene_receiver_config_set_preset,
        app);
    value_index = protopirate_scene_receiver_config_next_preset(
        furi_string_get_cstr(app->txrx->preset->name), app);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(
        item, subghz_setting_get_preset_name(app->setting, value_index));

#ifdef ENABLE_EMULATE_FEATURE
    // TX power option
    item = variable_item_list_add(
        app->variable_item_list,
        "TX Power:",
        TX_POWER_COUNT,
        protopirate_scene_receiver_config_set_tx_power,
        app);
    variable_item_set_current_value_index(item, app->tx_power);
    variable_item_set_current_value_text(item, tx_power_text[app->tx_power]);
#endif
    // Auto-save option
    item = variable_item_list_add(
        app->variable_item_list,
        "Auto-Save:",
        AUTO_SAVE_COUNT,
        protopirate_scene_receiver_config_set_auto_save,
        app);
    variable_item_set_current_value_index(item, app->auto_save ? 1 : 0);
    variable_item_set_current_value_text(item, auto_save_text[app->auto_save ? 1 : 0]);

    variable_item_list_add(app->variable_item_list, "Lock Keyboard", 1, NULL, NULL);
    variable_item_list_set_enter_callback(
        app->variable_item_list, protopirate_scene_receiver_config_var_list_enter_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewVariableItemList);
}

bool protopirate_scene_receiver_config_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ProtoPirateCustomEventSceneSettingLock) {
            app->lock = ProtoPirateLockOn;
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    }
    return consumed;
}

void protopirate_scene_receiver_config_on_exit(void* context) {
    ProtoPirateApp* app = context;
    variable_item_list_set_selected_item(app->variable_item_list, 0);
    variable_item_list_reset(app->variable_item_list);
}
