/**
 * @file flippass_output_plugin.h
 * @brief Shared ABI for FlipPass transport plugins.
 *
 * These plugins own transport-specific runtime state such as USB takeover or
 * BLE advertising sessions. The host keeps AutoType parsing, layout mapping,
 * and vault interaction, while late-loaded plugins perform the actual HID
 * transport work.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <flipper_application/flipper_application.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_OUTPUT_PLUGIN_API_VERSION 4u
#define FLIPPASS_OUTPUT_USB_PLUGIN_APP_ID  "flippass_output_usb"
#define FLIPPASS_OUTPUT_BLE_PLUGIN_APP_ID  "flippass_output_ble"

typedef enum {
    FlipPassOutputPluginTransportUsb = 0,
    FlipPassOutputPluginTransportBluetooth = 1,
} FlipPassOutputPluginTransport;

/* Notifications are serialized through a short critical section. Callbacks
 * must only latch state: no waits, allocation, transport calls or GUI work. */
typedef void (*FlipPassOutputConnectionCallback)(bool connected, void* context);

typedef struct {
    uint32_t api_version;
    void* host_context;

    void (*progress)(void* host_context, const char* stage, const char* detail, uint8_t percent);
    void (*log)(void* host_context, const char* module_name, const char* message);
    bool (*should_cancel)(void* host_context);
} FlipPassOutputPluginHostApiV1;

typedef struct {
    uint32_t api_version;
    const char* module_name;
    FlipPassOutputPluginTransport transport;

    bool (*begin)(const FlipPassOutputPluginHostApiV1* host_api);
    bool (*press_key)(const FlipPassOutputPluginHostApiV1* host_api, uint16_t hid_key);
    bool (*release_key)(const FlipPassOutputPluginHostApiV1* host_api, uint16_t hid_key);
    void (*release_all)(const FlipPassOutputPluginHostApiV1* host_api);
    void (*end)(const FlipPassOutputPluginHostApiV1* host_api);
    bool (*is_connected)(const FlipPassOutputPluginHostApiV1* host_api);
    bool (*is_advertising)(const FlipPassOutputPluginHostApiV1* host_api);
    bool (*advertise)(const FlipPassOutputPluginHostApiV1* host_api);
    void (*get_name)(char* buffer, size_t size);
    void (*cleanup)(const FlipPassOutputPluginHostApiV1* host_api);
    /* NULL detaches synchronously: no callback may retain context on return. */
    void (*watch_connection)(FlipPassOutputConnectionCallback callback, void* context);
} FlipPassOutputPluginV1;

const FlipPassOutputPluginV1* flippass_output_usb_plugin_table(void);
const FlipPassOutputPluginV1* flippass_output_ble_plugin_table(void);

#ifdef __cplusplus
}
#endif
