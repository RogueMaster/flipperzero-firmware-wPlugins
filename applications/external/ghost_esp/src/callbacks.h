#pragma once
#include "settings_def.h"
#include "app_state.h"
#include <gui/modules/variable_item_list.h>
#include <stdbool.h>
#include <furi.h>
#include "utils.h"
#include <storage/storage.h>

// Function declarations
void update_settings_and_write(AppState* app, Settings* settings);
void on_rgb_mode_changed(VariableItem* item);
void on_channelswitchdelay_changed(VariableItem* item);
void on_togglechannelhopping_changed(VariableItem* item);
void on_ble_mac_changed(VariableItem* item);
void on_stop_on_back_changed(VariableItem* item);
void on_reboot_esp_changed(VariableItem* item);
void on_clear_logs_changed(VariableItem* item);
void on_clear_nvs_changed(VariableItem* item);
void logs_clear_confirmed_callback(void* context);
void logs_clear_cancelled_callback(void* context);
void nvs_clear_confirmed_callback(void* context);
void nvs_clear_cancelled_callback(void* context);
void show_app_info(void* context);
void app_info_ok_callback(void* context);
void app_info_cancel_callback(void* context);
void wardrive_clear_confirmed_callback(void* context);
void wardrive_clear_cancelled_callback(void* context);
void pcap_clear_confirmed_callback(void* context);
void pcap_clear_cancelled_callback(void* context);
void on_disable_esp_check_changed(VariableItem* item);
