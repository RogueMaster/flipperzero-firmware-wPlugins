#include "cogs_mikai.h"

static bool cogs_mikai_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    COGSMyKaiApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool cogs_mikai_back_event_callback(void* context) {
    furi_assert(context);
    COGSMyKaiApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static COGSMyKaiApp* cogs_mikai_app_alloc() {
    COGSMyKaiApp* app = malloc(sizeof(COGSMyKaiApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&cogs_mikai_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, cogs_mikai_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, cogs_mikai_back_event_callback);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Allocate views
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, COGSMyKaiViewSubmenu, submenu_get_view(app->submenu));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, COGSMyKaiViewTextInput, text_input_get_view(app->text_input));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, COGSMyKaiViewPopup, popup_get_view(app->popup));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, COGSMyKaiViewWidget, widget_get_view(app->widget));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, COGSMyKaiViewTextBox, text_box_get_view(app->text_box));
    app->text_box_store = furi_string_alloc();

    // Initialize MyKey data
    memset(&app->mykey, 0, sizeof(MyKeyData));
    app->mykey.is_loaded = false;

    scene_manager_next_scene(app->scene_manager, COGSMyKaiSceneStart);

    return app;
}

static void cogs_mikai_app_free(COGSMyKaiApp* app) {
    furi_assert(app);

    // Remove views
    view_dispatcher_remove_view(app->view_dispatcher, COGSMyKaiViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, COGSMyKaiViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, COGSMyKaiViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, COGSMyKaiViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, COGSMyKaiViewTextBox);

    submenu_free(app->submenu);
    text_input_free(app->text_input);
    popup_free(app->popup);
    widget_free(app->widget);
    text_box_free(app->text_box);
    furi_string_free(app->text_box_store);

    // Free view dispatcher and scene manager
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Close records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_NOTIFICATION);

    free(app);
}

int32_t cogs_mikai_app(void* p) {
    UNUSED(p);
    COGSMyKaiApp* app = cogs_mikai_app_alloc();

    FURI_LOG_I(TAG, "COGS MyKai app started");

    view_dispatcher_run(app->view_dispatcher);

    FURI_LOG_I(TAG, "COGS MyKai app stopped");

    cogs_mikai_app_free(app);

    return 0;
}
