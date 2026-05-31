
#include "ui.h"

#include <notification/notification_messages.h>

#define LOG_TAG "infrared_playback_ui"

/// collection of all scene on_enter handlers - in the same order as their enum
void (*const scene_on_enter_handlers[])(void*) = {
    remote_select_scene_on_enter,
    remote_playback_scene_on_enter,
};

/// collection of all scene on event handlers - in the same order as their enum
bool (*const scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    remote_select_scene_on_event,
    remote_playback_scene_on_event,
};

/// collection of all scene on exit handlers - in the same order as their enum
void (*const scene_on_exit_handlers[])(void*) = {
    remote_select_scene_on_exit,
    remote_playback_scene_on_exit,
};

static const SceneManagerHandlers scene_event_handlers = {
    .on_enter_handlers = scene_on_enter_handlers,
    .on_event_handlers = scene_on_event_handlers,
    .on_exit_handlers = scene_on_exit_handlers,
    .scene_num = ui_count
};

static bool scene_manager_navigation_event_callback(void* context) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_check(context);
    
    UI* ui = context;
    return scene_manager_handle_back_event(ui->scene_manager);
}

static bool scene_manager_custom_event_callback(void* context, uint32_t event) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_check(context);
    
    UI* ui = context;
    return scene_manager_handle_custom_event(ui->scene_manager, event);
}

UI* ui_alloc() {
    FURI_LOG_T(LOG_TAG, __func__);
    
    UI* ui = malloc(sizeof(UI));
    ui->scene_manager = scene_manager_alloc(&scene_event_handlers, ui);
    ui->view_dispatcher = view_dispatcher_alloc();
    
    ui->storage = furi_record_open(RECORD_STORAGE);
    ui->notifications = furi_record_open(RECORD_NOTIFICATION);
    
    ui->remote_select_scene = remote_select_scene_alloc(ui);
    ui->remote_playback_scene = remote_playback_scene_alloc(ui);
    
    FURI_LOG_D(LOG_TAG, "Setting Up View Dispatcher");
    view_dispatcher_set_event_callback_context(ui->view_dispatcher, ui);
    view_dispatcher_set_navigation_event_callback(ui->view_dispatcher, scene_manager_navigation_event_callback);
    view_dispatcher_set_custom_event_callback(ui->view_dispatcher, scene_manager_custom_event_callback);
    
    FURI_LOG_D(LOG_TAG, "Adding Scenes");
    view_dispatcher_add_view(ui->view_dispatcher, View_RemoteSelectDisplay, file_browser_get_view(ui->remote_select_scene->file_browser));
    view_dispatcher_add_view(ui->view_dispatcher, View_RemotePlaybackDisplay, ui->remote_playback_scene->view);
    
    return ui;
}

void ui_start(UI* ui) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_check(ui);
    
    notification_message(ui->notifications, &sequence_display_backlight_on);
    
    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(ui->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(ui->scene_manager, RemoteSelectDisplay);
    
    FURI_LOG_D(LOG_TAG, "Starting dispatcher");
    view_dispatcher_run(ui->view_dispatcher);
}

void ui_free(UI* ui) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_check(ui);
    
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    
    view_dispatcher_remove_view(ui->view_dispatcher, View_RemoteSelectDisplay);
    view_dispatcher_remove_view(ui->view_dispatcher, View_RemotePlaybackDisplay);
    view_dispatcher_free(ui->view_dispatcher);
    
    scene_manager_free(ui->scene_manager);
    
    remote_select_scene_free(ui->remote_select_scene);
    remote_playback_scene_free(ui->remote_playback_scene);
    
    free(ui);
}
