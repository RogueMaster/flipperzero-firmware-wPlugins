
#include "ui_i.h"

#include <furi_hal.h>
#include <datetime/datetime.h>

#define LOG_TAG "nfc_sniffer_ui"

#define LOG_FILE_FOLDER "/ext/nfc_sniffer_logs"

char* PROTOCOL_NAMES[NfcTechNum] = {"ISO14443-3A", "ISO14443-3B", "ISO15693-3", "FELICA"};

/// collection of all scene on_enter handlers - in the same order as their enum
void (*const scene_on_enter_handlers[])(void*) = {
    protocol_select_scene_on_enter,
    log_scene_on_enter,
};

/// collection of all scene on event handlers - in the same order as their enum
bool (*const scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    protocol_select_scene_on_event,
    log_scene_on_event,
};

/// collection of all scene on exit handlers - in the same order as their enum
void (*const scene_on_exit_handlers[])(void*) = {
    protocol_select_scene_on_exit,
    log_scene_on_exit,
};

static const SceneManagerHandlers scene_event_handlers = {
    .on_enter_handlers = scene_on_enter_handlers,
    .on_event_handlers = scene_on_event_handlers,
    .on_exit_handlers = scene_on_exit_handlers,
    .scene_num = ui_count};

static bool scene_manager_navigation_event_callback(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UI* ui = context;
    return scene_manager_handle_back_event(ui->scene_manager);
}

static bool scene_manager_custom_event_callback(void* context, uint32_t event) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(context);

    UI* ui = context;
    return scene_manager_handle_custom_event(ui->scene_manager, event);
}

UI* ui_alloc() {
    FURI_LOG_T(LOG_TAG, __func__);

    UI* ui = malloc(sizeof(UI));
    ui->protocol_select_scene = protocol_select_scene_alloc();
    ui->log_scene = log_scene_alloc();
    ui->scene_manager = scene_manager_alloc(&scene_event_handlers, ui);
    ui->view_dispatcher = view_dispatcher_alloc();
    ui->notifications = furi_record_open(RECORD_NOTIFICATION);
    ui->storage = furi_record_open(RECORD_STORAGE);
    ui->gui = furi_record_open(RECORD_GUI);
    ui->nfc = nfc_alloc();

    FURI_LOG_D(LOG_TAG, "Setting Up View Dispatcher");
    view_dispatcher_set_event_callback_context(ui->view_dispatcher, ui);
    view_dispatcher_set_navigation_event_callback(
        ui->view_dispatcher, scene_manager_navigation_event_callback);
    view_dispatcher_set_custom_event_callback(
        ui->view_dispatcher, scene_manager_custom_event_callback);

    FURI_LOG_D(LOG_TAG, "Adding Scenes");
    view_dispatcher_add_view(
        ui->view_dispatcher,
        View_ProtocolSelectDisplay,
        submenu_get_view(ui->protocol_select_scene->protocol_menu));
    view_dispatcher_add_view(
        ui->view_dispatcher, View_LogDisplay, text_box_get_view(ui->log_scene->log_display));

    FURI_LOG_D(LOG_TAG, "Creating log file");
    if(!storage_dir_exists(ui->storage, LOG_FILE_FOLDER)) {
        FURI_LOG_D(LOG_TAG, "Creating folder %s", LOG_FILE_FOLDER);
        if(!storage_simply_mkdir(ui->storage, LOG_FILE_FOLDER)) {
            FURI_LOG_E(LOG_TAG, "Failed to create %s", LOG_FILE_FOLDER);
        }
    }

    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);
    char path[64];
    snprintf(
        path,
        64,
        "%s/log_%04d%02d%02d_%02d%02d%02d.txt",
        LOG_FILE_FOLDER,
        datetime.year,
        datetime.month,
        datetime.day,
        datetime.hour,
        datetime.minute,
        datetime.second);

    FURI_LOG_D(LOG_TAG, "Opening file %s", path);
    ui->log_file = storage_file_alloc(ui->storage);
    bool open = storage_file_open(ui->log_file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);

    if(!open) {
        FURI_LOG_E(
            LOG_TAG,
            "Failed to open %s. ERROR: %s",
            path,
            storage_file_get_error_desc(ui->log_file));
    }

    return ui;
}

void ui_start(UI* ui) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(ui);

    view_dispatcher_attach_to_gui(ui->view_dispatcher, ui->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(ui->scene_manager, ProtocolSelectDisplay);

    FURI_LOG_D(LOG_TAG, "Starting dispatcher");
    view_dispatcher_run(ui->view_dispatcher);
}

void ui_free(UI* ui) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(ui);

    storage_file_close(ui->log_file);
    storage_file_free(ui->log_file);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);

    view_dispatcher_remove_view(ui->view_dispatcher, View_ProtocolSelectDisplay);
    view_dispatcher_remove_view(ui->view_dispatcher, View_LogDisplay);
    view_dispatcher_free(ui->view_dispatcher);

    nfc_free(ui->nfc);
    scene_manager_free(ui->scene_manager);
    log_scene_free(ui->log_scene);
    protocol_select_scene_free(ui->protocol_select_scene);

    free(ui);
}
