#pragma once

#include <furi.h>
#include <furi_hal_bt.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// BLE handler instance
typedef struct BleHandler BleHandler;

// BLE connection status
typedef enum {
    BleStatusDisconnected,
    BleStatusScanning,
    BleStatusConnecting,
    BleStatusConnected,
    BleStatusError,
} BleStatus;

// Callback for received data
typedef void (*BleHandlerRxCallback)(const uint8_t* data, size_t length, void* context);

// Callback for connection status changes
typedef void (*BleHandlerStatusCallback)(BleStatus status, void* context);

// Create and destroy BLE handler
BleHandler* ble_handler_alloc();
void ble_handler_free(BleHandler* handler);

// Initialize BLE
bool ble_handler_init(BleHandler* handler);
void ble_handler_deinit(BleHandler* handler);

// Set callbacks
void ble_handler_set_rx_callback(BleHandler* handler, BleHandlerRxCallback callback, void* context);
void ble_handler_set_status_callback(BleHandler* handler, BleHandlerStatusCallback callback, void* context);

// Scan for Chameleon Ultra devices
bool ble_handler_start_scan(BleHandler* handler);
void ble_handler_stop_scan(BleHandler* handler);

// Get list of found devices
size_t ble_handler_get_device_count(BleHandler* handler);
const char* ble_handler_get_device_name(BleHandler* handler, size_t index);

// Connect to device
bool ble_handler_connect(BleHandler* handler, size_t device_index);
void ble_handler_disconnect(BleHandler* handler);

// Send data
bool ble_handler_send(BleHandler* handler, const uint8_t* data, size_t length);

// Check connection status
BleStatus ble_handler_get_status(BleHandler* handler);
bool ble_handler_is_connected(BleHandler* handler);
