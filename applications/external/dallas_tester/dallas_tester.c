#include "dallas_tester_i.h"

static bool dallas_tester_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    DallasTester* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool dallas_tester_back_event_callback(void* context) {
    furi_assert(context);
    DallasTester* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static DallasTester* dallas_tester_alloc(void) {
    DallasTester* app = malloc(sizeof(DallasTester));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->worker = dallas_test_worker_alloc();

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&dallas_tester_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, dallas_tester_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, dallas_tester_back_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DallasTesterViewSubmenu, submenu_get_view(app->submenu));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DallasTesterViewWidget, widget_get_view(app->widget));

    app->test_view = dallas_test_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DallasTesterViewTest, dallas_test_view_get_view(app->test_view));

    return app;
}

static void dallas_tester_free(DallasTester* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, DallasTesterViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_remove_view(app->view_dispatcher, DallasTesterViewWidget);
    widget_free(app->widget);
    view_dispatcher_remove_view(app->view_dispatcher, DallasTesterViewTest);
    dallas_test_view_free(app->test_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    dallas_test_worker_free(app->worker);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t dallas_tester_app(void* p) {
    UNUSED(p);

    DallasTester* app = dallas_tester_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, DallasTesterSceneStart);

    view_dispatcher_run(app->view_dispatcher);

    dallas_tester_free(app);
    return 0;
}
