#include "wol_flipper.h"

#include <storage/storage.h>

/** So the file browser has somewhere to point at before the first backup. */
static void wol_app_make_dirs(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, WOL_APP_DIR);
    storage_common_mkdir(storage, WOL_FW_DIR);
    storage_common_mkdir(storage, WOL_BACKUP_DIR);
    furi_record_close(RECORD_STORAGE);
}

static bool wol_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    WolApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool wol_back_event_callback(void* context) {
    furi_assert(context);
    WolApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void wol_tick_event_callback(void* context) {
    furi_assert(context);
    WolApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

static WolApp* wol_app_alloc(void) {
    WolApp* app = malloc(sizeof(WolApp));
    memset(app, 0, sizeof(WolApp));

    wol_app_make_dirs();
    wol_config_load(&app->config);

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->flasher_path = furi_string_alloc();

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&wol_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, wol_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, wol_back_event_callback);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, wol_tick_event_callback, 500);

    app->submenu = submenu_alloc();
    app->text_input = text_input_alloc();
    app->byte_input = byte_input_alloc();
    app->popup = popup_alloc();
    app->widget = widget_alloc();

    view_dispatcher_add_view(app->view_dispatcher, WolViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(
        app->view_dispatcher, WolViewTextInput, text_input_get_view(app->text_input));
    view_dispatcher_add_view(
        app->view_dispatcher, WolViewByteInput, byte_input_get_view(app->byte_input));
    view_dispatcher_add_view(app->view_dispatcher, WolViewPopup, popup_get_view(app->popup));
    view_dispatcher_add_view(app->view_dispatcher, WolViewWidget, widget_get_view(app->widget));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void wol_app_free(WolApp* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, WolViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, WolViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, WolViewByteInput);
    view_dispatcher_remove_view(app->view_dispatcher, WolViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, WolViewWidget);

    submenu_free(app->submenu);
    text_input_free(app->text_input);
    byte_input_free(app->byte_input);
    popup_free(app->popup);
    widget_free(app->widget);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_string_free(app->flasher_path);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t wol_flipper_app(void* p) {
    UNUSED(p);

    WolApp* app = wol_app_alloc();
    scene_manager_next_scene(app->scene_manager, WolSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    wol_app_free(app);

    return 0;
}
