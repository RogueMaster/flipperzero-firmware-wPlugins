#pragma once

#include "../nfc_login_app.h"

// BLE HID functions - completely separate from USB
bool ble_hid_init(void);
void ble_hid_deinit(void);
bool ble_hid_is_ready(void);
bool ble_hid_is_connected(void); // Check if BLE is connected
void ble_hid_release_all_keys(void);
void ble_hid_press_key(uint16_t keycode);
void ble_hid_release_key(uint16_t keycode);
uint32_t ble_hid_type_password(App* app, const char* password);
void ble_hid_start_advertising(void);
void ble_hid_stop_advertising(void);
