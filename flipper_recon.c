#include "flipper_recon_i.h"

bool flipper_recon_perform_save(FlipperReconApp* app) {
    furi_assert(app);
    char newbase[RECON_NAME_LEN];
    recon_sanitize_filename(app->session->name, newbase, sizeof(newbase));
    bool renamed = app->session_file[0] && strcmp(app->session_file, newbase) != 0;

    bool ok = recon_storage_save_session(app->storage, app->session);
    if(ok) {
        if(renamed) recon_storage_delete_session(app->storage, app->session_file);
        strncpy(app->session_file, newbase, RECON_NAME_LEN - 1);
        app->session_file[RECON_NAME_LEN - 1] = '\0';
        session_mark_clean(app->session);
        furi_string_printf(app->message_text, "Saved engagement:\n%s", newbase);
        notification_message(app->notifications, &sequence_success);
    } else {
        furi_string_set(app->message_text, "Save failed");
        notification_message(app->notifications, &sequence_error);
    }
    app->message_mode = ReconMessageInfo;
    return ok;
}

static bool flipper_recon_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    FlipperReconApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool flipper_recon_back_event_callback(void* context) {
    furi_assert(context);
    FlipperReconApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void flipper_recon_tick_event_callback(void* context) {
    furi_assert(context);
    FlipperReconApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

static FlipperReconApp* flipper_recon_app_alloc(void) {
    FlipperReconApp* app = malloc(sizeof(FlipperReconApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->session = session_alloc();
    app->message_text = furi_string_alloc();
    app->session_file[0] = '\0';
    app->session_names_count = 0;

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&flipper_recon_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, flipper_recon_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, flipper_recon_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, flipper_recon_tick_event_callback, 500);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ReconViewSubmenu, submenu_get_view(app->submenu));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ReconViewTextInput, text_input_get_view(app->text_input));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        ReconViewVarItemList,
        variable_item_list_get_view(app->var_item_list));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ReconViewWidget, widget_get_view(app->widget));

    app->graph_view = graph_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ReconViewGraph, graph_view_get_view(app->graph_view));

    recon_storage_ensure_dirs(app->storage);

    return app;
}

static void flipper_recon_app_free(FlipperReconApp* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, ReconViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewVarItemList);
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewGraph);

    submenu_free(app->submenu);
    text_input_free(app->text_input);
    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);
    graph_view_free(app->graph_view);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    for(size_t i = 0; i < app->session_names_count; i++) {
        furi_string_free(app->session_names[i]);
    }
    furi_string_free(app->message_text);
    session_free(app->session);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t flipper_recon_app(void* p) {
    UNUSED(p);
    FlipperReconApp* app = flipper_recon_app_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, FlipperReconSceneStart);

    view_dispatcher_run(app->view_dispatcher);

    flipper_recon_app_free(app);
    return 0;
}
