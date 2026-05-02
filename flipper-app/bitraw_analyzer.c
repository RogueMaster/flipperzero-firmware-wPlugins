#include "bitraw_analyzer_app.h"
#include "scenes/scenes.h"
#include "analyzer/sub_parser.h"

static bool bitraw_back_event_callback(void* context) {
    BitrawApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static bool bitraw_custom_event_callback(void* context, uint32_t event) {
    BitrawApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static BitrawApp* bitraw_app_alloc(void) {
    BitrawApp* app = malloc(sizeof(BitrawApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&bitraw_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, bitraw_back_event_callback);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, bitraw_custom_event_callback);

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BitrawViewTextBox, text_box_get_view(app->text_box));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BitrawViewPopup, popup_get_view(app->popup));

    app->selected_path = furi_string_alloc();
    app->report = furi_string_alloc();
    app->parse_error = furi_string_alloc();

    sub_file_init(&app->sub);

    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void bitraw_app_free(BitrawApp* app) {
    sub_file_reset(&app->sub);

    view_dispatcher_remove_view(app->view_dispatcher, BitrawViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, BitrawViewPopup);
    text_box_free(app->text_box);
    popup_free(app->popup);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_string_free(app->selected_path);
    furi_string_free(app->report);
    furi_string_free(app->parse_error);

    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t bitraw_analyzer_app(void* p) {
    UNUSED(p);
    BitrawApp* app = bitraw_app_alloc();

    scene_manager_next_scene(app->scene_manager, BitrawSceneBrowser);
    view_dispatcher_run(app->view_dispatcher);

    bitraw_app_free(app);
    return 0;
}
