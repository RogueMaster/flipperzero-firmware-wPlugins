#pragma once

#include <furi.h>
#include <flipper_application/flipper_application.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_OUTPUT_ACTION_PLUGIN_APP_ID      "flippass_output_action"
#define FLIPPASS_OUTPUT_ACTION_PLUGIN_API_VERSION 1u
#define FLIPPASS_OUTPUT_ACTION_HOST_API_VERSION   1u

typedef enum {
    FlipPassOutputActionPluginTransportUsb = 0,
    FlipPassOutputActionPluginTransportBluetooth = 1,
} FlipPassOutputActionPluginTransport;

typedef enum {
    FlipPassOutputActionPluginKindString = 0,
    FlipPassOutputActionPluginKindLogin,
    FlipPassOutputActionPluginKindVaultRef,
    FlipPassOutputActionPluginKindLoginRefs,
    FlipPassOutputActionPluginKindAutotype,
} FlipPassOutputActionPluginKind;

typedef enum {
    FlipPassOutputActionPluginRefPrimary = 0,
    FlipPassOutputActionPluginRefUsername,
    FlipPassOutputActionPluginRefPassword,
} FlipPassOutputActionPluginRef;

typedef bool (
    *FlipPassOutputActionChunkCallback)(const uint8_t* data, size_t data_size, void* context);

typedef struct {
    uint32_t api_version;
    FlipPassOutputActionPluginTransport transport;
    FlipPassOutputActionPluginKind action;
    const char* text;
    const char* username;
    const char* password;
    const char* autotype_sequence;
    const char* keyboard_layout_path;
    const char* entry_title;
    const char* entry_username;
    const char* entry_password;
    const char* entry_url;
    const char* entry_notes;
    const char* entry_uuid;
    const char* group_name;
    const char* group_path;
    const char* db_path;
    const char* db_dir;
    const char* db_name;
    const char* db_basename;
    const char* db_ext;
    size_t primary_ref_plain_len;
    size_t username_ref_plain_len;
    size_t password_ref_plain_len;
} FlipPassOutputActionRequestV1;

typedef struct {
    uint32_t api_version;
    void* context;
    void (*progress)(void* context, const char* stage, const char* detail, uint8_t percent);
    bool (*should_cancel)(void* context);
    bool (*begin_transport)(void* context, FlipPassOutputActionPluginTransport transport);
    void (*end_transport)(void* context, FlipPassOutputActionPluginTransport transport);
    bool (*press_key)(
        void* context,
        FlipPassOutputActionPluginTransport transport,
        uint16_t hid_key);
    bool (*release_key)(
        void* context,
        FlipPassOutputActionPluginTransport transport,
        uint16_t hid_key);
    void (*release_all)(void* context, FlipPassOutputActionPluginTransport transport);
    bool (*usb_numlock_on)(void* context);
    bool (*stream_ref)(
        void* context,
        FlipPassOutputActionPluginRef ref,
        FlipPassOutputActionChunkCallback callback,
        void* callback_context);
} FlipPassOutputActionHostApiV1;

typedef struct {
    uint32_t api_version;
    bool (*run)(
        const FlipPassOutputActionRequestV1* request,
        const FlipPassOutputActionHostApiV1* host_api,
        FuriString* error);
} FlipPassOutputActionPluginV1;

const FlipperAppPluginDescriptor* flippass_output_action_plugin_ep(void);

#ifdef __cplusplus
}
#endif
