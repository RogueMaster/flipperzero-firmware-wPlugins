/* Shared lifecycle for the app's mutually exclusive embedded plugins. */

#include "morse_flipper_app_i.h"

static __attribute__((noinline)) bool morse_flipper_plugin_runtime_typed_api_valid(
    MorseFlipperPluginOwner owner,
    const void* entry) {
    if(entry == NULL) return false;
    if(owner == MorseFlipperPluginOwnerContent) {
        const MorseFlipperHelpAboutApi* api = entry;
        return api->enter != NULL && api->input != NULL;
    }
    if(owner == MorseFlipperPluginOwnerIcr) {
        const MorseFlipperIcrApi* api = entry;
        return api->enter != NULL && api->input != NULL;
    }
    if(owner == MorseFlipperPluginOwnerRxPractice) {
        const MfRxPracticeApi* api = entry;
        return api->enter != NULL && api->input != NULL && api->command != NULL &&
               api->feed_text != NULL;
    }
    if(owner == MorseFlipperPluginOwnerPassive) {
        const MfPassiveApi* api = entry;
        return api->mapped.enter != NULL && api->mapped.input != NULL;
    }
    return false;
}

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

bool morse_flipper_plugin_runtime_open_mapped_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner,
    uint8_t mode,
    const char* path,
    uint32_t api_version,
    uint32_t api_magic,
    uint32_t minimum_api_size,
    const void* enter_args,
    MorseFlipperMappedFalResult* initial) {
    PluginManager* manager = NULL;
    const MorseFlipperMappedFalApi* api = NULL;
    void* state = NULL;
    bool entered = false;

    if(initial != NULL) *initial = (MorseFlipperMappedFalResult){0};
    if(!morse_flipper_plugin_runtime_claim_locked(app, owner, mode)) return false;
    app->plugin_slot.error = morse_flipper_plugin_runtime_load_locked(
        path, api_version, &manager, (const void**)&api);
    if(app->plugin_slot.error != MorseFlipperPluginErrorNone) goto cleanup;
    if(api == NULL || api->magic != api_magic || api->api_version != api_version ||
       api->struct_size != minimum_api_size || api->alloc == NULL || api->free == NULL ||
       api->enter == NULL || api->leave == NULL || api->tick == NULL || api->draw == NULL ||
       !morse_flipper_plugin_runtime_typed_api_valid(owner, api)) {
        app->plugin_slot.error = MorseFlipperPluginErrorTable;
        goto cleanup;
    }
    state = api->alloc();
    if(state == NULL) {
        app->plugin_slot.error = MorseFlipperPluginErrorState;
        goto cleanup;
    }
    if(!api->enter(state, enter_args, initial)) {
        app->plugin_slot.error = MorseFlipperPluginErrorState;
        goto cleanup;
    }
    entered = true;
    if(morse_flipper_plugin_runtime_publish_locked(app, owner, manager, api, state)) return true;
    app->plugin_slot.error = MorseFlipperPluginErrorState;

cleanup:
    if(entered) api->leave(state);
    if(owner == MorseFlipperPluginOwnerPassive) {
        furi_hal_vibro_on(false);
        morse_flipper_audio_pwm_stop(&app->audio_pwm);
    }
    if(state != NULL && api != NULL) api->free(state);
    if(manager != NULL) plugin_manager_free(manager);
    return false;
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

void morse_flipper_plugin_runtime_detach_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner) {
    void* state;
    PluginManager* manager;
    const MorseFlipperMappedFalApi* api;
    if(app == NULL || app->plugin_slot.owner != owner) return;
    state = app->plugin_slot.state;
    manager = app->plugin_slot.manager;
    api = app->plugin_slot.api;
    app->plugin_slot.api = NULL;
    app->plugin_slot.state = NULL;
    app->plugin_slot.manager = NULL;
    if(state != NULL) {
        if(api != NULL && api->leave != NULL) api->leave(state);
        if(owner == MorseFlipperPluginOwnerPassive) {
            furi_hal_vibro_on(false);
            morse_flipper_audio_pwm_stop(&app->audio_pwm);
        }
        if(api != NULL && api->free != NULL) api->free(state);
    }
    if(manager != NULL) plugin_manager_free(manager);
    morse_flipper_plugin_runtime_release_claim_locked(app, owner);
    app->session_result_tone = false;
    app->session_result_good = false;
    app->session_result_until = 0U;
}

bool morse_flipper_plugin_runtime_tick_locked(
    MorseFlipperApp* app,
    MorseFlipperPluginOwner owner,
    uint32_t now_ms,
    MorseFlipperMappedFalResult* result) {
    const MorseFlipperMappedFalApi* api;
    if(result != NULL) *result = (MorseFlipperMappedFalResult){0};
    if(app == NULL || result == NULL || app->plugin_slot.owner != owner ||
       app->plugin_slot.error != MorseFlipperPluginErrorNone || app->plugin_slot.api == NULL ||
       app->plugin_slot.state == NULL)
        return false;
    api = app->plugin_slot.api;
    *result = api->tick(app->plugin_slot.state, now_ms);
    return true;
}

void morse_flipper_plugin_runtime_apply_result_locked(
    MorseFlipperApp* app,
    MorseFlipperMappedFalResult result,
    uint32_t now_ms) {
    if(app == NULL) return;
    app->plugin_slot.phase = result.phase;
    app->plugin_slot.playback_active = result.playback_active;
    app->plugin_slot.playback_mark = result.playback_mark;
    morse_flipper_plugin_feedback_locked(app, result.feedback, now_ms);
}

void morse_flipper_plugin_runtime_draw(MorseFlipperApp* app, Canvas* canvas, uint32_t now_ms) {
    const MorseFlipperMappedFalApi* api;
    if(app == NULL || canvas == NULL || app->plugin_slot.mutex == NULL) return;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.error == MorseFlipperPluginErrorNone && app->plugin_slot.api != NULL &&
       app->plugin_slot.state != NULL) {
        api = app->plugin_slot.api;
        api->draw(app->plugin_slot.state, canvas, now_ms);
    } else if(
        app->plugin_slot.error != MorseFlipperPluginErrorNone ||
        app->plugin_slot.owner != MorseFlipperPluginOwnerNone) {
        morse_flipper_draw_plugin_unavailable(canvas);
    }
    furi_mutex_release(app->plugin_slot.mutex);
}

void morse_flipper_plugin_feedback_locked(
    MorseFlipperApp* app,
    uint8_t feedback,
    uint32_t now_ms) {
    if(app == NULL || feedback == 0U) return;
    app->session_result_tone = feedback >= 3U;
    app->session_result_good = feedback == 2U;
    app->session_result_until =
        feedback == 1U ? 0U : now_ms + MORSE_FLIPPER_SESSION_RESULT_MS;
}

void morse_flipper_plugin_feedback_expire_locked(
    MorseFlipperApp* app,
    uint32_t now_ms) {
    if(app != NULL && (app->session_result_tone || app->session_result_good) &&
       morse_flipper_time_reached(now_ms, app->session_result_until)) {
        app->session_result_tone = false;
        app->session_result_good = false;
        app->session_result_until = 0U;
    }
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
    if(app->plugin_slot.owner != MorseFlipperPluginOwnerNone)
        morse_flipper_plugin_runtime_detach_locked(app, app->plugin_slot.owner);
    furi_mutex_release(app->plugin_slot.mutex);
    morse_flipper_drop_live_keying_for_playback(app, furi_get_tick());
    morse_flipper_reset_answer_decoder(app);
    morse_flipper_release_all_notes(app);
    morse_flipper_update_sidetone(app);
}

void morse_flipper_plugin_runtime_deinit(MorseFlipperApp* app) {
    if(app == NULL) return;
    morse_flipper_plugin_runtime_unload_current(app);
    if(app->plugin_slot.mutex != NULL) {
        furi_mutex_free(app->plugin_slot.mutex);
        app->plugin_slot.mutex = NULL;
    }
}
