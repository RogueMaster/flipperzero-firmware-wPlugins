#include "rf_studio_i.h"

static bool rfs_custom_event_callback(void* context, uint32_t event) {
    RfStudioApp* app = context;
    if(event == RfsEventTxDone) {
        /* Stop the transmit LED whichever screen is showing. */
        notification_message(app->notifications, &sequence_blink_stop);
    }
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool rfs_back_event_callback(void* context) {
    RfStudioApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void rfs_tick_event_callback(void* context) {
    RfStudioApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

static void rfs_tx_done_callback(void* context) {
    RfStudioApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, RfsEventTxDone);
}

static void rfs_capture_callback(RfsCaptureEvent event, void* context) {
    RfStudioApp* app = context;
    uint32_t custom;
    switch(event) {
    case RfsCaptureEventFrequencyFound:
        custom = RfsEventCaptureFound;
        break;
    case RfsCaptureEventDecoded:
        custom = RfsEventCaptureDecoded;
        break;
    case RfsCaptureEventRawReady:
        custom = RfsEventCaptureRaw;
        break;
    default:
        custom = RfsEventCaptureError;
        break;
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, custom);
}

static void rfs_remote_view_callback(RfsRemoteViewEvent event, void* context) {
    RfStudioApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        event == RfsRemoteViewEventSend ? RfsEventRemoteSend : RfsEventRemoteMenu);
}

static void rfs_capture_view_callback(RfsCaptureViewEvent event, void* context) {
    RfStudioApp* app = context;
    uint32_t custom = RfsEventCaptureOptions;
    if(event == RfsCaptureViewEventStop) custom = RfsEventCaptureStop;
    if(event == RfsCaptureViewEventRestart) custom = RfsEventCaptureRestart;
    view_dispatcher_send_custom_event(app->view_dispatcher, custom);
}

void rfs_app_submenu_callback(void* context, uint32_t index) {
    RfStudioApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, RfsEventMenuBase + index);
}

void rfs_format_mhz(uint32_t frequency, char* out, size_t size) {
    snprintf(
        out,
        size,
        "%lu.%02lu",
        (unsigned long)(frequency / 1000000),
        (unsigned long)((frequency % 1000000) / 10000));
}

void rfs_app_message(RfStudioApp* app, const char* title, const char* text, uint32_t return_scene) {
    furi_string_set(app->message_title, title);
    furi_string_set(app->message_text, text);
    app->message_return = return_scene;
    scene_manager_next_scene(app->scene_manager, RfsSceneMessage);
}

bool rfs_app_save_remote(RfStudioApp* app) {
    if(rfs_remote_save(app->storage, &app->remote)) return true;
    rfs_diag_error(app->diag, "Save failed: remote %s", app->remote.id);
    return false;
}

bool rfs_app_save_signals(RfStudioApp* app) {
    if(rfs_signal_index_save(app->storage, &app->signals)) return true;
    rfs_diag_error(app->diag, "Save failed: signal index");
    return false;
}

RfsRadioMode rfs_app_remote_radio_mode(RfStudioApp* app) {
    if(app->remote.preferred_radio != RfsRadioModeAuto) return app->remote.preferred_radio;
    return app->settings.radio_mode;
}

bool rfs_app_send_signal(
    RfStudioApp* app,
    const char* signal_id,
    RfsRadioMode mode,
    FuriString* out) {
    if(!signal_id || !signal_id[0]) {
        furi_string_set(out, "No signal assigned");
        return false;
    }
    RfsSignalMeta* meta = rfs_signal_find(&app->signals, signal_id);
    if(!meta) {
        furi_string_set(out, "Signal missing");
        return false;
    }
    RfsReplay replay = rfs_signal_replay(meta);
    if(!rfs_replay_allowed(replay)) {
        furi_string_set(out, replay == RfsReplayRolling ? "Rolling code" : "Not supported");
        return false;
    }
    if(rfs_tx_is_running(app->tx)) {
        furi_string_set(out, "Busy");
        return false;
    }
    FuriString* path = furi_string_alloc();
    rfs_signal_path(meta->id, path);
    bool started =
        rfs_tx_start(app->tx, furi_string_get_cstr(path), mode, rfs_tx_done_callback, app);
    furi_string_free(path);
    if(!started) {
        furi_string_set(out, "Busy");
        return false;
    }
    notification_message(app->notifications, &sequence_blink_start_magenta);
    furi_string_set(out, "Sending...");
    return true;
}

void rfs_app_exit_capture(RfStudioApp* app) {
    rfs_capture_stop(app->capture);
    rfs_capture_discard(app->capture);
    const uint32_t targets[] = {
        RfsSceneButtonMenu,
        RfsSceneRemoteEditor,
        RfsSceneRemoteRun,
        RfsSceneSignalList,
        RfsSceneStart,
    };
    scene_manager_search_and_switch_to_previous_scene_one_of(
        app->scene_manager, targets, COUNT_OF(targets));
}

static void rfs_app_capture_defaults(RfStudioApp* app) {
    app->capture_frequency = 0;
    strlcpy(app->capture_preset, app->settings.default_preset, sizeof(app->capture_preset));
}

void rfs_app_start_button_capture(RfStudioApp* app, RfsCaptureTarget target) {
    if(memmgr_get_free_heap() < RFS_MIN_FREE_HEAP * 2) {
        rfs_app_message(
            app, "Low Memory", "Not enough memory to\nadd another button.", RfsSceneNum);
        return;
    }
    app->capture_target = target;
    app->capture_radio = rfs_app_remote_radio_mode(app);
    rfs_app_capture_defaults(app);

    /* Suggest the frequency already used by this remote (Addendum Section 4.4). */
    if(app->remote.preferred_frequency) {
        app->capture_frequency = app->remote.preferred_frequency;
        if(app->remote.preferred_preset[0]) {
            strlcpy(
                app->capture_preset, app->remote.preferred_preset, sizeof(app->capture_preset));
        }
    }
    if(target == RfsCaptureForRerecord && app->button_index >= 0 &&
       (size_t)app->button_index < app->remote.count) {
        RfsSignalMeta* meta =
            rfs_signal_find(&app->signals, app->remote.buttons[app->button_index].signal_id);
        if(meta) {
            app->capture_frequency = meta->frequency;
            if(strcmp(meta->preset, "Custom") != 0) {
                strlcpy(app->capture_preset, meta->preset, sizeof(app->capture_preset));
            }
        }
    }
    scene_manager_next_scene(app->scene_manager, RfsSceneCapture);
}

void rfs_app_start_library_capture(RfStudioApp* app) {
    app->capture_target = RfsCaptureForLibrary;
    app->capture_radio = app->settings.radio_mode;
    rfs_app_capture_defaults(app);
    scene_manager_next_scene(app->scene_manager, RfsSceneCapture);
}

bool rfs_app_store_capture(RfStudioApp* app, const char* name, char* out_id) {
    const RfsCaptureResult* result = rfs_capture_result(app->capture);
    if(!result->valid) return false;

    RfsSignalMeta* meta = rfs_signal_add(&app->signals);
    if(!meta) return false;
    rfs_storage_new_id(app->storage, 'S', RFS_SIGNALS_DIR, RFS_SIGNAL_EXT, meta->id);
    strlcpy(meta->name, name, sizeof(meta->name));
    strlcpy(meta->protocol, result->protocol, sizeof(meta->protocol));
    strlcpy(meta->preset, result->preset, sizeof(meta->preset));
    meta->frequency = result->frequency;
    meta->kind = result->kind;
    meta->source = RfsSourceCapture;
    meta->verified = false;
    meta->created = rfs_storage_now();

    char id[RFS_ID_LEN];
    strlcpy(id, meta->id, sizeof(id));
    FuriString* path = furi_string_alloc();
    rfs_signal_path(id, path);
    bool ok = rfs_capture_save(app->capture, app->storage, furi_string_get_cstr(path));
    if(ok) ok = rfs_app_save_signals(app);
    if(!ok) {
        storage_common_remove(app->storage, furi_string_get_cstr(path));
        rfs_signal_remove(&app->signals, id);
        rfs_diag_error(app->diag, "Could not store captured signal");
    }
    furi_string_free(path);
    if(ok) {
        strlcpy(out_id, id, RFS_ID_LEN);
        rfs_capture_discard(app->capture);
    }
    return ok;
}

void rfs_app_remember_frequency(RfStudioApp* app, uint32_t frequency, const char* preset) {
    if(app->remote.preferred_frequency) return;
    app->remote.preferred_frequency = frequency;
    if(preset && strcmp(preset, "Custom") != 0) {
        strlcpy(app->remote.preferred_preset, preset, sizeof(app->remote.preferred_preset));
    }
}

void rfs_app_finish_button_capture(RfStudioApp* app, const char* button_name) {
    const RfsCaptureResult* result = rfs_capture_result(app->capture);
    uint32_t frequency = result->frequency;
    char preset[RFS_PRESET_LEN];
    strlcpy(preset, result->preset, sizeof(preset));

    /* Signals are named after their remote and button so the library stays readable. */
    FuriString* signal_name = furi_string_alloc_printf("%s %s", app->remote.name, button_name);
    if(furi_string_size(signal_name) > RFS_NAME_LEN) furi_string_left(signal_name, RFS_NAME_LEN);
    char signal_id[RFS_ID_LEN];
    bool stored = rfs_app_store_capture(app, furi_string_get_cstr(signal_name), signal_id);
    furi_string_free(signal_name);
    if(!stored) {
        rfs_app_message(
            app, "Not Saved", "Could not save the signal.\nCheck the SD card.", RfsSceneNum);
        return;
    }

    bool ok = true;
    if(app->capture_target == RfsCaptureForRerecord && app->button_index >= 0 &&
       (size_t)app->button_index < app->remote.count) {
        /* The previous signal stays in the library; other buttons may still use it. */
        RfsButton* button = &app->remote.buttons[app->button_index];
        strlcpy(button->signal_id, signal_id, sizeof(button->signal_id));
        button->updated = rfs_storage_now();
    } else {
        RfsButton* button =
            rfs_remote_add_button(app->storage, &app->remote, button_name, signal_id);
        if(button) {
            app->button_index = app->remote.count - 1;
        } else {
            ok = false;
        }
    }
    rfs_app_remember_frequency(app, frequency, preset);

    if(!ok) {
        rfs_app_message(
            app,
            "Low Memory",
            "Signal saved to the library,\nbut the button wasn't added.",
            RfsSceneNum);
        return;
    }
    if(!rfs_app_save_remote(app)) {
        rfs_app_message(
            app, "Not Saved", "Could not save the remote.\nCheck the SD card.", RfsSceneNum);
        return;
    }
    notification_message(app->notifications, &sequence_success);
    rfs_app_exit_capture(app);
}

static RfStudioApp* rfs_app_alloc(void) {
    RfStudioApp* app = malloc(sizeof(RfStudioApp));
    memset(app, 0, sizeof(RfStudioApp));

    /* 1. Configuration */
    app->storage = furi_record_open(RECORD_STORAGE);
    app->gui = furi_record_open(RECORD_GUI);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->sd_ready = rfs_storage_init(app->storage);
    rfs_settings_defaults(&app->settings);
    if(app->sd_ready) rfs_settings_load(app->storage, &app->settings);

    app->diag = rfs_diag_alloc(app->storage);
    rfs_diag_set_logging(app->diag, app->settings.logging && app->sd_ready);

    /* 2-7. Firmware, radios, capabilities and protocols (Section 5.4) */
    app->radio = rfs_radio_alloc(app->diag);
    rfs_radio_refresh(app->radio, app->settings.radio_mode != RfsRadioModeInternal);
    app->tx = rfs_tx_alloc(app->radio, app->diag);
    app->capture = rfs_capture_alloc(app->radio, app->diag);
    rfs_capture_set_callback(app->capture, rfs_capture_callback, app);

    /* 8. Library */
    rfs_signal_index_init(&app->signals);
    rfs_remote_init(&app->remote);
    rfs_remote_list_init(&app->remote_list);
    if(app->sd_ready && !rfs_signal_index_load(app->storage, &app->signals)) {
        rfs_diag_error(app->diag, "Signal index unreadable");
    }

    app->message_title = furi_string_alloc();
    app->message_text = furi_string_alloc();
    app->text_store = furi_string_alloc();
    app->capture_radio = app->settings.radio_mode;
    strlcpy(app->capture_preset, app->settings.default_preset, sizeof(app->capture_preset));

    /* UI */
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&rfs_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, rfs_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, rfs_back_event_callback);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, rfs_tick_event_callback, 100);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, RfsViewSubmenu, submenu_get_view(app->submenu));
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, RfsViewTextInput, text_input_get_view(app->text_input));
    app->var_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, RfsViewVarList, variable_item_list_get_view(app->var_list));
    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, RfsViewWidget, widget_get_view(app->widget));
    app->dialog = dialog_ex_alloc();
    view_dispatcher_add_view(app->view_dispatcher, RfsViewDialog, dialog_ex_get_view(app->dialog));
    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, RfsViewPopup, popup_get_view(app->popup));
    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, RfsViewTextBox, text_box_get_view(app->text_box));

    app->remote_view = rfs_remote_view_alloc();
    rfs_remote_view_set_callback(app->remote_view, rfs_remote_view_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, RfsViewRemote, rfs_remote_view_get_view(app->remote_view));
    app->capture_view = rfs_capture_view_alloc();
    rfs_capture_view_set_callback(app->capture_view, rfs_capture_view_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, RfsViewCapture, rfs_capture_view_get_view(app->capture_view));

    return app;
}

static void rfs_app_free(RfStudioApp* app) {
    /* Stop radio work before tearing anything down. */
    rfs_capture_stop(app->capture);
    rfs_tx_free(app->tx);
    rfs_capture_free(app->capture);
    notification_message(app->notifications, &sequence_blink_stop);

    view_dispatcher_remove_view(app->view_dispatcher, RfsViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, RfsViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, RfsViewVarList);
    view_dispatcher_remove_view(app->view_dispatcher, RfsViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, RfsViewDialog);
    view_dispatcher_remove_view(app->view_dispatcher, RfsViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, RfsViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, RfsViewRemote);
    view_dispatcher_remove_view(app->view_dispatcher, RfsViewCapture);
    submenu_free(app->submenu);
    text_input_free(app->text_input);
    variable_item_list_free(app->var_list);
    widget_free(app->widget);
    dialog_ex_free(app->dialog);
    popup_free(app->popup);
    text_box_free(app->text_box);
    rfs_remote_view_free(app->remote_view);
    rfs_capture_view_free(app->capture_view);
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    rfs_radio_free(app->radio);
    rfs_signal_index_clear(&app->signals);
    rfs_remote_clear(&app->remote);
    rfs_remote_list_clear(&app->remote_list);
    rfs_diag_free(app->diag);
    furi_string_free(app->message_title);
    furi_string_free(app->message_text);
    furi_string_free(app->text_store);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    free(app);
}

int32_t rf_studio_app(void* p) {
    UNUSED(p);
    RfStudioApp* app = rfs_app_alloc();

    /* 9. Home screen */
    scene_manager_next_scene(app->scene_manager, RfsSceneStart);
    if(!app->sd_ready) {
        rfs_app_message(
            app,
            "No SD card",
            "Insert a microSD card to save\nremotes and signals.",
            RfsSceneStart);
    }
    view_dispatcher_run(app->view_dispatcher);

    rfs_app_free(app);
    return 0;
}
