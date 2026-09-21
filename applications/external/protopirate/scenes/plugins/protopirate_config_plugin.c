#include "protopirate_config_plugin.h"
#include "../../protopirate_app_i.h"
#include "../../helpers/protopirate_models.h"

static const ProtoPirateConfigSceneHostApi* g_config_scene_host_api = NULL;

#define protopirate_preset_init(app, preset_name, frequency, preset_data, preset_data_size) \
    g_config_scene_host_api->protopirate_preset_init(                                       \
        app, preset_name, frequency, preset_data, preset_data_size)

#define protopirate_refresh_protocol_registry(app, ensure_receiver_ready) \
    g_config_scene_host_api->protopirate_refresh_protocol_registry(app, ensure_receiver_ready)

#define ON_OFF_COUNT 2
const char* const on_off_text[ON_OFF_COUNT] = {
    "OFF",
    "ON",
};

const uint32_t hopping_value[ON_OFF_COUNT] = {
    ProtoPirateHopperStateOFF,
    ProtoPirateHopperStateRunning,
};

const char* const sequence_time_text[ON_OFF_COUNT] = {
    "Sequential",
    "Time",
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

bool protopirate_ensure_variable_item_list(ProtoPirateApp* app) {
    furi_check(app);

    if(app->variable_item_list) {
        view_dispatcher_remove_view(app->view_dispatcher, ProtoPirateViewVariableItemList);
        variable_item_list_free(app->variable_item_list);
        //Reassigned below... app->variable_item_list = NULL;
    }

    app->variable_item_list = variable_item_list_alloc();
    if(!app->variable_item_list) {
        return false;
    }

    view_dispatcher_add_view(
        app->view_dispatcher,
        ProtoPirateViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));
    return true;
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

    variable_item_set_current_value_text(item, on_off_text[index]);
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

static void protopirate_scene_receiver_config_set_model(VariableItem* item) {
    ProtoPirateApp* app = variable_item_get_context(item);
    uint8_t direction = variable_item_get_current_value_index(item);

    //Get current selection
    uint16_t model_index;
    model_index =
        (app->selected_model && app->selected_model->index) ? app->selected_model->index : 0;
    uint16_t old_model_index = model_index;

    //Cycle the index.
    switch(direction) {
    case VariableItemListEventCycleReset: {
        model_index = 0;
        break;
    }
    case VariableItemListEventCycleLeft: {
        if(model_index > 0)
            model_index--;
        else
            model_index = app->car_models_count;
        break;
    }
    case VariableItemListEventCycleRight: {
        if(model_index < app->car_models_count)
            model_index++;
        else
            model_index = 0;
    }
    }

    //Get a Car Model object, and dont forget to shut down on app free!
    car_model_get_by_index(
        (ProtoPirateCarModel*)app->selected_model,
        model_index,
        app->car_models_count,
        app->setting);

    //Add he car model the list, with the correct selection and text.
    variable_item_set_item_label(item, furi_string_get_cstr(app->selected_model->name));
    variable_item_set_current_value_text(item, "");
    variable_item_set_current_value_index(item, 0);

    //Restore Original Preset.
    VariableItem* freq_menu =
        variable_item_list_get(app->variable_item_list, ProtoPirateSettingIndexFrequency);
    VariableItem* hop_menu =
        variable_item_list_get(app->variable_item_list, ProtoPirateSettingIndexHopping);
    VariableItem* preset_menu =
        variable_item_list_get(app->variable_item_list, ProtoPirateSettingIndexModulation);

    //set the Preset, Frequency and Hopper off or restore.
    if(model_index) {
        //Save Original Preset.
        if(!old_model_index) {
            if(!strcmp(furi_string_get_cstr(app->txrx->preset->name), "Custom")) {
                app->selected_model->last_preset_index = 0;
            } else {
                app->selected_model->last_preset_index = subghz_setting_get_inx_preset_by_name(
                    app->setting, furi_string_get_cstr(app->txrx->preset->name));
            }
        }

        protopirate_preset_init(
            app,
            furi_string_get_cstr(app->selected_model->preset->name),
            app->selected_model->preset->frequency,
            app->selected_model->preset->data,
            app->selected_model->preset->data_size);

        variable_item_set_current_value_text(freq_menu, "Locked");
        variable_item_set_current_value_text(hop_menu, "Locked");
        app->txrx->hopper_state = ProtoPirateHopperStateOFF;
        //app->txrx->preset->frequency = app->selected_model->preset->frequency;
    } else {
        //Restore Original Preset.
        protopirate_scene_receiver_config_set_frequency(freq_menu);
        protopirate_scene_receiver_config_set_hopping_running(hop_menu);

        protopirate_preset_init(
            app,
            subghz_setting_get_preset_name(app->setting, app->selected_model->last_preset_index),
            app->txrx->preset->frequency,
            subghz_setting_get_preset_data(app->setting, app->selected_model->last_preset_index),
            subghz_setting_get_preset_data_size(
                app->setting, app->selected_model->last_preset_index));

        variable_item_set_current_value_text(
            preset_menu,
            subghz_setting_get_preset_name(app->setting, app->selected_model->last_preset_index));
    }

    //Refresh the protocol registry now that we have a new Modulation Type.
    if(!protopirate_refresh_protocol_registry(app, false)) {
        notification_message(app->notifications, &sequence_error);
    }

    //Lock or Unlock the menus.
    bool lock = (app->selected_model->index != 0);
    variable_item_set_locked(freq_menu, lock, "Turn off\nCar Model\nto do that!");
    variable_item_set_locked(hop_menu, lock, "Turn off\nCar Model\nto do that!");
    variable_item_set_locked(preset_menu, lock, "Turn off\nCar Model\nto do that!");
}

static uint8_t
    protopirate_scene_receiver_config_next_frequency(const uint32_t value, void* context) {
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

static uint8_t
    protopirate_scene_receiver_config_next_preset(const char* preset_name, void* context) {
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

static uint8_t protopirate_scene_receiver_config_hopper_value_index(
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

static void protopirate_scene_receiver_config_set_auto_save(VariableItem* item) {
    ProtoPirateApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->auto_save = (index == 1);
    variable_item_set_current_value_text(item, on_off_text[index]);
}

static void protopirate_scene_receiver_config_set_datetime_filenames(VariableItem* item) {
    ProtoPirateApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->datetime_filenames = (index == 1);
    variable_item_set_current_value_text(item, sequence_time_text[index]);
}

static void protopirate_scene_receiver_config_set_check_saved(VariableItem* item) {
    ProtoPirateApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->check_saved = (index == 1);
    variable_item_set_current_value_text(item, on_off_text[index]);
}

static void protopirate_scene_receiver_config_set_sound(VariableItem* item) {
    ProtoPirateApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->sound = (index == 1);
    variable_item_set_current_value_text(item, on_off_text[index]);
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

    switch(index) {
    case ProtoPirateSettingIndexCarModel: {
        //Reset the Models Menu
        VariableItem* model_menu =
            variable_item_list_get(app->variable_item_list, ProtoPirateSettingIndexCarModel);
        variable_item_set_current_value_index(model_menu, 0);
        protopirate_scene_receiver_config_set_model(model_menu);
        break;
    }
    case ProtoPirateSettingIndexLock: {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, ProtoPirateCustomEventSceneSettingLock);
        break;
    }
    }
}

static void plugin_on_enter(void* context) {
    ProtoPirateApp* app = context;
    VariableItem* item;
    uint8_t value_index;

    if(!protopirate_ensure_variable_item_list(app)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    //Add he car model the list, with the correct selection and text.
    item = variable_item_list_add(
        app->variable_item_list,
        furi_string_get_cstr(app->selected_model->name),
        0, //Plus NONE
        protopirate_scene_receiver_config_set_model,
        app);

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
    variable_item_set_locked(
        item,
        app->selected_model && (app->selected_model->index),
        "Turn off\nCar Model\nto do that!");

    item = variable_item_list_add(
        app->variable_item_list,
        "Hopping:",
        ON_OFF_COUNT,
        protopirate_scene_receiver_config_set_hopping_running,
        app);
    value_index = protopirate_scene_receiver_config_hopper_value_index(
        app->txrx->hopper_state, hopping_value, ON_OFF_COUNT, app);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, on_off_text[value_index]);
    variable_item_set_locked(
        item,
        app->selected_model && (app->selected_model->index),
        "Turn off\nCar Model\nto do that!");

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
    variable_item_set_locked(
        item,
        app->selected_model && (app->selected_model->index),
        "Turn off\nCar Model\nto do that!");

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
        ON_OFF_COUNT,
        protopirate_scene_receiver_config_set_auto_save,
        app);
    variable_item_set_current_value_index(item, app->auto_save ? 1 : 0);
    variable_item_set_current_value_text(item, on_off_text[app->auto_save ? 1 : 0]);
    // Date/time filenames option
    item = variable_item_list_add(
        app->variable_item_list,
        "Filenames:",
        2,
        protopirate_scene_receiver_config_set_datetime_filenames,
        app);
    variable_item_set_current_value_index(item, app->datetime_filenames ? 1 : 0);
    variable_item_set_current_value_text(
        item, sequence_time_text[app->datetime_filenames ? 1 : 0]);

    item = variable_item_list_add(
        app->variable_item_list,
        "Check Saved:",
        ON_OFF_COUNT,
        protopirate_scene_receiver_config_set_check_saved,
        app);
    variable_item_set_current_value_index(item, app->check_saved ? 1 : 0);
    variable_item_set_current_value_text(item, on_off_text[app->check_saved ? 1 : 0]);

    // Sound option
    item = variable_item_list_add(
        app->variable_item_list,
        "Sound:",
        ON_OFF_COUNT,
        protopirate_scene_receiver_config_set_sound,
        app);
    variable_item_set_current_value_index(item, app->sound);
    variable_item_set_current_value_text(item, on_off_text[app->sound ? 1 : 0]);

    variable_item_list_add(app->variable_item_list, "Lock Keyboard", 1, NULL, NULL);
    variable_item_list_set_enter_callback(
        app->variable_item_list, protopirate_scene_receiver_config_var_list_enter_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewVariableItemList);
}

void config_plugin_set_host_api(const ProtoPirateConfigSceneHostApi* host_api) {
    g_config_scene_host_api = host_api;
}

static const ProtoPirateConfigPlugin protopirate_config_plugin = {
    .plugin_name = "ProtoPirate Config",
    .car_model_get_by_index = car_model_get_by_index,
    .car_model_get_count = car_model_get_count,
    .on_enter = plugin_on_enter,
    .set_host_api = config_plugin_set_host_api,
};

static const FlipperAppPluginDescriptor protopirate_config_plugin_descriptor = {
    .appid = PROTOPIRATE_CONFIG_PLUGIN_APP_ID,
    .ep_api_version = PROTOPIRATE_CONFIG_PLUGIN_API_VERSION,
    .entry_point = &protopirate_config_plugin,
};

const FlipperAppPluginDescriptor* protopirate_config_plugin_ep(void) {
    return &protopirate_config_plugin_descriptor;
}
