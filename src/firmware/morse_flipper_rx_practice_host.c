#include "morse_flipper_app_i.h"

#define MORSE_FLIPPER_RX_PRACTICE_PLUGIN_PATH APP_ASSETS_PATH("plugins/morse_flipper_rx_practice.fal")

static bool mf_rx_api_valid(const MfRxPracticeApi* api) {
    return api != NULL && api->magic == MORSE_FLIPPER_RX_PRACTICE_API_MAGIC &&
           api->api_version == MORSE_FLIPPER_RX_PRACTICE_API_VERSION &&
           api->struct_size >= sizeof(*api) && api->alloc != NULL && api->free != NULL &&
           api->enter != NULL && api->leave != NULL && api->command != NULL && api->feed_text != NULL &&
           api->tick != NULL && api->draw != NULL;
}

static void mf_rx_apply_locked(MorseFlipperApp* app, MfRxPracticeResult result) {
    app->plugin_slot.phase = result.phase;
    app->plugin_slot.playback_active = result.phase == MfRxPracticePhasePlayback;
    app->plugin_slot.playback_mark = result.playback_mark;
    if(result.feedback == MfRxPracticeFeedbackPass) {
        app->session_result_tone = false;
        app->session_result_good = true;
        app->session_result_until = furi_get_tick() + MORSE_FLIPPER_SESSION_RESULT_MS;
    } else if(result.feedback == MfRxPracticeFeedbackFail) {
        app->session_result_good = false;
        morse_flipper_feedback_fail(app);
    } else if(result.feedback == MfRxPracticeFeedbackTimeout) {
        app->session_result_good = false;
        morse_flipper_feedback_timeout(app);
    } else if(result.feedback == MfRxPracticeFeedbackClear) {
        app->session_result_tone = false;
        app->session_result_good = false;
        app->session_result_until = 0U;
    }
    if(result.redraw) morse_flipper_view_dirty(app);
}

void morse_flipper_rx_practice_host_unload_locked(MorseFlipperApp* app) {
    const MfRxPracticeApi* api;
    void* state;
    PluginManager* manager;
    if(app == NULL || app->plugin_slot.owner != MorseFlipperPluginOwnerRxPractice) return;
    api = app->plugin_slot.api;
    state = app->plugin_slot.state;
    manager = app->plugin_slot.manager;
    app->plugin_slot.api = NULL;
    app->plugin_slot.state = NULL;
    app->plugin_slot.manager = NULL;
    app->plugin_slot.playback_active = false;
    app->plugin_slot.playback_mark = false;
    if(api != NULL && state != NULL) {
        api->leave(state);
        api->free(state);
    }
    if(manager != NULL) plugin_manager_free(manager);
    morse_flipper_plugin_runtime_release_claim_locked(app, MorseFlipperPluginOwnerRxPractice);
    app->session_result_tone = false;
    app->session_result_good = false;
    app->session_result_until = 0U;
}

void morse_flipper_rx_practice_host_unload(MorseFlipperApp* app) {
    if(app == NULL || app->plugin_slot.mutex == NULL) return;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    morse_flipper_rx_practice_host_unload_locked(app);
    furi_mutex_release(app->plugin_slot.mutex);
    morse_flipper_reset_answer_decoder(app);
    morse_flipper_release_all_notes(app);
    morse_flipper_update_sidetone(app);
}

bool morse_flipper_rx_practice_host_enter(MorseFlipperApp* app, MfRxPracticeMode mode, uint32_t now_ms) {
    PluginManager* manager = NULL;
    const MfRxPracticeApi* api = NULL;
    void* state = NULL;
    MfRxPracticeResult initial = {0};
    MfRxPracticeEnterArgs args;
    if(app == NULL || app->plugin_slot.mutex == NULL) return false;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(!morse_flipper_plugin_runtime_claim_locked(app, MorseFlipperPluginOwnerRxPractice, mode)) {
        furi_mutex_release(app->plugin_slot.mutex);
        return false;
    }
    app->plugin_slot.error = morse_flipper_plugin_runtime_load_locked(
        MORSE_FLIPPER_RX_PRACTICE_PLUGIN_PATH,
        MORSE_FLIPPER_RX_PRACTICE_API_VERSION,
        &manager,
        (const void**)&api);
    if(app->plugin_slot.error != MorseFlipperPluginErrorNone || !mf_rx_api_valid(api)) {
        if(app->plugin_slot.error == MorseFlipperPluginErrorNone)
            app->plugin_slot.error = MorseFlipperPluginErrorTable;
        goto fail;
    }
    state = api->alloc();
    if(state == NULL) {
        app->plugin_slot.error = MorseFlipperPluginErrorState;
        goto fail;
    }
    args = (MfRxPracticeEnterArgs){
        .struct_size = sizeof(args),
        .mode = mode,
        .now_ms = now_ms,
        .rng_seed = furi_hal_random_get(),
        .answer_timeout_ms = (uint32_t)(app->trainer_answer_timeout_s == 0U ?
              MORSE_FLIPPER_TRAINER_TIMEOUT_DEFAULT_S : app->trainer_answer_timeout_s) * 1000U,
        .result_hold_ms = 3000U,
        .dit_ms = morse_flipper_current_dit_ms(app),
        .char_gap_ms = morse_flipper_training_char_gap_ms(
            morse_flipper_current_dit_ms(app), morse_flipper_local_wpm(app), app->trainer_farnsworth_wpm),
        .physical_key_can_start = app->input_source != MorseFlipperInputSourceButtons,
    };
    if(!api->enter(state, &args, &initial) ||
       !morse_flipper_plugin_runtime_publish_locked(
           app, MorseFlipperPluginOwnerRxPractice, manager, api, state)) {
        app->plugin_slot.error = MorseFlipperPluginErrorState;
        goto fail;
    }
    mf_rx_apply_locked(app, initial);
    furi_mutex_release(app->plugin_slot.mutex);
    morse_flipper_update_sidetone(app);
    morse_flipper_reset_answer_decoder(app);
    return true;
fail:
    if(api != NULL && state != NULL) api->free(state);
    if(manager != NULL) plugin_manager_free(manager);
    furi_mutex_release(app->plugin_slot.mutex);
    morse_flipper_reset_answer_decoder(app);
    return false;
}

static bool mf_rx_call_locked(MorseFlipperApp* app, MfRxPracticeResult* result, uint8_t operation, const char* text, size_t length, MfRxPracticeCommand command, uint32_t now_ms) {
    const MfRxPracticeApi* api;
    if(app->plugin_slot.owner != MorseFlipperPluginOwnerRxPractice || app->plugin_slot.api == NULL ||
       app->plugin_slot.state == NULL)
        return false;
    api = app->plugin_slot.api;
    if(operation == 0U) *result = api->command(app->plugin_slot.state, command, now_ms);
    else if(operation == 1U) *result = api->feed_text(app->plugin_slot.state, text, length, now_ms);
    else *result = api->tick(app->plugin_slot.state, now_ms);
    mf_rx_apply_locked(app, *result);
    return true;
}

bool morse_flipper_rx_practice_host_command(MorseFlipperApp* app, MfRxPracticeCommand command, uint32_t now_ms) {
    MfRxPracticeResult result = {0};
    bool ok;
    if(app == NULL || app->plugin_slot.mutex == NULL) return false;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    ok = mf_rx_call_locked(app, &result, 0U, NULL, 0U, command, now_ms);
    furi_mutex_release(app->plugin_slot.mutex);
    morse_flipper_update_sidetone(app);
    if(ok && result.request_exit)
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MorseFlipperSceneMenuTraining);
    return ok;
}

bool morse_flipper_rx_practice_host_feed(MorseFlipperApp* app, const char* text, size_t length, uint32_t now_ms) {
    MfRxPracticeResult result = {0};
    bool ok;
    if(app == NULL || app->plugin_slot.mutex == NULL) return false;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    ok = mf_rx_call_locked(app, &result, 1U, text, length, MfRxPracticeCommandNone, now_ms);
    furi_mutex_release(app->plugin_slot.mutex);
    morse_flipper_update_sidetone(app);
    return ok;
}

void morse_flipper_rx_practice_host_tick(MorseFlipperApp* app, uint32_t now_ms) {
    MfRxPracticeResult result = {0};
    if(app == NULL || app->plugin_slot.mutex == NULL) return;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    mf_rx_call_locked(app, &result, 2U, NULL, 0U, MfRxPracticeCommandNone, now_ms);
    furi_mutex_release(app->plugin_slot.mutex);
    morse_flipper_update_sidetone(app);
}

void morse_flipper_rx_practice_host_draw(MorseFlipperApp* app, Canvas* canvas) {
    if(app == NULL || canvas == NULL || app->plugin_slot.mutex == NULL) return;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.owner == MorseFlipperPluginOwnerRxPractice && app->plugin_slot.api != NULL &&
       app->plugin_slot.state != NULL)
        ((const MfRxPracticeApi*)app->plugin_slot.api)->draw(app->plugin_slot.state, canvas);
    else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignBottom, "RX Practice");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignBottom, "Unavailable");
        canvas_draw_str_aligned(canvas, 64, 49, AlignCenter, AlignBottom, "Plugin missing/corrupt");
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "Back");
    }
    furi_mutex_release(app->plugin_slot.mutex);
}
