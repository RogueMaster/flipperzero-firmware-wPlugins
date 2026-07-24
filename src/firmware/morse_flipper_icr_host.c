#include "morse_flipper_app_i.h"

#include <flipper_application/plugins/plugin_manager.h>

#define MORSE_FLIPPER_ICR_PLUGIN_PATH APP_ASSETS_PATH("plugins/morse_flipper_icr.fal")

static bool morse_flipper_icr_api_valid(const MorseFlipperIcrApi* api) {
    return api != NULL && api->mapped.magic == MORSE_FLIPPER_ICR_API_MAGIC &&
           api->mapped.api_version == MORSE_FLIPPER_ICR_API_VERSION &&
           api->mapped.struct_size >= sizeof(MorseFlipperIcrApi) && api->mapped.alloc != NULL &&
           api->mapped.free != NULL && api->enter != NULL && api->mapped.leave != NULL &&
           api->input != NULL && api->mapped.tick != NULL && api->mapped.draw != NULL;
}

/* Caller holds plugin_mutex.  Result mirrors are snapshots, not edge events. */
static void morse_flipper_icr_host_apply_locked(
    MorseFlipperApp* app,
    MorseFlipperIcrResult result,
    uint32_t now_ms) {
    if(app == NULL) return;
    morse_flipper_plugin_runtime_apply_result_locked(app, result, now_ms);
}

void morse_flipper_icr_host_unload(MorseFlipperApp* app) {
    if(app == NULL || app->plugin_slot.mutex == NULL) return;
    morse_flipper_plugin_runtime_unload_current(app);
}

bool morse_flipper_icr_host_enter(MorseFlipperApp* app, uint32_t now_ms) {
    PluginManager* manager = NULL;
    const MorseFlipperIcrApi* api = NULL;
    void* state = NULL;
    MorseFlipperIcrResult initial = {0};
    bool entered = false;

    if(app == NULL || app->plugin_slot.mutex == NULL) return false;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(!morse_flipper_plugin_runtime_claim_locked(app, MorseFlipperPluginOwnerIcr, 0U)) goto cleanup;
    app->plugin_slot.error = morse_flipper_plugin_runtime_load_locked(
        MORSE_FLIPPER_ICR_PLUGIN_PATH,
        MORSE_FLIPPER_ICR_API_VERSION,
        &manager,
        (const void**)&api);
    if(app->plugin_slot.error != MorseFlipperPluginErrorNone) goto cleanup;
    if(!morse_flipper_icr_api_valid(api)) {
        app->plugin_slot.error = MorseFlipperPluginErrorTable;
        goto cleanup;
    }
    state = api->mapped.alloc();
    if(state == NULL) {
        app->plugin_slot.error = MorseFlipperPluginErrorState;
        goto cleanup;
    }
    if(!api->enter(
           state,
           &(MorseFlipperIcrEnterArgs){.now_ms = now_ms, .rng_seed = now_ms ^ 0x49435231UL},
           &initial))
        {
            app->plugin_slot.error = MorseFlipperPluginErrorState;
            goto cleanup;
        }
    entered = true;
    if(!morse_flipper_plugin_runtime_publish_locked(
           app, MorseFlipperPluginOwnerIcr, manager, api, state)) {
        app->plugin_slot.error = MorseFlipperPluginErrorState;
        goto cleanup;
    }
    morse_flipper_icr_host_apply_locked(app, initial, now_ms);
    furi_mutex_release(app->plugin_slot.mutex);
    morse_flipper_update_sidetone(app);
    if(initial.redraw) morse_flipper_view_dirty(app);
    return true;

cleanup:
    if(entered && api != NULL && state != NULL) api->mapped.leave(state);
    if(api != NULL && state != NULL) api->mapped.free(state);
    if(manager != NULL) plugin_manager_free(manager);
    furi_mutex_release(app->plugin_slot.mutex);
    morse_flipper_update_sidetone(app);
    return false;
}

bool morse_flipper_icr_host_input(MorseFlipperApp* app, const InputEvent* event, uint32_t now_ms) {
    MorseFlipperIcrResult result = {0};

    if(app == NULL || event == NULL || app->plugin_slot.mutex == NULL) return false;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.owner == MorseFlipperPluginOwnerIcr && app->plugin_slot.api != NULL &&
       app->plugin_slot.state != NULL) {
        result = ((const MorseFlipperIcrApi*)app->plugin_slot.api)->input(app->plugin_slot.state, event, now_ms);
        if(result.handled) morse_flipper_icr_host_apply_locked(app, result, now_ms);
    }
    furi_mutex_release(app->plugin_slot.mutex);
    morse_flipper_update_sidetone(app);
    if(result.handled && result.redraw) morse_flipper_view_dirty(app);
    if(!result.handled) {
        if(event->key == InputKeyBack &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            morse_flipper_scene_back(app);
            return true;
        }
        return false;
    }
    if(result.request_exit) morse_flipper_scene_back(app);
    return true;
}

void morse_flipper_icr_host_tick(MorseFlipperApp* app, uint32_t now_ms) {
    MorseFlipperIcrResult result = {0};
    if(app == NULL || app->plugin_slot.mutex == NULL) return;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    morse_flipper_plugin_feedback_expire_locked(app, now_ms);
    if(app->plugin_slot.owner == MorseFlipperPluginOwnerIcr && app->plugin_slot.api != NULL &&
       app->plugin_slot.state != NULL) {
        morse_flipper_plugin_runtime_tick_locked(
            app, MorseFlipperPluginOwnerIcr, now_ms, &result);
        if(result.handled) morse_flipper_icr_host_apply_locked(app, result, now_ms);
    }
    furi_mutex_release(app->plugin_slot.mutex);
    morse_flipper_update_sidetone(app);
    if(result.handled && result.redraw) morse_flipper_view_dirty(app);
}
