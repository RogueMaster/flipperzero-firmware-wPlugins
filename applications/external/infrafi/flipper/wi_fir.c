#include "wi_fir.h"

static bool wi_fir_custom_event_callback(void* context, uint32_t event) {
    WiFirApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool wi_fir_back_event_callback(void* context) {
    WiFirApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static WiFirApp* wi_fir_alloc(void) {
    WiFirApp* app = malloc(sizeof(WiFirApp));
    memset(app, 0, sizeof(WiFirApp));

    /* Default security type */
    app->security_type = WFR_SEC_WPA;
    app->ack_enabled = false;

    /* Open system services */
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);

    /* Allocate ViewDispatcher and SceneManager */
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&wi_fir_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, wi_fir_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, wi_fir_back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* Allocate view modules and register with ViewDispatcher */
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, WiFirViewSubmenu, submenu_get_view(app->submenu));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, WiFirViewTextInput, text_input_get_view(app->text_input));

    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        WiFirViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));

    app->dialog_ex = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, WiFirViewDialogEx, dialog_ex_get_view(app->dialog_ex));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, WiFirViewPopup, popup_get_view(app->popup));

    app->loading = loading_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, WiFirViewLoading, loading_get_view(app->loading));

    /* Load settings from SD card */
    wfr_storage_load_settings(app->storage, &app->ack_enabled);

    return app;
}

static void wi_fir_free(WiFirApp* app) {
    /* Remove views before freeing modules */
    view_dispatcher_remove_view(app->view_dispatcher, WiFirViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, WiFirViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, WiFirViewVariableItemList);
    view_dispatcher_remove_view(app->view_dispatcher, WiFirViewDialogEx);
    view_dispatcher_remove_view(app->view_dispatcher, WiFirViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, WiFirViewLoading);

    /* Free view modules */
    submenu_free(app->submenu);
    text_input_free(app->text_input);
    variable_item_list_free(app->variable_item_list);
    dialog_ex_free(app->dialog_ex);
    popup_free(app->popup);
    loading_free(app->loading);

    /* Free scene manager and view dispatcher */
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    /* Close system services */
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    free(app);
}

int32_t wi_fir_app(void* p) {
    UNUSED(p);

    WiFirApp* app = wi_fir_alloc();

    scene_manager_next_scene(app->scene_manager, WiFirSceneMainMenu);
    view_dispatcher_run(app->view_dispatcher);

    wi_fir_free(app);
    return 0;
}
