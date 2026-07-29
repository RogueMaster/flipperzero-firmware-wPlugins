#ifdef MF_RADIO_HOST_TEST
#include "morse_flipper_radio_host_test.h"
#else
#include "morse_flipper_app_i.h"
#endif

#define MORSE_FLIPPER_RADIO_PLUGIN_PATH APP_ASSETS_PATH("plugins/morse_flipper_radio.fal")

static bool morse_flipper_radio_host_snapshot_locked(
    const MorseFlipperApp* app,
    MfRadioSnapshot* snapshot) {
    const MfRadioApi* api;
    if(app == NULL || snapshot == NULL || app->plugin_slot.owner != MorseFlipperPluginOwnerRadio ||
       app->plugin_slot.error != MorseFlipperPluginErrorNone || app->plugin_slot.api == NULL ||
       app->plugin_slot.state == NULL)
        return false;
    api = app->plugin_slot.api;
    *snapshot = (MfRadioSnapshot){.struct_size = sizeof(MfRadioSnapshot)};
    return mf_radio_api_valid(api) && api->snapshot(app->plugin_slot.state, snapshot);
}

static void morse_flipper_radio_host_apply(
    MorseFlipperApp* app,
    const MfRadioSnapshot* snapshot,
    MorseFlipperMappedFalResult result,
    uint32_t now_ms) {
    bool save_frequency = false;
    if(app == NULL) return;
    if(snapshot != NULL && snapshot->struct_size == sizeof(MfRadioSnapshot)) {
        if(snapshot->frequency_dirty && app->rf_frequency_hz != snapshot->frequency_hz) {
            app->rf_frequency_hz = snapshot->frequency_hz;
            save_frequency = true;
        }
        app->rf_rx_audio_enabled = snapshot->receive_audio_enabled;
        app->radio_tx_allowed = snapshot->tx_allowed;
        app->radio_tx_active = snapshot->tx_active;
        app->radio_monitor_tone = snapshot->monitor_tone;
    }
    morse_flipper_update_sidetone(app);
    morse_flipper_sync_ptt(app, now_ms);
    if(save_frequency) morse_flipper_save_config(app);
    if(result.redraw) morse_flipper_view_dirty(app);
}

bool morse_flipper_radio_host_open(MorseFlipperApp* app, uint32_t now_ms) {
    MorseFlipperMappedFalResult initial = {0};
    MfRadioSnapshot snapshot = {0};
    MfRadioEnterArgs args;
    bool opened;
    bool have_snapshot = false;
    if(app == NULL || app->plugin_slot.mutex == NULL) return false;
    if(morse_flipper_radio_host_active(app)) return true;

    app->radio_draw_services = (MfRadioDrawServices){
        .struct_size = sizeof(MfRadioDrawServices),
        .context = app,
        .history_reset = morse_flipper_run_history_reset,
        .history_append = morse_flipper_run_history_append,
        .draw_tx_history = morse_flipper_draw_tx_history_supplied,
        .draw_rx_text = morse_flipper_draw_radio_rx_text,
    };
    args = (MfRadioEnterArgs){
        .struct_size = sizeof(MfRadioEnterArgs),
        .now_ms = now_ms,
        .frequency_hz = app->rf_frequency_hz,
        .dit_ms = morse_flipper_current_dit_ms(app),
        .monitor_threshold_dbm = app->rf_monitor_threshold_dbm,
        .receive_audio_enabled = app->rf_rx_audio_enabled,
        .decoder = morse_flipper_radio_decoder_services(),
        .draw = &app->radio_draw_services,
    };

    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    opened = morse_flipper_plugin_runtime_open_mapped_locked(
        app,
        MorseFlipperPluginOwnerRadio,
        0U,
        MORSE_FLIPPER_RADIO_PLUGIN_PATH,
        MF_RADIO_API_VERSION,
        MF_RADIO_API_MAGIC,
        sizeof(MfRadioApi),
        &args,
        &initial);
    if(opened) {
        have_snapshot = morse_flipper_radio_host_snapshot_locked(app, &snapshot);
    } else {
        morse_flipper_plugin_runtime_release_claim_locked(app, MorseFlipperPluginOwnerRadio);
    }
    furi_mutex_release(app->plugin_slot.mutex);

    app->radio_load_error = !opened;
    if(opened)
        morse_flipper_radio_host_apply(app, have_snapshot ? &snapshot : NULL, initial, now_ms);
    else
        morse_flipper_view_dirty(app);
    return opened;
}

bool morse_flipper_radio_host_active(const MorseFlipperApp* app) {
    MorseFlipperPluginSnapshot snapshot;
    return morse_flipper_plugin_runtime_snapshot(app, &snapshot) &&
           snapshot.owner == MorseFlipperPluginOwnerRadio && snapshot.active;
}

bool morse_flipper_radio_host_set_page(MorseFlipperApp* app, MfRadioPage page, uint32_t now_ms) {
    const MfRadioApi* api;
    MorseFlipperMappedFalResult result = {0};
    MfRadioSnapshot snapshot = {0};
    bool have_snapshot = false;
    bool called = false;
    if(app == NULL || app->plugin_slot.mutex == NULL) return false;

    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.owner == MorseFlipperPluginOwnerRadio &&
       app->plugin_slot.error == MorseFlipperPluginErrorNone && app->plugin_slot.api != NULL &&
       app->plugin_slot.state != NULL) {
        api = app->plugin_slot.api;
        if(mf_radio_api_valid(api)) {
            result = api->set_page(app->plugin_slot.state, page, now_ms);
            called = true;
            have_snapshot = morse_flipper_radio_host_snapshot_locked(app, &snapshot);
        }
    }
    furi_mutex_release(app->plugin_slot.mutex);
    if(called)
        morse_flipper_radio_host_apply(app, have_snapshot ? &snapshot : NULL, result, now_ms);
    return called;
}

bool morse_flipper_radio_host_input(MorseFlipperApp* app, const InputEvent* event, uint32_t now_ms) {
    const MfRadioApi* api;
    MorseFlipperMappedFalResult result = {0};
    MfRadioSnapshot snapshot = {0};
    bool have_snapshot = false;
    bool called = false;
    if(app == NULL || event == NULL || app->plugin_slot.mutex == NULL) return false;

    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.owner == MorseFlipperPluginOwnerRadio &&
       app->plugin_slot.error == MorseFlipperPluginErrorNone && app->plugin_slot.api != NULL &&
       app->plugin_slot.state != NULL) {
        api = app->plugin_slot.api;
        if(mf_radio_api_valid(api)) {
            result = api->mapped.input(app->plugin_slot.state, event, now_ms);
            called = true;
            have_snapshot = morse_flipper_radio_host_snapshot_locked(app, &snapshot);
        }
    }
    furi_mutex_release(app->plugin_slot.mutex);

    if(called)
        morse_flipper_radio_host_apply(app, have_snapshot ? &snapshot : NULL, result, now_ms);
    if(result.request_exit || (!called && event->key == InputKeyBack &&
                               (event->type == InputTypeShort || event->type == InputTypeLong))) {
        morse_flipper_release_all_notes(app);
        morse_flipper_radio_host_set_page(app, MfRadioPageIdle, now_ms);
        morse_flipper_scene_back(app);
        return true;
    }
    if(called && app->screen == MorseFlipperScreenRf && app->radio_tx_allowed && !result.handled) {
        morse_flipper_handle_active_keying_event(app, event);
        return true;
    }
    return called ? result.handled : false;
}

void morse_flipper_radio_host_tick(MorseFlipperApp* app, uint32_t now_ms) {
    const MfRadioApi* api;
    MorseFlipperMappedFalResult result = {0};
    MfRadioSnapshot snapshot = {0};
    bool have_snapshot = false;
    bool called = false;
    if(app == NULL || app->plugin_slot.mutex == NULL) return;

    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.owner == MorseFlipperPluginOwnerRadio &&
       app->plugin_slot.error == MorseFlipperPluginErrorNone && app->plugin_slot.api != NULL &&
       app->plugin_slot.state != NULL) {
        api = app->plugin_slot.api;
        if(mf_radio_api_valid(api)) {
            result = api->mapped.tick(app->plugin_slot.state, now_ms);
            called = true;
            have_snapshot = morse_flipper_radio_host_snapshot_locked(app, &snapshot);
        }
    }
    furi_mutex_release(app->plugin_slot.mutex);
    if(called)
        morse_flipper_radio_host_apply(app, have_snapshot ? &snapshot : NULL, result, now_ms);
}

void morse_flipper_radio_host_sync_tx(
    MorseFlipperApp* app,
    MfRadioTxInterval completed_interval,
    uint16_t duration_ms,
    bool level,
    uint32_t now_ms) {
    const MfRadioApi* api;
    MorseFlipperMappedFalResult result = {0};
    MfRadioSnapshot snapshot = {0};
    bool have_snapshot = false;
    bool called = false;
    if(app == NULL || app->plugin_slot.mutex == NULL) return;

    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.owner == MorseFlipperPluginOwnerRadio &&
       app->plugin_slot.error == MorseFlipperPluginErrorNone && app->plugin_slot.api != NULL &&
       app->plugin_slot.state != NULL) {
        api = app->plugin_slot.api;
        if(mf_radio_api_valid(api)) {
            result = api->sync_tx(
                app->plugin_slot.state, completed_interval, duration_ms, level, now_ms);
            called = true;
            have_snapshot = morse_flipper_radio_host_snapshot_locked(app, &snapshot);
        }
    }
    furi_mutex_release(app->plugin_slot.mutex);
    if(called)
        morse_flipper_radio_host_apply(app, have_snapshot ? &snapshot : NULL, result, now_ms);
}

void morse_flipper_radio_host_draw(MorseFlipperApp* app, Canvas* canvas, uint32_t now_ms) {
    const MfRadioApi* api;
    bool drawn = false;
    if(app == NULL || canvas == NULL || app->plugin_slot.mutex == NULL) return;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.owner == MorseFlipperPluginOwnerRadio &&
       app->plugin_slot.error == MorseFlipperPluginErrorNone && app->plugin_slot.api != NULL &&
       app->plugin_slot.state != NULL) {
        api = app->plugin_slot.api;
        if(mf_radio_api_valid(api)) {
            api->mapped.draw(app->plugin_slot.state, canvas, now_ms);
            drawn = true;
        }
    }
    furi_mutex_release(app->plugin_slot.mutex);
    if(!drawn) morse_flipper_draw_plugin_unavailable(canvas);
}

void morse_flipper_radio_host_close(MorseFlipperApp* app, uint32_t now_ms) {
    const MfRadioApi* api;
    MfRadioSnapshot snapshot = {0};
    MorseFlipperMappedFalResult result = {0};
    bool have_snapshot = false;
    if(app == NULL || app->plugin_slot.mutex == NULL) return;

    morse_flipper_release_all_notes(app);
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.owner == MorseFlipperPluginOwnerRadio) {
        api = app->plugin_slot.api;
        if(app->plugin_slot.error == MorseFlipperPluginErrorNone &&
           app->plugin_slot.state != NULL && mf_radio_api_valid(api)) {
            result = api->set_page(app->plugin_slot.state, MfRadioPageIdle, now_ms);
            have_snapshot = morse_flipper_radio_host_snapshot_locked(app, &snapshot);
        }
        morse_flipper_plugin_runtime_detach_locked(app, MorseFlipperPluginOwnerRadio);
    }
    furi_mutex_release(app->plugin_slot.mutex);

    app->radio_load_error = false;
    app->radio_tx_active = false;
    app->radio_monitor_tone = false;
    morse_flipper_radio_host_apply(app, have_snapshot ? &snapshot : NULL, result, now_ms);
}
