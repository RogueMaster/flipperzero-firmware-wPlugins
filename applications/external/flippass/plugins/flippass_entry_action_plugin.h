#pragma once

#include <furi.h>
#include <flipper_application/flipper_application.h>

#include "../flippass.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_ENTRY_ACTION_PLUGIN_APP_ID      "flippass_entry_action"
#define FLIPPASS_ENTRY_ACTION_PLUGIN_API_VERSION 1u
#define FLIPPASS_ENTRY_ACTION_HOST_API_VERSION   1u

typedef struct {
    uint32_t api_version;
    void* context;
    void (*deactivate_entry)(void* context);
    bool (*activate_entry)(void* context, KDBXEntry* entry, bool load_notes, FuriString* error);
    void (*clear_loaded_fields)(KDBXEntry* entry);
    bool (*load_vault_text)(
        KDBXVault* vault,
        const KDBXFieldRef* ref,
        char** out_text,
        size_t* out_size);
    bool (*generate_otp)(
        void* context,
        KDBXEntry* entry,
        FlipPassOtpKind kind,
        bool force_reload,
        char* out_code,
        FuriString* error);
    bool (*typing_should_cancel)(void* context);
    void (*output_release_all)(void* context);
    void (*output_cleanup)(void* context);
    void (*output_cleanup_transport)(void* context, FlipPassOutputTransport transport);
    bool (*output_bluetooth_is_connected)(void* context);
    bool (*output_bluetooth_is_advertising)(void* context);
    bool (*output_bluetooth_advertise)(void* context);
    bool (*output_usb_is_connected)(void* context);
    bool (*output_prewarm_transport)(void* context, FlipPassOutputTransport transport);
    bool (*output_type_string)(void* context, FlipPassOutputTransport transport, const char* text);
    bool (*output_type_login)(
        void* context,
        FlipPassOutputTransport transport,
        const char* username,
        const char* password);
    bool (*output_type_vault_ref)(
        void* context,
        FlipPassOutputTransport transport,
        KDBXVault* vault,
        const KDBXFieldRef* ref);
    bool (*output_type_login_refs)(
        void* context,
        FlipPassOutputTransport transport,
        KDBXVault* vault,
        const KDBXFieldRef* username_ref,
        const KDBXFieldRef* password_ref);
    bool (*output_type_autotype)(
        void* context,
        FlipPassOutputTransport transport,
        const KDBXEntry* entry);
    void (*secure_memzero)(void* data, size_t size);
} FlipPassEntryActionHostApiV1;

typedef struct {
    uint32_t api_version;
    void (*prepare_pending)(App* app, const FlipPassEntryActionHostApiV1* host_api);
    void (*prepare_type_menu)(App* app, const FlipPassEntryActionHostApiV1* host_api);
    void (*cleanup_type_menu)(App* app, const FlipPassEntryActionHostApiV1* host_api);
    bool (*execute_pending)(
        App* app,
        const FlipPassEntryActionHostApiV1* host_api,
        FuriString* error);
} FlipPassEntryActionPluginV1;

const FlipperAppPluginDescriptor* flippass_entry_action_plugin_ep(void);

#ifdef __cplusplus
}
#endif
