#include "kyberwrite_app.h"
#include "scenes/kyberwrite_scene.h"

static bool kyberwrite_scene_custom_callback(void* context, uint32_t custom_event) {
    furi_assert(context);
    KyberApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

static bool kyberwrite_scene_back_callback(void* context) {
    furi_assert(context);
    KyberApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static KyberApp* kyberwrite_alloc(void) {
    KyberApp* app = malloc(sizeof(KyberApp));

    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->show_hints    = true; // Set a default

    app->scene_manager = scene_manager_alloc(&kyberwrite_scene_handlers, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, kyberwrite_scene_custom_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, kyberwrite_scene_back_callback);
    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->menu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, KyberViewMenu, submenu_get_view(app->menu));

    app->color_menu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, KyberViewColorSelect, submenu_get_view(app->color_menu));

    app->writing_widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, KyberViewWriting, widget_get_view(app->writing_widget));

    app->result_popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, KyberViewResult, popup_get_view(app->result_popup));
    
    app->splash_widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, KyberViewSplash, widget_get_view(app->splash_widget));

    app->hint_widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, KyberViewHint, widget_get_view(app->hint_widget));

    app->read_widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, KyberViewRead, widget_get_view(app->read_widget));

    // RFID — used by read scene
    app->protocol_dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
    app->rfid_worker   = lfrfid_worker_alloc(app->protocol_dict);
    lfrfid_worker_start_thread(app->rfid_worker);

    app->mode          = KyberModeFullInit;
    app->crystal_index = 0;
    app->write_success = false;

    return app;
}

static void kyberwrite_free(KyberApp* app) {
    furi_assert(app);

    lfrfid_worker_stop(app->rfid_worker);
    lfrfid_worker_stop_thread(app->rfid_worker);
    lfrfid_worker_free(app->rfid_worker);
    protocol_dict_free(app->protocol_dict);

    view_dispatcher_remove_view(app->view_dispatcher, KyberViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, KyberViewColorSelect);
    view_dispatcher_remove_view(app->view_dispatcher, KyberViewWriting);
    view_dispatcher_remove_view(app->view_dispatcher, KyberViewResult);
    view_dispatcher_remove_view(app->view_dispatcher, KyberViewRead);
    view_dispatcher_remove_view(app->view_dispatcher, KyberViewSplash);
    view_dispatcher_remove_view(app->view_dispatcher, KyberViewHint);

    

    submenu_free(app->menu);
    submenu_free(app->color_menu);
    widget_free(app->writing_widget);
    widget_free(app->splash_widget);
    widget_free(app->hint_widget);
    popup_free(app->result_popup);
    widget_free(app->read_widget);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t kyberwrite_app(void* p) {
    UNUSED(p);
    KyberApp* app = kyberwrite_alloc();
    // scene_manager_next_scene(app->scene_manager, KyberSceneMenu);
    scene_manager_next_scene(app->scene_manager, KyberSceneSplash);
    view_dispatcher_run(app->view_dispatcher);
    kyberwrite_free(app);
    return 0;
}
