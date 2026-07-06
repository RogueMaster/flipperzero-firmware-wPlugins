#include "src/core/cfs_app_i.h"

static bool cfs_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    CfsApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool cfs_back_event_callback(void* context) {
    furi_assert(context);
    CfsApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

// Runs on the NFC worker thread; hand the result to the GUI thread.
static void cfs_worker_callback(CfsNfcWorkerEvent event, void* context) {
    CfsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, cfs_worker_event_to_custom(event));
}

static CfsApp* cfs_app_alloc(void) {
    CfsApp* app = malloc(sizeof(CfsApp));
    memset(app, 0, sizeof(CfsApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->selected_path = furi_string_alloc();

    app->scene_manager = scene_manager_alloc(&cfs_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, cfs_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, cfs_back_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, CfsViewSubmenu, submenu_get_view(app->submenu));
    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, CfsViewWidget, widget_get_view(app->widget));
    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, CfsViewPopup, popup_get_view(app->popup));
    app->number_input = number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, CfsViewNumberInput, number_input_get_view(app->number_input));
    app->byte_input = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, CfsViewByteInput, byte_input_get_view(app->byte_input));
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, CfsViewTextInput, text_input_get_view(app->text_input));
    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        CfsViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));

    app->worker = cfs_nfc_worker_alloc();
    cfs_nfc_worker_set_callback(app->worker, cfs_worker_callback, app);

    cfs_storage_ensure_dir(app->storage);
    cfs_settings_load(app->storage, &app->settings);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void cfs_app_free(CfsApp* app) {
    furi_assert(app);

    cfs_nfc_worker_free(app->worker);

    view_dispatcher_remove_view(app->view_dispatcher, CfsViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_remove_view(app->view_dispatcher, CfsViewWidget);
    widget_free(app->widget);
    view_dispatcher_remove_view(app->view_dispatcher, CfsViewPopup);
    popup_free(app->popup);
    view_dispatcher_remove_view(app->view_dispatcher, CfsViewNumberInput);
    number_input_free(app->number_input);
    view_dispatcher_remove_view(app->view_dispatcher, CfsViewByteInput);
    byte_input_free(app->byte_input);
    view_dispatcher_remove_view(app->view_dispatcher, CfsViewTextInput);
    text_input_free(app->text_input);
    view_dispatcher_remove_view(app->view_dispatcher, CfsViewVariableItemList);
    variable_item_list_free(app->variable_item_list);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_string_free(app->selected_path);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t cfs_tag_writer_app(void* p) {
    UNUSED(p);
    CfsApp* app = cfs_app_alloc();

    scene_manager_next_scene(app->scene_manager, CfsSceneStart);
    view_dispatcher_run(app->view_dispatcher);

    cfs_app_free(app);
    return 0;
}
