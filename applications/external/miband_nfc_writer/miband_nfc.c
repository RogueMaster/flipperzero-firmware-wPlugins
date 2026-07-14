/**
 * @file miband_nfc.c
 * @brief Main application with FIXED memory management
 */

#include "miband_nfc_i.h"

#define TAG "MiBandNfc"

static void miband_nfc_app_free(MiBandNfcApp* app);

bool miband_nfc_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    MiBandNfcApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

bool miband_nfc_app_back_event_callback(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static MiBandNfcApp* miband_nfc_app_alloc() {
    MiBandNfcApp* app = malloc(sizeof(MiBandNfcApp));
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate app structure");
        return NULL;
    }

    memset(app, 0, sizeof(MiBandNfcApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    if(!app->storage) {
        FURI_LOG_E(TAG, "Failed to open storage record");
        goto error;
    }

    FURI_LOG_I(TAG, "Records opened, storage=%p", app->storage);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&miband_nfc_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, miband_nfc_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, miband_nfc_app_back_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MiBandNfcViewIdMainMenu, submenu_get_view(app->submenu));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MiBandNfcViewIdScanner, popup_get_view(app->popup));
    view_dispatcher_add_view(
        app->view_dispatcher, MiBandNfcViewIdMagicEmulator, popup_get_view(app->popup));
    view_dispatcher_add_view(
        app->view_dispatcher, MiBandNfcViewIdWriter, popup_get_view(app->popup));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MiBandNfcViewIdAbout, text_box_get_view(app->text_box));

    app->dialog_ex = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MiBandNfcViewIdDialog, dialog_ex_get_view(app->dialog_ex));

    app->text_box_report = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MiBandNfcViewIdUidReport, text_box_get_view(app->text_box_report));

    app->nfc = nfc_alloc();
    app->nfc_device = nfc_device_alloc();
    app->target_data = mf_classic_alloc();
    app->mf_classic_data = mf_classic_alloc();

    app->poller = NULL;
    app->scanner = NULL;
    app->listener = NULL;

    app->file_path = furi_string_alloc();
    app->temp_text_buffer = furi_string_alloc();

    app->is_valid_nfc_data = false;
    app->is_scan_active = false;
    app->last_selected_submenu_index = SubmenuIndexQuickUidCheck;
    app->current_operation = OperationTypeEmulateMagic;

    app->logger = miband_logger_alloc(app->storage);
    if(!app->logger) {
        FURI_LOG_W(TAG, "Failed to allocate logger, cleaning up");
        // FIX: Use miband_nfc_app_free for complete cleanup instead of
        // partial manual cleanup that missed GUI views (submenu, popup, etc.)
        miband_nfc_app_free(app);
        return NULL;
    }

    if(!miband_settings_load(app)) {
        app->auto_backup_enabled = true;
        app->verify_after_write = false;
        app->show_detailed_progress = true;
        app->enable_logging = true;
        FURI_LOG_I(TAG, "Using default settings");
    }

    miband_logger_set_enabled(app->logger, app->enable_logging);
    miband_logger_log(app->logger, LogLevelInfo, "Application started");

    return app;

error:
    // FIX: Use miband_nfc_app_free for complete cleanup
    miband_nfc_app_free(app);
    return NULL;
}

static void miband_nfc_app_free(MiBandNfcApp* app) {
    furi_assert(app);

    if(app->uid_check_context) {
        FURI_LOG_W(TAG, "UidCheckContext still allocated at app free, cleaning up");
        free(app->uid_check_context);
        app->uid_check_context = NULL;
    }

    if(app->submenu) {
        view_dispatcher_remove_view(app->view_dispatcher, MiBandNfcViewIdMainMenu);
        submenu_free(app->submenu);
    }

    if(app->popup) {
        view_dispatcher_remove_view(app->view_dispatcher, MiBandNfcViewIdScanner);
        view_dispatcher_remove_view(app->view_dispatcher, MiBandNfcViewIdMagicEmulator);
        view_dispatcher_remove_view(app->view_dispatcher, MiBandNfcViewIdWriter);
        popup_free(app->popup);
    }

    if(app->text_box) {
        view_dispatcher_remove_view(app->view_dispatcher, MiBandNfcViewIdAbout);
        text_box_free(app->text_box);
    }

    if(app->text_box_report) {
        view_dispatcher_remove_view(app->view_dispatcher, MiBandNfcViewIdUidReport);
        text_box_free(app->text_box_report);
    }

    if(app->dialog_ex) {
        view_dispatcher_remove_view(app->view_dispatcher, MiBandNfcViewIdDialog);
        dialog_ex_free(app->dialog_ex);
    }

    if(app->scene_manager) {
        scene_manager_free(app->scene_manager);
    }

    if(app->view_dispatcher) {
        view_dispatcher_free(app->view_dispatcher);
    }

    if(app->listener) {
        nfc_listener_stop(app->listener);
        nfc_listener_free(app->listener);
    }

    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }

    if(app->scanner) {
        nfc_scanner_stop(app->scanner);
        nfc_scanner_free(app->scanner);
    }

    if(app->nfc) {
        nfc_free(app->nfc);
    }

    if(app->nfc_device) {
        nfc_device_free(app->nfc_device);
    }

    if(app->mf_classic_data) {
        mf_classic_free(app->mf_classic_data);
    }

    if(app->target_data) {
        mf_classic_free(app->target_data);
    }

    if(app->file_path) {
        furi_string_free(app->file_path);
    }

    if(app->temp_text_buffer) {
        furi_string_free(app->temp_text_buffer);
    }

    if(app->logger) {
        miband_logger_log(app->logger, LogLevelInfo, "Application closing");
        miband_logger_free(app->logger);
    }

    if(app->dialogs) {
        furi_record_close(RECORD_DIALOGS);
    }

    if(app->storage) {
        furi_record_close(RECORD_STORAGE);
    }

    if(app->notifications) {
        furi_record_close(RECORD_NOTIFICATION);
    }

    if(app->gui) {
        furi_record_close(RECORD_GUI);
    }

    free(app);
}

int32_t miband_nfc_app(void* p) {
    UNUSED(p);

    MiBandNfcApp* app = miband_nfc_app_alloc();
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate application");
        return -1;
    }

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, MiBandNfcSceneMainMenu);
    view_dispatcher_run(app->view_dispatcher);

    miband_nfc_app_free(app);

    return 0;
}
