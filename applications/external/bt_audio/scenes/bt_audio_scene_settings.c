#include "../bt_audio.h"
#include <string.h>

// Settings menu item indices
typedef enum {
    SettingsIndexSdSource,
    SettingsIndexTxPower,
    SettingsIndex5vGpio,
    SettingsIndexBackgroundMode,
    SettingsIndexBacklight,
    SettingsIndexShuffleMode,
    SettingsIndexContinuousPlay, // Continuous play mode for file browser
    SettingsIndexInitialVolume,
    SettingsIndexEqBass,
    SettingsIndexEqMid,
    SettingsIndexEqTreble,
    SettingsIndexBtDeviceName,
    SettingsIndexWifiEnabled, // WiFi enable/disable toggle
    SettingsIndexWifiSettings, // WiFi settings submenu
    SettingsIndexSavedDevices, // Saved devices list (includes last device as first entry)
} SettingsIndex;

static const char* sd_source_names[] = {
    "Flipper SD (WIP)",
    "ESP32 SD",
};

static const char* tx_power_names[] = {
    "Low (-12dBm)",
    "Medium (0dBm)",
    "High (+6dBm)",
    "Max (+9dBm)",
};

static const char* enable_5v_names[] = {
    "Off",
    "On",
};

static const char* background_mode_names[] = {
    "Off",
    "On",
};

static const char* backlight_names[] = {
    "Auto (Timeout)",
    "Always On",
};

static const char* shuffle_mode_names[] = {
    "Off",
    "On",
};

static const char* continuous_play_names[] = {
    "Repeat File", // OFF - repeat the selected file
    "Play Next", // ON - play next files in directory
};

static const char* wifi_enabled_names[] = {
    "Off",
    "On",
};

// Volume level names (0-127 in steps of ~12.7, showing %)
static const char* volume_names[] = {
    "0%", // 0
    "10%", // 13
    "20%", // 25
    "30%", // 38
    "40%", // 51
    "50%", // 64
    "60%", // 76
    "70%", // 89
    "75%", // 96 (default)
    "80%", // 102
    "90%", // 114
    "100%", // 127
};
#define VOLUME_STEP_COUNT 12
static const uint8_t volume_values[] = {0, 13, 25, 38, 51, 64, 76, 89, 96, 102, 114, 127};

// EQ level names (-10 to +10 dB)
static const char* eq_names[] = {
    "-10 dB",
    "-8 dB",
    "-6 dB",
    "-4 dB",
    "-2 dB",
    "0 dB",
    "+2 dB",
    "+4 dB",
    "+6 dB",
    "+8 dB",
    "+10 dB",
};
#define EQ_STEP_COUNT 11
static const int8_t eq_values[] = {-10, -8, -6, -4, -2, 0, 2, 4, 6, 8, 10};

// Bluetooth device name presets
// Users can choose from these names when connecting to speakers/headphones
// "FlipperAudio" is the default (index 0), followed by rest in alphabetical order
static const char* bt_device_names[] = {
    "FlipperAudio", // Default - kept first
    "Audio Stream",
    "BT Audio",
    "Flipper Zero",
    "Media Player",
    "Music Box",
    "My Speaker",
    "Party Mode",
};
#define BT_DEVICE_NAME_COUNT 8

// Default BT device name index (FlipperAudio)
#define BT_DEVICE_NAME_DEFAULT_INDEX 0

// Helper function to find BT device name index
static uint8_t bt_name_to_index(const char* name) {
    for(uint8_t i = 0; i < BT_DEVICE_NAME_COUNT; i++) {
        if(strcmp(name, bt_device_names[i]) == 0) {
            return i;
        }
    }
    return BT_DEVICE_NAME_DEFAULT_INDEX; // Default to FlipperAudio
}

// Helper function to find volume index
static uint8_t volume_to_index(uint8_t volume) {
    // Iterate backwards to find highest matching index
    for(uint8_t i = VOLUME_STEP_COUNT; i > 0; i--) {
        if(volume >= volume_values[i - 1]) {
            return (uint8_t)(i - 1);
        }
    }
    return 0;
}

// Helper function to find EQ index
static uint8_t eq_to_index(int8_t eq) {
    for(uint8_t i = 0; i < EQ_STEP_COUNT; i++) {
        if(eq_values[i] == eq) {
            return i;
        }
    }
    // Find closest
    for(uint8_t i = 0; i < EQ_STEP_COUNT - 1; i++) {
        if(eq >= eq_values[i] && eq < eq_values[i + 1]) {
            return i;
        }
    }
    return 5; // Default to 0 dB
}

static void bt_audio_scene_settings_sd_source_change(VariableItem* item) {
    BtAudio* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->config.sd_source = index;
    variable_item_set_current_value_text(item, sd_source_names[index]);

    FURI_LOG_I(TAG, "SD source changed to: %s", sd_source_names[index]);

    // Warn user if Flipper SD is selected (WIP feature)
    if(index == BtAudioSdSourceFlipper) {
        FURI_LOG_W(
            TAG, "Flipper SD mode selected - this feature is WIP and not fully implemented");
    }
}

static void bt_audio_scene_settings_tx_power_change(VariableItem* item) {
    BtAudio* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->config.tx_power = index;
    variable_item_set_current_value_text(item, tx_power_names[index]);

    // Send TX power command to ESP32
    char cmd[32];
    const char* power_level;
    switch(index) {
    case BtAudioTxPowerLow:
        power_level = "LOW";
        break;
    case BtAudioTxPowerMedium:
        power_level = "MEDIUM";
        break;
    case BtAudioTxPowerHigh:
        power_level = "HIGH";
        break;
    case BtAudioTxPowerMax:
    default:
        power_level = "MAX";
        break;
    }
    snprintf(cmd, sizeof(cmd), "TX_POWER:%s\n", power_level);
    bt_audio_uart_tx(app->uart, cmd);

    FURI_LOG_I(TAG, "TX power changed to: %s", tx_power_names[index]);
}

static void bt_audio_scene_settings_5v_gpio_change(VariableItem* item) {
    BtAudio* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->config.enable_5v_gpio = (index != 0);
    variable_item_set_current_value_text(item, enable_5v_names[index]);

    // Apply setting immediately
    if(app->config.enable_5v_gpio && !furi_hal_power_is_otg_enabled()) {
        furi_hal_power_enable_otg();
        furi_delay_ms(100);
        if(furi_hal_power_is_otg_enabled()) {
            FURI_LOG_I(TAG, "Enabled 5V on GPIO");
        }
    } else if(!app->config.enable_5v_gpio && furi_hal_power_is_otg_enabled()) {
        furi_hal_power_disable_otg();
        FURI_LOG_I(TAG, "Disabled 5V on GPIO");
    }

    FURI_LOG_I(TAG, "5V GPIO auto-enable: %s", enable_5v_names[index]);
}

static void bt_audio_scene_settings_background_mode_change(VariableItem* item) {
    BtAudio* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->config.background_mode = (index != 0);
    variable_item_set_current_value_text(item, background_mode_names[index]);

    FURI_LOG_I(TAG, "Background mode: %s", background_mode_names[index]);

    if(app->config.background_mode) {
        FURI_LOG_I(
            TAG,
            "Background mode enabled - audio will continue when app exits, UART pins stay in use");
    }
}

static void bt_audio_scene_settings_backlight_change(VariableItem* item) {
    BtAudio* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->config.keep_backlight_on = (index != 0);
    variable_item_set_current_value_text(item, backlight_names[index]);

    // Apply backlight setting immediately using enforce functions
    if(app->config.keep_backlight_on) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_on);
        FURI_LOG_I(TAG, "Backlight enforced always on");
    } else {
        notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
        FURI_LOG_I(TAG, "Backlight restored to auto timeout");
    }

    FURI_LOG_I(TAG, "Backlight: %s", backlight_names[index]);
}

static void bt_audio_scene_settings_shuffle_mode_change(VariableItem* item) {
    BtAudio* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->config.shuffle_mode = (index != 0);
    variable_item_set_current_value_text(item, shuffle_mode_names[index]);

    // Send shuffle mode to ESP32
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "SHUFFLE:%s\n", app->config.shuffle_mode ? "ON" : "OFF");
    bt_audio_uart_tx(app->uart, cmd);

    FURI_LOG_I(TAG, "Shuffle mode: %s", shuffle_mode_names[index]);
}

static void bt_audio_scene_settings_continuous_play_change(VariableItem* item) {
    BtAudio* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->config.continuous_play = (index != 0);
    variable_item_set_current_value_text(item, continuous_play_names[index]);

    // Send continuous play mode to ESP32
    char cmd[32];
    int cmd_len =
        snprintf(cmd, sizeof(cmd), "CONTINUOUS:%s\n", app->config.continuous_play ? "ON" : "OFF");
    if(cmd_len >= 0 && cmd_len < (int)sizeof(cmd)) {
        bt_audio_uart_tx(app->uart, cmd);
    }

    FURI_LOG_I(TAG, "Continuous play: %s", continuous_play_names[index]);
}

static void bt_audio_scene_settings_initial_volume_change(VariableItem* item) {
    BtAudio* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->config.initial_volume = volume_values[index];
    variable_item_set_current_value_text(item, volume_names[index]);

    // Send initial volume to ESP32 if connected
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "SET_INIT_VOL:%d\n", app->config.initial_volume);
    bt_audio_uart_tx(app->uart, cmd);

    FURI_LOG_I(TAG, "Initial volume: %d (%s)", app->config.initial_volume, volume_names[index]);
}

static void bt_audio_scene_settings_eq_bass_change(VariableItem* item) {
    BtAudio* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->config.eq_bass = eq_values[index];
    variable_item_set_current_value_text(item, eq_names[index]);

    // Send EQ setting to ESP32
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "EQ_BASS:%d\n", app->config.eq_bass);
    bt_audio_uart_tx(app->uart, cmd);

    FURI_LOG_I(TAG, "EQ Bass: %d dB", app->config.eq_bass);
}

static void bt_audio_scene_settings_eq_mid_change(VariableItem* item) {
    BtAudio* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->config.eq_mid = eq_values[index];
    variable_item_set_current_value_text(item, eq_names[index]);

    // Send EQ setting to ESP32
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "EQ_MID:%d\n", app->config.eq_mid);
    bt_audio_uart_tx(app->uart, cmd);

    FURI_LOG_I(TAG, "EQ Mid: %d dB", app->config.eq_mid);
}

static void bt_audio_scene_settings_eq_treble_change(VariableItem* item) {
    BtAudio* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->config.eq_treble = eq_values[index];
    variable_item_set_current_value_text(item, eq_names[index]);

    // Send EQ setting to ESP32
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "EQ_TREBLE:%d\n", app->config.eq_treble);
    bt_audio_uart_tx(app->uart, cmd);

    FURI_LOG_I(TAG, "EQ Treble: %d dB", app->config.eq_treble);
}

// Buffer size for BT commands: "SET_BT_NAME:" (12) + max name (31) + newline (1) + null (1) = 45
#define BT_COMMAND_BUFFER_SIZE (12 + BT_AUDIO_BT_NAME_LEN + 2)

static void bt_audio_scene_settings_bt_name_change(VariableItem* item) {
    BtAudio* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    snprintf(app->config.bt_device_name, BT_AUDIO_BT_NAME_LEN, "%s", bt_device_names[index]);
    variable_item_set_current_value_text(item, bt_device_names[index]);

    // Send BT name to ESP32 (will take effect on next BT restart)
    char cmd[BT_COMMAND_BUFFER_SIZE];
    snprintf(cmd, sizeof(cmd), "SET_BT_NAME:%s\n", app->config.bt_device_name);
    bt_audio_uart_tx(app->uart, cmd);

    FURI_LOG_I(TAG, "BT Device Name: %s", app->config.bt_device_name);
}

// WiFi enable/disable callback
static void bt_audio_scene_settings_wifi_enabled_change(VariableItem* item) {
    BtAudio* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->wifi_config.wifi_enabled = (index != 0);
    variable_item_set_current_value_text(item, wifi_enabled_names[index]);

    FURI_LOG_I(TAG, "WiFi Streaming: %s", wifi_enabled_names[index]);

    // Save WiFi config when changed
    bt_audio_wifi_config_save(app);
}

// Callback when user presses OK on a settings item
static void bt_audio_scene_settings_enter_callback(void* context, uint32_t index) {
    BtAudio* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void bt_audio_scene_settings_on_enter(void* context) {
    BtAudio* app = context;
    VariableItemList* var_item_list = app->variable_item_list;

    variable_item_list_reset(var_item_list);

    // SD Card Source setting
    VariableItem* item = variable_item_list_add(
        var_item_list,
        "Music Source",
        BtAudioSdSourceCount,
        bt_audio_scene_settings_sd_source_change,
        app);
    variable_item_set_current_value_index(item, app->config.sd_source);
    variable_item_set_current_value_text(item, sd_source_names[app->config.sd_source]);

    // BT TX Power setting
    item = variable_item_list_add(
        var_item_list,
        "BT TX Power",
        BtAudioTxPowerCount,
        bt_audio_scene_settings_tx_power_change,
        app);
    variable_item_set_current_value_index(item, app->config.tx_power);
    variable_item_set_current_value_text(item, tx_power_names[app->config.tx_power]);

    // 5V GPIO Power setting
    item = variable_item_list_add(
        var_item_list, "5V GPIO Power", 2, bt_audio_scene_settings_5v_gpio_change, app);
    variable_item_set_current_value_index(item, app->config.enable_5v_gpio ? 1 : 0);
    variable_item_set_current_value_text(
        item, enable_5v_names[app->config.enable_5v_gpio ? 1 : 0]);

    // Background mode setting (WIP)
    item = variable_item_list_add(
        var_item_list, "Background Audio", 2, bt_audio_scene_settings_background_mode_change, app);
    variable_item_set_current_value_index(item, app->config.background_mode ? 1 : 0);
    variable_item_set_current_value_text(
        item, background_mode_names[app->config.background_mode ? 1 : 0]);

    // Backlight setting
    item = variable_item_list_add(
        var_item_list, "Backlight", 2, bt_audio_scene_settings_backlight_change, app);
    variable_item_set_current_value_index(item, app->config.keep_backlight_on ? 1 : 0);
    variable_item_set_current_value_text(
        item, backlight_names[app->config.keep_backlight_on ? 1 : 0]);

    // Shuffle mode setting
    item = variable_item_list_add(
        var_item_list, "Shuffle Play", 2, bt_audio_scene_settings_shuffle_mode_change, app);
    variable_item_set_current_value_index(item, app->config.shuffle_mode ? 1 : 0);
    variable_item_set_current_value_text(
        item, shuffle_mode_names[app->config.shuffle_mode ? 1 : 0]);

    // Continuous play setting (for file browser)
    // OFF = Repeat selected file, ON = Play next files in directory
    item = variable_item_list_add(
        var_item_list, "Continuous Play", 2, bt_audio_scene_settings_continuous_play_change, app);
    variable_item_set_current_value_index(item, app->config.continuous_play ? 1 : 0);
    variable_item_set_current_value_text(
        item, continuous_play_names[app->config.continuous_play ? 1 : 0]);

    // Initial Volume setting
    item = variable_item_list_add(
        var_item_list,
        "Initial Volume",
        VOLUME_STEP_COUNT,
        bt_audio_scene_settings_initial_volume_change,
        app);
    uint8_t vol_index = volume_to_index(app->config.initial_volume);
    variable_item_set_current_value_index(item, vol_index);
    variable_item_set_current_value_text(item, volume_names[vol_index]);

    // EQ Bass setting
    item = variable_item_list_add(
        var_item_list, "EQ Bass", EQ_STEP_COUNT, bt_audio_scene_settings_eq_bass_change, app);
    uint8_t bass_index = eq_to_index(app->config.eq_bass);
    variable_item_set_current_value_index(item, bass_index);
    variable_item_set_current_value_text(item, eq_names[bass_index]);

    // EQ Mid setting
    item = variable_item_list_add(
        var_item_list, "EQ Mid", EQ_STEP_COUNT, bt_audio_scene_settings_eq_mid_change, app);
    uint8_t mid_index = eq_to_index(app->config.eq_mid);
    variable_item_set_current_value_index(item, mid_index);
    variable_item_set_current_value_text(item, eq_names[mid_index]);

    // EQ Treble setting
    item = variable_item_list_add(
        var_item_list, "EQ Treble", EQ_STEP_COUNT, bt_audio_scene_settings_eq_treble_change, app);
    uint8_t treble_index = eq_to_index(app->config.eq_treble);
    variable_item_set_current_value_index(item, treble_index);
    variable_item_set_current_value_text(item, eq_names[treble_index]);

    // Bluetooth device name setting (WIP - doesn't work on all devices)
    item = variable_item_list_add(
        var_item_list,
        "WIP BT Device Name",
        BT_DEVICE_NAME_COUNT,
        bt_audio_scene_settings_bt_name_change,
        app);
    uint8_t name_index = bt_name_to_index(app->config.bt_device_name);
    variable_item_set_current_value_index(item, name_index);
    variable_item_set_current_value_text(item, bt_device_names[name_index]);

    // WiFi Streaming Enable/Disable setting
    // NOTE: WiFi streaming is DISABLED due to ESP32 BT/WiFi coexistence limitation
    // The menu option will not appear even if this is enabled.
    // Keeping the setting for future ESP32-S3 support.
    item = variable_item_list_add(
        var_item_list, "WiFi (Disabled)", 2, bt_audio_scene_settings_wifi_enabled_change, app);
    variable_item_set_current_value_index(item, app->wifi_config.wifi_enabled ? 1 : 0);
    variable_item_set_current_value_text(
        item, wifi_enabled_names[app->wifi_config.wifi_enabled ? 1 : 0]);

    // WiFi Settings - click to configure SSID, password, stream URL
    {
        char wifi_label[64];
        if(app->wifi_config.ssid[0] != '\0') {
            // Truncate SSID for display to fit in label buffer (64 - 13 chars - 1 null = 50)
            char truncated_ssid[50];
            snprintf(truncated_ssid, sizeof(truncated_ssid), "%.49s", app->wifi_config.ssid);
            snprintf(wifi_label, sizeof(wifi_label), "WiFi Config: %s", truncated_ssid);
        } else {
            snprintf(wifi_label, sizeof(wifi_label), "WiFi Config");
        }
        item = variable_item_list_add(var_item_list, wifi_label, 1, NULL, app);
        if(app->wifi_config.ssid[0] != '\0') {
            variable_item_set_current_value_text(item, "Configured");
        } else {
            variable_item_set_current_value_text(item, "Not Set");
        }
    }

    // Saved Devices - click to view list of up to 10 last connected devices
    // The first device in the list is always the most recently connected (Last Device)
    {
        char saved_label[32];
        snprintf(
            saved_label, sizeof(saved_label), "Saved Devices (%d)", app->device_history_count);
        item = variable_item_list_add(var_item_list, saved_label, 1, NULL, app);
        if(app->device_history_count > 0) {
            // Show first device name as hint (this is the last connected device)
            variable_item_set_current_value_text(item, app->device_history[0].name);
        } else {
            variable_item_set_current_value_text(item, "None");
        }
    }

    // Set enter callback for item selection
    variable_item_list_set_enter_callback(
        var_item_list, bt_audio_scene_settings_enter_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewVariableItemList);
}

bool bt_audio_scene_settings_on_event(void* context, SceneManagerEvent event) {
    BtAudio* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // Handle click on "WiFi Config" item to view/edit settings
        if(event.event == SettingsIndexWifiSettings) {
            // For now, show WiFi config in a text box
            // TODO: Create proper WiFi settings scene with text input
            FuriString* wifi_info = furi_string_alloc();
            furi_string_printf(wifi_info, "WiFi Configuration\n\n");
            furi_string_cat_printf(
                wifi_info,
                "SSID: %s\n",
                app->wifi_config.ssid[0] != '\0' ? app->wifi_config.ssid : "(not set)");
            furi_string_cat_printf(
                wifi_info,
                "Password: %s\n",
                app->wifi_config.password[0] != '\0' ? "********" : "(not set)");
            furi_string_cat_printf(
                wifi_info,
                "Stream URL: %s\n",
                app->wifi_config.stream_url[0] != '\0' ? app->wifi_config.stream_url :
                                                         "(not set)");
            furi_string_cat_printf(
                wifi_info, "\nEdit wifi.config file manually:\n%s\n", BT_AUDIO_WIFI_CONFIG_FILE);

            text_box_set_text(app->text_box, furi_string_get_cstr(wifi_info));
            text_box_set_font(app->text_box, TextBoxFontText);
            view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewTextBox);

            furi_string_free(wifi_info);
            consumed = true;
        }
        // Handle click on "Saved Devices" item to view list
        else if(event.event == SettingsIndexSavedDevices) {
            scene_manager_next_scene(app->scene_manager, BtAudioSceneSavedDevices);
            consumed = true;
        }
    }

    return consumed;
}

void bt_audio_scene_settings_on_exit(void* context) {
    BtAudio* app = context;
    variable_item_list_reset(app->variable_item_list);

    // Save config when leaving settings
    bt_audio_config_save(app);
}
