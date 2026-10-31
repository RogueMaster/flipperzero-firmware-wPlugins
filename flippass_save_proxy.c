#include "flippass.h"
#include "plugins/flippass_save_host_plugin.h"

static const FlipPassSaveHostPluginV1* flippass_save_host_get(App* app, FuriString* error) {
    const FlipperAppPluginDescriptor* descriptor = flippass_module_ensure(
        app,
        FlipPassModuleSlotSaveHost,
        NULL,
        FLIPPASS_SAVE_HOST_PLUGIN_APP_ID,
        FLIPPASS_SAVE_HOST_PLUGIN_API_VERSION,
        error);
    const FlipPassSaveHostPluginV1* plugin = descriptor ? descriptor->entry_point : NULL;
    if(plugin == NULL || plugin->api_version != FLIPPASS_SAVE_HOST_PLUGIN_API_VERSION ||
       plugin->current_database == NULL || plugin->execute == NULL
#if FLIPPASS_ENABLE_DEBUG_SAVE_HOOK
       || plugin->debug_after_open == NULL
#endif
    ) {
        flippass_module_unload(app, FlipPassModuleSlotSaveHost);
        if(error != NULL && furi_string_empty(error)) {
            furi_string_set_str(error, "Not enough RAM is available to load the save module.");
        }
        return NULL;
    }
    flippass_module_pin(app, FlipPassModuleSlotSaveHost);
    return plugin;
}

static void flippass_save_host_release(App* app) {
    flippass_module_unpin(app, FlipPassModuleSlotSaveHost);
    /* A nested proxy call cannot unload the still-pinned outer frame. */
    flippass_module_unload(app, FlipPassModuleSlotSaveHost);
}

bool flippass_save_current_database(
    App* app,
    const char* path,
    const char* password,
    FuriString* error) {
    const FlipPassSaveHostPluginV1* plugin = flippass_save_host_get(app, error);
    if(plugin == NULL) return false;
    const bool ok = plugin->current_database(app, path, password, error);
    flippass_save_host_release(app);
    return ok;
}

bool flippass_save_execute(
    App* app,
    const char* path,
    const char* password,
    FlipPassKdbxCipher cipher,
    uint32_t compression,
    uint64_t rounds,
    FuriString* error) {
    const FlipPassSaveHostPluginV1* plugin = flippass_save_host_get(app, error);
    if(plugin == NULL) return false;
    const bool ok = plugin->execute(app, path, password, cipher, compression, rounds, error);
    flippass_save_host_release(app);
    return ok;
}

#if FLIPPASS_ENABLE_DEBUG_SAVE_HOOK
bool flippass_debug_save_after_open(App* app, FuriString* error) {
    const FlipPassSaveHostPluginV1* plugin = flippass_save_host_get(app, error);
    if(plugin == NULL) return false;
    const bool ok = plugin->debug_after_open(app, error);
    flippass_save_host_release(app);
    return ok;
}
#endif
