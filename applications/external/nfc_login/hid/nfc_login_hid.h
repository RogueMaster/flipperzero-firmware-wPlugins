#pragma once

#include "../nfc_login_app.h"

bool initialize_hid_and_wait_with_mode(HidMode mode);
void deinitialize_hid_with_restore_and_mode(FuriHalUsbInterface* previous_config, HidMode mode);
void release_all_keys_with_mode(HidMode mode);
bool is_ble_hid_ready(void);
uint32_t app_type_password(App* app, const char* password);
void app_start_ble_advertising(void);
void app_stop_ble_advertising(void);
