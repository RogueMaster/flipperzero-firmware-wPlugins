/**
 * @file flippass_scene_send_confirm.c
 * @brief Thin host gateway for deferred credential typing actions.
 */
#include "flippass_scene_send_confirm.h"
#include "../flippass.h"
#include "../flippass_db.h"
#include "../kdbx/memzero.h"
#include "../plugins/flippass_entry_action_plugin.h"

static void flippass_entry_action_host_deactivate_entry(void* context) {
    flippass_db_deactivate_entry(context);
}

static bool flippass_entry_action_host_activate_entry(
    void* context,
    KDBXEntry* entry,
    bool load_notes,
    FuriString* error) {
    return flippass_db_activate_entry(context, entry, load_notes, error);
}

static bool flippass_entry_action_host_generate_otp(
    void* context,
    KDBXEntry* entry,
    FlipPassOtpKind kind,
    bool force_reload,
    char* out_code,
    FuriString* error) {
    return flippass_otp_generate_code(context, entry, kind, force_reload, out_code, error);
}

static bool flippass_entry_action_host_typing_should_cancel(void* context) {
    return flippass_typing_should_cancel(context);
}

static void flippass_entry_action_host_output_release_all(void* context) {
    flippass_output_release_all(context);
}

static void flippass_entry_action_host_output_cleanup(void* context) {
    flippass_output_cleanup(context);
}

static void flippass_entry_action_host_output_cleanup_transport(
    void* context,
    FlipPassOutputTransport transport) {
    flippass_output_cleanup_transport(context, transport);
}

static bool flippass_entry_action_host_output_bluetooth_is_connected(void* context) {
    return flippass_output_bluetooth_is_connected(context);
}

static bool flippass_entry_action_host_output_bluetooth_is_advertising(void* context) {
    return flippass_output_bluetooth_is_advertising(context);
}

static bool flippass_entry_action_host_output_bluetooth_advertise(void* context) {
    return flippass_output_bluetooth_advertise(context);
}

static bool flippass_entry_action_host_output_usb_is_connected(void* context) {
    return flippass_output_usb_is_connected(context);
}

static bool flippass_entry_action_host_output_prewarm_transport(
    void* context,
    FlipPassOutputTransport transport) {
    return flippass_output_prewarm_transport(context, transport);
}

static bool flippass_entry_action_host_output_type_string(
    void* context,
    FlipPassOutputTransport transport,
    const char* text) {
    return flippass_output_type_string(context, transport, text);
}

static bool flippass_entry_action_host_output_type_login(
    void* context,
    FlipPassOutputTransport transport,
    const char* username,
    const char* password) {
    return flippass_output_type_login(context, transport, username, password);
}

static bool flippass_entry_action_host_output_type_vault_ref(
    void* context,
    FlipPassOutputTransport transport,
    KDBXVault* vault,
    const KDBXFieldRef* ref) {
    return flippass_output_type_vault_ref(context, transport, vault, ref);
}

static bool flippass_entry_action_host_output_type_login_refs(
    void* context,
    FlipPassOutputTransport transport,
    KDBXVault* vault,
    const KDBXFieldRef* username_ref,
    const KDBXFieldRef* password_ref) {
    return flippass_output_type_login_refs(context, transport, vault, username_ref, password_ref);
}

static bool flippass_entry_action_host_output_type_autotype(
    void* context,
    FlipPassOutputTransport transport,
    const KDBXEntry* entry) {
    return flippass_output_type_autotype(context, transport, entry);
}

static FlipPassEntryActionHostApiV1 flippass_entry_action_host_api(App* app) {
    return (FlipPassEntryActionHostApiV1){
        .api_version = FLIPPASS_ENTRY_ACTION_HOST_API_VERSION,
        .context = app,
        .deactivate_entry = flippass_entry_action_host_deactivate_entry,
        .activate_entry = flippass_entry_action_host_activate_entry,
        .clear_loaded_fields = kdbx_entry_clear_loaded_fields,
        .load_vault_text = kdbx_vault_load_text,
        .generate_otp = flippass_entry_action_host_generate_otp,
        .typing_should_cancel = flippass_entry_action_host_typing_should_cancel,
        .output_release_all = flippass_entry_action_host_output_release_all,
        .output_cleanup = flippass_entry_action_host_output_cleanup,
        .output_cleanup_transport = flippass_entry_action_host_output_cleanup_transport,
        .output_bluetooth_is_connected = flippass_entry_action_host_output_bluetooth_is_connected,
        .output_bluetooth_is_advertising =
            flippass_entry_action_host_output_bluetooth_is_advertising,
        .output_bluetooth_advertise = flippass_entry_action_host_output_bluetooth_advertise,
        .output_usb_is_connected = flippass_entry_action_host_output_usb_is_connected,
        .output_prewarm_transport = flippass_entry_action_host_output_prewarm_transport,
        .output_type_string = flippass_entry_action_host_output_type_string,
        .output_type_login = flippass_entry_action_host_output_type_login,
        .output_type_vault_ref = flippass_entry_action_host_output_type_vault_ref,
        .output_type_login_refs = flippass_entry_action_host_output_type_login_refs,
        .output_type_autotype = flippass_entry_action_host_output_type_autotype,
        .secure_memzero = memzero,
    };
}

static const FlipPassEntryActionPluginV1*
    flippass_entry_action_plugin_get(App* app, FuriString* error) {
    const FlipperAppPluginDescriptor* descriptor = flippass_module_ensure(
        app,
        FlipPassModuleSlotEntryAction,
        NULL,
        FLIPPASS_ENTRY_ACTION_PLUGIN_APP_ID,
        FLIPPASS_ENTRY_ACTION_PLUGIN_API_VERSION,
        error);
    if(descriptor == NULL || descriptor->entry_point == NULL) {
        if(error != NULL && furi_string_empty(error)) {
            furi_string_set_str(error, "FlipPass typing action plugin is unavailable.");
        }
        return NULL;
    }

    const FlipPassEntryActionPluginV1* plugin = descriptor->entry_point;
    if(plugin->api_version != FLIPPASS_ENTRY_ACTION_PLUGIN_API_VERSION ||
       plugin->prepare_pending == NULL || plugin->prepare_type_menu == NULL ||
       plugin->cleanup_type_menu == NULL || plugin->execute_pending == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass typing action plugin has an incompatible API.");
        }
        flippass_module_unload(app, FlipPassModuleSlotEntryAction);
        return NULL;
    }

    flippass_module_pin(app, FlipPassModuleSlotEntryAction);
    return plugin;
}

static const FlipPassEntryActionPluginV1* flippass_entry_action_try_load(App* app) {
    FuriString* error = furi_string_alloc();
    const FlipPassEntryActionPluginV1* plugin = flippass_entry_action_plugin_get(app, error);
    if(plugin == NULL) {
        FLIPPASS_LOG_EVENT(
            app, "ENTRY_ACTION_PLUGIN_LOAD_FAIL reason=%s", furi_string_get_cstr(error));
    }
    furi_string_free(error);
    return plugin;
}

void flippass_entry_action_prepare_pending(App* app) {
    furi_assert(app);

    const FlipPassEntryActionPluginV1* plugin = flippass_entry_action_try_load(app);
    if(plugin != NULL) {
        FlipPassEntryActionHostApiV1 host_api = flippass_entry_action_host_api(app);
        plugin->prepare_pending(app, &host_api);
        flippass_module_unpin(app, FlipPassModuleSlotEntryAction);
    }
    flippass_module_unload(app, FlipPassModuleSlotEntryAction);
}

void flippass_entry_action_prepare_type_menu(App* app) {
    furi_assert(app);

    const FlipPassEntryActionPluginV1* plugin = flippass_entry_action_try_load(app);
    if(plugin != NULL) {
        FlipPassEntryActionHostApiV1 host_api = flippass_entry_action_host_api(app);
        plugin->prepare_type_menu(app, &host_api);
        flippass_module_unpin(app, FlipPassModuleSlotEntryAction);
    }
    flippass_module_unload(app, FlipPassModuleSlotEntryAction);
}

void flippass_entry_action_cleanup_type_menu(App* app) {
    furi_assert(app);

    /* Cleanup must not map a cold action module just to release transports.
     * That temporary load can evict the session editor during a scene exit. */
    app->entry_action_keep_type_menu_prewarm = false;
    app->entry_action_restore_type_drawer = false;
    const FlipPassEntryActionPluginV1* plugin =
        app->module_loader.slot[FlipPassModuleSlotEntryAction].descriptor != NULL ?
            flippass_entry_action_try_load(app) :
            NULL;
    if(plugin != NULL) {
        FlipPassEntryActionHostApiV1 host_api = flippass_entry_action_host_api(app);
        plugin->cleanup_type_menu(app, &host_api);
        flippass_module_unpin(app, FlipPassModuleSlotEntryAction);
    } else {
        flippass_output_release_all(app);
        flippass_output_cleanup(app);
    }
    flippass_module_unload(app, FlipPassModuleSlotEntryAction);
}

bool flippass_entry_action_execute_pending(App* app, FuriString* error) {
    furi_assert(app);

    const FlipPassEntryActionPluginV1* plugin = flippass_entry_action_plugin_get(app, error);
    bool ok = false;
    if(plugin != NULL) {
        FlipPassEntryActionHostApiV1 host_api = flippass_entry_action_host_api(app);
        ok = plugin->execute_pending(app, &host_api, error);
        flippass_module_unpin(app, FlipPassModuleSlotEntryAction);
    }
    flippass_module_unload(app, FlipPassModuleSlotEntryAction);
    return ok;
}
