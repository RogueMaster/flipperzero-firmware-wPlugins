
#include "ui.h"

#include <furi_hal.h>

#define LOG_TAG "gs1_rfid_parser_ui"

/// collection of all scene on_enter handlers - in the same order as their enum
void (*const scene_on_enter_handlers[])(void*) = {
    open_scene_on_enter,
    scan_scene_on_enter,
    parse_scene_on_enter,
    raw_data_scene_on_enter,
};

/// collection of all scene on event handlers - in the same order as their enum
bool (*const scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    open_scene_on_event,
    scan_scene_on_event,
    parse_scene_on_event,
    raw_data_scene_on_event,
};

/// collection of all scene on exit handlers - in the same order as their enum
void (*const scene_on_exit_handlers[])(void*) = {
    open_scene_on_exit,
    scan_scene_on_exit,
    parse_scene_on_exit,
    raw_data_scene_on_exit,
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
    ui->uhf_device = uhf_u107_alloc();
    ui->epc_data = malloc(sizeof(UhfEpcTagData));
    ui->user_mem_data = malloc(sizeof(UhfUserMemoryData));

    ui->open_scene = open_scene_alloc();
    ui->scan_scene = scan_scene_alloc(ui);
    ui->parse_scene = parse_scene_alloc();
    ui->raw_data_scene = raw_data_scene_alloc();

    ui->scene_manager = scene_manager_alloc(&scene_event_handlers, ui);
    ui->view_dispatcher = view_dispatcher_alloc();
    ui->notifications = furi_record_open(RECORD_NOTIFICATION);
    ui->gui = furi_record_open(RECORD_GUI);

    FURI_LOG_D(LOG_TAG, "Setting Up View Dispatcher");
    view_dispatcher_set_event_callback_context(ui->view_dispatcher, ui);
    view_dispatcher_set_navigation_event_callback(
        ui->view_dispatcher, scene_manager_navigation_event_callback);
    view_dispatcher_set_custom_event_callback(
        ui->view_dispatcher, scene_manager_custom_event_callback);

    FURI_LOG_D(LOG_TAG, "Adding Scenes");
    view_dispatcher_add_view(
        ui->view_dispatcher, View_OpenDisplay, widget_get_view(ui->open_scene->open_widget));
    view_dispatcher_add_view(
        ui->view_dispatcher, View_ScanDisplay, widget_get_view(ui->scan_scene->scan_widget));
    view_dispatcher_add_view(
        ui->view_dispatcher, View_ParseDisplay, widget_get_view(ui->parse_scene->parse_widget));
    view_dispatcher_add_view(
        ui->view_dispatcher,
        View_RawDataDisplay,
        widget_get_view(ui->raw_data_scene->raw_data_widget));

    return ui;
}

void ui_start(UI* ui) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(ui);

    view_dispatcher_attach_to_gui(ui->view_dispatcher, ui->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(ui->scene_manager, OpenDisplay);

    FURI_LOG_D(LOG_TAG, "Starting dispatcher");
    view_dispatcher_run(ui->view_dispatcher);
}

void ui_free(UI* ui) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(ui);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    view_dispatcher_remove_view(ui->view_dispatcher, View_OpenDisplay);
    view_dispatcher_remove_view(ui->view_dispatcher, View_ScanDisplay);
    view_dispatcher_remove_view(ui->view_dispatcher, View_ParseDisplay);
    view_dispatcher_remove_view(ui->view_dispatcher, View_RawDataDisplay);
    view_dispatcher_free(ui->view_dispatcher);

    scene_manager_free(ui->scene_manager);
    parse_scene_free(ui->parse_scene);
    scan_scene_free(ui->scan_scene);
    open_scene_free(ui->open_scene);
    raw_data_scene_free(ui->raw_data_scene);

    free(ui->epc_data);
    free(ui->user_mem_data);
    uhf_u107_free(ui->uhf_device);
    free(ui);
}
