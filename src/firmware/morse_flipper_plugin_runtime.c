/* Shared lifecycle for the app's mutually exclusive embedded plugins. */

#include "morse_flipper_app_i.h"

static void morse_flipper_plugin_runtime_clear_locked(MorseFlipperApp* app) {
    if(app == NULL) return;
    app->plugin_slot.manager = NULL;
    app->plugin_slot.api = NULL;
    app->plugin_slot.state = NULL;
    app->plugin_slot.owner = MorseFlipperPluginOwnerNone;
    app->plugin_slot.error = MorseFlipperPluginErrorNone;
    app->plugin_slot.mode = 0U;
    app->plugin_slot.phase = 0U;
    app->plugin_slot.playback_active = false;
    app->plugin_slot.playback_mark = false;
    app->plugin_slot.prompt_visible = false;
    app->plugin_slot.prompt_char = 0U;
    app->plugin_slot.start_hold_mask = 0U;
}

bool morse_flipper_plugin_runtime_init(MorseFlipperApp* app) {
    if(app == NULL) return false;
    app->plugin_slot.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    return app->plugin_slot.mutex != NULL;
}

bool morse_flipper_plugin_runtime_claim_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner,
    uint8_t mode) {
    if(app == NULL || owner == MorseFlipperPluginOwnerNone ||
       app->plugin_slot.owner != MorseFlipperPluginOwnerNone)
        return false;
    morse_flipper_plugin_runtime_clear_locked(app);
    app->plugin_slot.owner = owner;
    app->plugin_slot.mode = mode;
    app->plugin_slot.error = MorseFlipperPluginErrorLoad;
    return true;
}

MorseFlipperPluginError morse_flipper_plugin_runtime_load_locked(
    const char* path,
    uint32_t api_version,
    PluginManager** manager_out,
    const void** entry_out) {
    PluginManager* manager;
    PluginManagerError error;

    if(manager_out != NULL) *manager_out = NULL;
    if(entry_out != NULL) *entry_out = NULL;
    if(path == NULL || manager_out == NULL || entry_out == NULL) return MorseFlipperPluginErrorState;
    manager = plugin_manager_alloc("morse_flipper", api_version, NULL);
    if(manager == NULL) return MorseFlipperPluginErrorState;
    error = plugin_manager_load_single(manager, path);
    if(error != PluginManagerErrorNone) {
        plugin_manager_free(manager);
        if(error == PluginManagerErrorApplicationIdMismatch) return MorseFlipperPluginErrorHostId;
        if(error == PluginManagerErrorAPIVersionMismatch) return MorseFlipperPluginErrorApiVersion;
        return MorseFlipperPluginErrorLoad;
    }
    if(plugin_manager_get_count(manager) != 1U) {
        plugin_manager_free(manager);
        return MorseFlipperPluginErrorTable;
    }
    *entry_out = plugin_manager_get_ep(manager, 0U);
    if(*entry_out == NULL) {
        plugin_manager_free(manager);
        return MorseFlipperPluginErrorTable;
    }
    *manager_out = manager;
    return MorseFlipperPluginErrorNone;
}

bool morse_flipper_plugin_runtime_publish_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner,
    PluginManager* manager,
    const void* api,
    void* state) {
    if(app == NULL || owner == MorseFlipperPluginOwnerNone || manager == NULL || api == NULL || state == NULL ||
       app->plugin_slot.owner != owner || app->plugin_slot.manager != NULL || app->plugin_slot.api != NULL ||
       app->plugin_slot.state != NULL)
        return false;
    app->plugin_slot.manager = manager;
    app->plugin_slot.api = api;
    app->plugin_slot.state = state;
    app->plugin_slot.error = MorseFlipperPluginErrorNone;
    return true;
}

void morse_flipper_plugin_runtime_release_claim_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner) {
    if(app == NULL || app->plugin_slot.owner != owner || app->plugin_slot.manager != NULL ||
       app->plugin_slot.api != NULL || app->plugin_slot.state != NULL)
        return;
    morse_flipper_plugin_runtime_clear_locked(app);
}

bool morse_flipper_plugin_runtime_snapshot(const MorseFlipperApp* app, MorseFlipperPluginSnapshot* out) {
    if(app == NULL || out == NULL || app->plugin_slot.mutex == NULL) return false;
    *out = (MorseFlipperPluginSnapshot){0};
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    out->owner = app->plugin_slot.owner;
    out->mode = app->plugin_slot.mode;
    out->phase = app->plugin_slot.phase;
    out->playback_active = app->plugin_slot.playback_active;
    out->playback_mark = app->plugin_slot.playback_mark;
    out->prompt_visible = app->plugin_slot.prompt_visible;
    out->prompt_char = app->plugin_slot.prompt_char;
    out->start_holdoff = app->plugin_slot.start_hold_mask != 0U;
    out->active = app->plugin_slot.error == MorseFlipperPluginErrorNone &&
                  app->plugin_slot.manager != NULL && app->plugin_slot.api != NULL &&
                  app->plugin_slot.state != NULL;
    furi_mutex_release(app->plugin_slot.mutex);
    return true;
}

void morse_flipper_plugin_runtime_unload_current(MorseFlipperApp* app) {
    if(app == NULL || app->plugin_slot.mutex == NULL) return;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.owner == MorseFlipperPluginOwnerContent)
        morse_flipper_content_host_unload_locked(app);
    else if(app->plugin_slot.owner == MorseFlipperPluginOwnerIcr)
        morse_flipper_icr_host_unload_locked(app);
    furi_mutex_release(app->plugin_slot.mutex);
}

void morse_flipper_plugin_runtime_deinit(MorseFlipperApp* app) {
    if(app == NULL) return;
    morse_flipper_plugin_runtime_unload_current(app);
    if(app->plugin_slot.mutex != NULL) {
        furi_mutex_free(app->plugin_slot.mutex);
        app->plugin_slot.mutex = NULL;
    }
}
