// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"

static const char* const backend_text[] = {"Companion", "Marauder"};
static const char* const port_text[] = {"USART 13/14", "LPUART 15/16"};
static const char* const onoff_text[] = {"OFF", "ON"};

static const uint32_t esp_baud_val[] = {115200, 921600};
static const char* const esp_baud_text[] = {"115200", "921600"};
static const uint32_t gps_baud_val[] = {9600, 115200, 57600};
static const char* const gps_baud_text[] = {"9600", "115200", "57600"};

// Index-aligned with ESP_MARAUDER_CMDS in helpers/esp_link.c.
#define MARAUDER_CMD_COUNT 4
static const char* const marauder_text[] = {"Probe req", "AP scan", "Beacon", "Raw"};

static uint8_t index_of_u32(const uint32_t* arr, size_t n, uint32_t val) {
    for(size_t i = 0; i < n; i++) {
        if(arr[i] == val) return (uint8_t)i;
    }
    return 0;
}

static void backend_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.backend = (idx == 1) ? EspBackendGeneric : EspBackendCompanion;
    variable_item_set_current_value_text(item, backend_text[idx]);
    recon_settings_save(app);
}

static void esp_port_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.esp_uart = (idx == 1) ? FuriHalSerialIdLpuart : FuriHalSerialIdUsart;
    variable_item_set_current_value_text(item, port_text[idx]);
    recon_settings_save(app);
}

static void esp_baud_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.esp_baud = esp_baud_val[idx];
    variable_item_set_current_value_text(item, esp_baud_text[idx]);
    recon_settings_save(app);
}

static void marauder_cmd_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.marauder_cmd = idx;
    variable_item_set_current_value_text(item, marauder_text[idx]);
    recon_settings_save(app);
}

static void gps_enabled_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.gps_enabled = (idx == 1);
    variable_item_set_current_value_text(item, onoff_text[idx]);
    recon_settings_save(app);
}

static void gps_port_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.gps_uart = (idx == 1) ? FuriHalSerialIdLpuart : FuriHalSerialIdUsart;
    variable_item_set_current_value_text(item, port_text[idx]);
    recon_settings_save(app);
}

static void gps_baud_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.gps_baud = gps_baud_val[idx];
    variable_item_set_current_value_text(item, gps_baud_text[idx]);
    recon_settings_save(app);
}

static void sound_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.sound = (idx == 1);
    variable_item_set_current_value_text(item, onoff_text[idx]);
    recon_settings_save(app);
}

static const char* const flash_speed_text[] = {"Safe 115k", "Fast 921k"};

static void flash_fast_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.flash_fast = (idx == 1);
    variable_item_set_current_value_text(item, flash_speed_text[idx]);
    recon_settings_save(app);
}

static void log_serials_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.log_serials = (idx == 1);
    variable_item_set_current_value_text(item, onoff_text[idx]);
    recon_settings_save(app);
}

static void anomaly_flag_changed(VariableItem* item) {
    ReconApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.anomaly_flag = (idx == 1);
    variable_item_set_current_value_text(item, onoff_text[idx]);
    recon_settings_save(app);
}

void recon_scene_settings_on_enter(void* context) {
    ReconApp* app = context;
    VariableItemList* list = app->var_item_list;
    variable_item_list_reset(list);
    VariableItem* item;
    uint8_t idx;

    idx = (app->settings.backend == EspBackendGeneric) ? 1 : 0;
    item = variable_item_list_add(list, "Board Mode", 2, backend_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, backend_text[idx]);

    idx = (app->settings.esp_uart == FuriHalSerialIdLpuart) ? 1 : 0;
    item = variable_item_list_add(list, "ESP Port", 2, esp_port_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, port_text[idx]);

    idx = index_of_u32(esp_baud_val, COUNT_OF(esp_baud_val), app->settings.esp_baud);
    item = variable_item_list_add(list, "ESP Baud", COUNT_OF(esp_baud_val), esp_baud_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, esp_baud_text[idx]);

    idx = (app->settings.marauder_cmd < MARAUDER_CMD_COUNT) ? app->settings.marauder_cmd : 0;
    item = variable_item_list_add(
        list, "Marauder Cmd", MARAUDER_CMD_COUNT, marauder_cmd_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, marauder_text[idx]);

    idx = app->settings.gps_enabled ? 1 : 0;
    item = variable_item_list_add(list, "GPS", 2, gps_enabled_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, onoff_text[idx]);

    idx = (app->settings.gps_uart == FuriHalSerialIdLpuart) ? 1 : 0;
    item = variable_item_list_add(list, "GPS Port", 2, gps_port_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, port_text[idx]);

    idx = index_of_u32(gps_baud_val, COUNT_OF(gps_baud_val), app->settings.gps_baud);
    item = variable_item_list_add(list, "GPS Baud", COUNT_OF(gps_baud_val), gps_baud_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, gps_baud_text[idx]);

    idx = app->settings.sound ? 1 : 0;
    item = variable_item_list_add(list, "Sound", 2, sound_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, onoff_text[idx]);

    idx = app->settings.flash_fast ? 1 : 0;
    item = variable_item_list_add(list, "Flash Speed", 2, flash_fast_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, flash_speed_text[idx]);

    idx = app->settings.log_serials ? 1 : 0;
    item = variable_item_list_add(list, "Log Flock serials", 2, log_serials_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, onoff_text[idx]);

    // Net Guardian: flag unidentified strong/persistent BLE devices as suspicious.
    // Off by default -- it trades a higher false-positive rate for more coverage.
    idx = app->settings.anomaly_flag ? 1 : 0;
    item = variable_item_list_add(list, "Anomaly flag", 2, anomaly_flag_changed, app);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, onoff_text[idx]);

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewVarItemList);
}

bool recon_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void recon_scene_settings_on_exit(void* context) {
    ReconApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
