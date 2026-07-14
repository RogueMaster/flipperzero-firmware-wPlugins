#include "app_user.h"

void makePaths(App* app) {
    furi_assert(app);
    if(!storage_simply_mkdir(app->storage, PATHEXPORTS)) {
        dialog_message_show_storage_error(app->dialogs, "Cannot create\nexports folder");
    }
    if(!storage_simply_mkdir(app->storage, PATHLOGS)) {
        dialog_message_show_storage_error(app->dialogs, "Cannot create\nlogs folder");
    }
}

static bool app_scene_costum_callback(void* context, uint32_t costum_event) {
    furi_assert(context);
    App* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, costum_event);
}

static bool app_scene_back_event(void* context) {
    furi_assert(context);
    App* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void app_tick_event(void* context) {
    furi_assert(context);
    App* app = context;
    UNUSED(app);
}

static App* app_alloc() {
    App* app = malloc(sizeof(App));
    app->scene_manager = scene_manager_alloc(&app_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, app_scene_costum_callback);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, app_scene_back_event);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, app_tick_event, 100);

    app->loading = loading_alloc();
    view_dispatcher_add_view(app->view_dispatcher, LoadingView, loading_get_view(app->loading));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ViewWidget, widget_get_view(app->widget));

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, SubmenuView, submenu_get_view(app->submenu));

    app->varList = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, VarListView, variable_item_list_get_view(app->varList));

    app->textBox = text_box_alloc();
    view_dispatcher_add_view(app->view_dispatcher, TextBoxView, text_box_get_view(app->textBox));

    app->input_byte_value = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, InputByteView, byte_input_get_view(app->input_byte_value));

    app->dialog_ex = dialog_ex_alloc();
    view_dispatcher_add_view(app->view_dispatcher, DialogView, dialog_ex_get_view(app->dialog_ex));
    view_dispatcher_add_view(app->view_dispatcher, SubmenuLogView, submenu_get_view(app->submenu));
    app->frame_active = frame_can_alloc();
    app->file_active = file_active_alloc();
    app->can_send_frame = (bool*)calloc(1, sizeof(bool));
    app->send_timestamp = (bool*)calloc(1, sizeof(bool));

    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->log_file = storage_file_alloc(app->storage);

    app->text = furi_string_alloc();
    app->data = furi_string_alloc();
    app->path = furi_string_alloc();

    furi_string_reset(app->data);
    furi_string_cat_printf(app->data, "---");

    app->frame_queue = frame_can_queue_alloc();

    app->file_browser = file_browser_alloc(app->path);
    view_dispatcher_add_view(
        app->view_dispatcher, FileBrowserView, file_browser_get_view(app->file_browser));

    app->mcp_can = mcp_alloc(MCP_NORMAL, MCP_16MHZ, MCP_500KBPS);

    app->frameArray = (CANFRAME*)calloc(100, sizeof(CANFRAME));

    app->log_file_path = (char*)malloc(100 * sizeof(char));

    app->frame_to_send = malloc(sizeof(CANFRAME));

    app->obdii.bitrate = app->mcp_can->bitRate;

    app->uds_received_id = UDS_RESPONSE_ID_DEFAULT;
    app->uds_send_id = UDS_REQUEST_ID_DEFAULT;

    makePaths(app);

    return app;
}

static void app_free(App* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, LoadingView);
    view_dispatcher_remove_view(app->view_dispatcher, SubmenuView);
    view_dispatcher_remove_view(app->view_dispatcher, ViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, TextBoxView);
    view_dispatcher_remove_view(app->view_dispatcher, SubmenuLogView);
    view_dispatcher_remove_view(app->view_dispatcher, DialogView);
    view_dispatcher_remove_view(app->view_dispatcher, VarListView);
    view_dispatcher_remove_view(app->view_dispatcher, InputByteView);
    view_dispatcher_remove_view(app->view_dispatcher, FileBrowserView);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    widget_free(app->widget);
    submenu_free(app->submenu);
    text_box_free(app->textBox);
    byte_input_free(app->input_byte_value);
    file_browser_free(app->file_browser);
    loading_free(app->loading);

    furi_string_free(app->text);
    furi_string_free(app->data);
    furi_string_free(app->path);

    frame_can_queue_free(app->frame_queue);

    if(app->log_file && storage_file_is_open(app->log_file)) {
        storage_file_close(app->log_file);
    }

    storage_file_free(app->log_file);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);

    file_active_free(app->file_active);
    frame_can_free(app->frame_active);
    dialog_ex_free(app->dialog_ex);
    free(app->can_send_frame);
    free(app->send_timestamp);

    free(app->log_file_path);
    free(app->frameArray);

    free_mcp2515(app->mcp_can);

    free(app);
}

int app_main(void* p) {
    UNUSED(p);

    SLCAN_open_port();

    App* app = app_alloc();

    Gui* gui = furi_record_open(RECORD_GUI);

    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(app->scene_manager, app_scene_main_menu);

    view_dispatcher_run(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    app_free(app);

    SLCAN_close_port();

    return 0;
}
