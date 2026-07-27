#ifdef MF_ICR_HOST_TEST
#include "morse_flipper_icr_host_test.h"
#else
#include "morse_flipper_app_i.h"
#endif

#define MORSE_FLIPPER_ICR_PLUGIN_PATH APP_ASSETS_PATH("plugins/morse_flipper_icr.fal")

/* Caller holds plugin_mutex.  Result mirrors are snapshots, not edge events. */
static void morse_flipper_icr_host_apply_locked(
    MorseFlipperApp* app,
    MorseFlipperIcrResult result,
    uint32_t now_ms) {
    if(app == NULL) return;
    morse_flipper_plugin_runtime_apply_result_locked(app, result, now_ms);
}

bool morse_flipper_icr_host_enter(MorseFlipperApp* app, uint32_t now_ms) {
    MorseFlipperIcrResult initial = {0};
    bool entered;

    if(app == NULL || app->plugin_slot.mutex == NULL) return false;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    entered = morse_flipper_plugin_runtime_open_mapped_locked(
        app,
        MorseFlipperPluginOwnerIcr,
        0U,
        MORSE_FLIPPER_ICR_PLUGIN_PATH,
        MORSE_FLIPPER_ICR_API_VERSION,
        MORSE_FLIPPER_ICR_API_MAGIC,
        sizeof(MorseFlipperIcrApi),
        &(MorseFlipperIcrEnterArgs){
            .now_ms = now_ms,
            .rng_seed = now_ms ^ 0x49435231UL,
            .entry_kind = app->scene == MorseFlipperSceneMenuSettings ?
                              MorseFlipperIcrEntrySettings :
                              MorseFlipperIcrEntryTraining,
        },
        &initial);
    if(entered) morse_flipper_icr_host_apply_locked(app, initial, now_ms);
    furi_mutex_release(app->plugin_slot.mutex);
    if(initial.redraw) morse_flipper_view_dirty(app);
    return entered;
}

bool morse_flipper_icr_host_input(MorseFlipperApp* app, const InputEvent* event, uint32_t now_ms) {
    MorseFlipperMappedFalResult result = {0};

    if(app == NULL || event == NULL || app->plugin_slot.mutex == NULL) return false;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.owner == MorseFlipperPluginOwnerIcr && app->plugin_slot.api != NULL &&
       app->plugin_slot.state != NULL) {
        const MorseFlipperMappedFalApi* api = app->plugin_slot.api;
        result = api->input(app->plugin_slot.state, event, now_ms);
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
    if(result.redraw) morse_flipper_view_dirty(app);
    if(result.request_exit) morse_flipper_scene_back(app);
}
