#include "morse_flipper_app_i.h"

#include <flipper_application/plugins/plugin_manager.h>

#define MORSE_FLIPPER_ICR_PLUGIN_PATH APP_ASSETS_PATH("plugins/morse_flipper_icr.fal")

static bool morse_flipper_icr_api_valid(const MorseFlipperIcrApi* api) {
    return api != NULL && api->magic == MORSE_FLIPPER_ICR_API_MAGIC &&
           api->api_version == MORSE_FLIPPER_ICR_API_VERSION &&
           api->struct_size >= sizeof(MorseFlipperIcrApi) && api->alloc != NULL &&
           api->free != NULL && api->enter != NULL && api->leave != NULL &&
           api->input != NULL && api->tick != NULL && api->draw != NULL;
}

/* Caller holds plugin_mutex, so no stale plugin result can re-enable a gate. */
static void morse_flipper_icr_host_clear_locked(MorseFlipperApp* app) {
    bool audio_changed;

    if(app == NULL) return;

    audio_changed = app->icr_playback_mark || app->session_result_tone ||
                    app->session_result_good || app->session_result_until != 0U;
    app->icr_playback_active = false;
    app->icr_playback_mark = false;
    app->icr_prompt_visible = false;
    app->icr_prompt_char = 0U;
    app->session_result_tone = false;
    app->session_result_good = false;
    app->session_result_until = 0U;
    if(audio_changed) morse_flipper_update_sidetone(app);
}

/* Caller holds plugin_mutex.  Result mirrors are snapshots, not edge events. */
static void morse_flipper_icr_host_apply_locked(MorseFlipperApp* app, MorseFlipperIcrResult result) {
    bool audio_changed;

    if(app == NULL) return;

    audio_changed = app->icr_playback_mark != result.playback_mark;
    app->icr_playback_active = result.playback_active;
    app->icr_playback_mark = result.playback_mark;
    app->icr_prompt_visible = result.prompt_visible;
    app->icr_prompt_char = result.prompt_char;
    switch(result.feedback) {
    case MorseFlipperIcrFeedbackGood:
        app->session_result_tone = false;
        app->session_result_good = true;
        app->session_result_until = furi_get_tick() + MORSE_FLIPPER_SESSION_RESULT_MS;
        audio_changed = true;
        break;
    case MorseFlipperIcrFeedbackFail:
        app->session_result_good = false;
        morse_flipper_feedback_fail(app);
        break;
    case MorseFlipperIcrFeedbackTimeout:
        app->session_result_good = false;
        morse_flipper_feedback_timeout(app);
        break;
    case MorseFlipperIcrFeedbackClear:
        audio_changed = audio_changed || app->session_result_tone || app->session_result_good ||
                        app->session_result_until != 0U;
        app->session_result_tone = false;
        app->session_result_good = false;
        app->session_result_until = 0U;
        break;
    case MorseFlipperIcrFeedbackNone:
    default:
        break;
    }
    if(audio_changed) morse_flipper_update_sidetone(app);
    if(result.redraw) morse_flipper_view_dirty(app);
}

void morse_flipper_icr_host_unload(MorseFlipperApp* app) {
    const MorseFlipperIcrApi* api;
    void* state;
    PluginManager* manager;

    if(app == NULL || app->plugin_mutex == NULL) return;
    furi_mutex_acquire(app->plugin_mutex, FuriWaitForever);
    api = app->icr_api;
    state = app->icr_state;
    manager = app->icr_manager;
    app->icr_api = NULL;
    app->icr_state = NULL;
    app->icr_manager = NULL;
    app->icr_active = false;
    if(api != NULL && state != NULL) {
        api->leave(state);
        api->free(state);
    }
    if(manager != NULL) plugin_manager_free(manager);
    morse_flipper_icr_host_clear_locked(app);
    furi_mutex_release(app->plugin_mutex);
}

bool morse_flipper_icr_host_enter(MorseFlipperApp* app, uint32_t now_ms) {
    PluginManager* manager = NULL;
    const MorseFlipperIcrApi* api = NULL;
    void* state = NULL;
    PluginManagerError error;
    MorseFlipperIcrResult initial = {0};
    bool entered = false;

    if(app == NULL || app->plugin_mutex == NULL) return false;
    furi_mutex_acquire(app->plugin_mutex, FuriWaitForever);
    if(app->content_active || app->icr_active) goto cleanup;
    manager = plugin_manager_alloc("morse_flipper", MORSE_FLIPPER_ICR_API_VERSION, NULL);
    if(manager == NULL) goto cleanup;
    error = plugin_manager_load_single(manager, MORSE_FLIPPER_ICR_PLUGIN_PATH);
    if(error != PluginManagerErrorNone || plugin_manager_get_count(manager) != 1U) goto cleanup;
    api = plugin_manager_get_ep(manager, 0U);
    if(!morse_flipper_icr_api_valid(api)) goto cleanup;
    state = api->alloc();
    if(state == NULL) goto cleanup;
    if(!api->enter(
           state,
           &(MorseFlipperIcrEnterArgs){.now_ms = now_ms, .rng_seed = now_ms ^ 0x49435231UL},
           &initial))
        goto cleanup;
    entered = true;
    app->icr_manager = manager;
    app->icr_api = api;
    app->icr_state = state;
    app->icr_active = true;
    morse_flipper_icr_host_apply_locked(app, initial);
    furi_mutex_release(app->plugin_mutex);
    return true;

cleanup:
    if(entered && api != NULL && state != NULL) api->leave(state);
    if(api != NULL && state != NULL) api->free(state);
    if(manager != NULL) plugin_manager_free(manager);
    furi_mutex_release(app->plugin_mutex);
    return false;
}

bool morse_flipper_icr_host_input(MorseFlipperApp* app, const InputEvent* event, uint32_t now_ms) {
    MorseFlipperIcrResult result = {0};

    if(app == NULL || event == NULL || app->plugin_mutex == NULL) return false;
    furi_mutex_acquire(app->plugin_mutex, FuriWaitForever);
    if(app->icr_active && app->icr_api != NULL && app->icr_state != NULL) {
        result = app->icr_api->input(app->icr_state, event, now_ms);
        if(result.handled) morse_flipper_icr_host_apply_locked(app, result);
    }
    furi_mutex_release(app->plugin_mutex);
    if(!result.handled) {
        if(event->key == InputKeyBack &&
           (event->type == InputTypeShort || event->type == InputTypeLong)) {
            morse_flipper_scene_back(app);
            return true;
        }
        return false;
    }
    if(result.request_back) morse_flipper_scene_back(app);
    return true;
}

void morse_flipper_icr_host_tick(MorseFlipperApp* app, uint32_t now_ms) {
    MorseFlipperIcrResult result = {0};
    if(app == NULL || app->plugin_mutex == NULL) return;
    furi_mutex_acquire(app->plugin_mutex, FuriWaitForever);
    if(app->icr_active && app->icr_api != NULL && app->icr_state != NULL) {
        result = app->icr_api->tick(app->icr_state, now_ms);
        if(result.handled) morse_flipper_icr_host_apply_locked(app, result);
    }
    furi_mutex_release(app->plugin_mutex);
}

void morse_flipper_icr_host_draw(MorseFlipperApp* app, Canvas* canvas) {
    MorseFlipperIcrDrawResult result = {0};
    if(app == NULL || canvas == NULL || app->plugin_mutex == NULL) return;
    furi_mutex_acquire(app->plugin_mutex, FuriWaitForever);
    if(app->icr_active && app->icr_api != NULL && app->icr_state != NULL) {
        result = app->icr_api->draw(app->icr_state, canvas, furi_get_tick());
        if(result.draw_prompt)
            morse_flipper_draw_straight_prompt(
                canvas, app, result.prompt_cx, result.prompt_cy, (char)result.prompt_char);
        furi_mutex_release(app->plugin_mutex);
        return;
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "ICR unavailable");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 39, AlignCenter, AlignCenter, "Plugin missing/corrupt");
    canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "Back");
    furi_mutex_release(app->plugin_mutex);
}
