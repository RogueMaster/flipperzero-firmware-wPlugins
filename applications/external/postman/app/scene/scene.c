
#include "../app.h"
#include "../structs.h"
#include <gui/view.h>

#include <gui/modules/dialog_ex.h> // Setup dialog (start up and checks)
#include "./setup_dialog/setup_dialog.h"

#include "./main_menu/main_menu.h" // Main menu

#include "./connect/connect.h" // Connect submenu (list wifi's) get list from uart and display
#include "./connect_favs/connect_favs.h" // Connect submenu (list wifi's) get list from csv and display
#include "./connect_details/connect_details.h" // Connect details submenu (connect, disconnect, save to csv)

#include "./text_input/text_input.h" // Text input for ssid and password, url, etc
#include "./display/display.h" // Text box for displaying uart responses

#include "./get/get.h" // Get View variable list
#include "./get_url_list/get_url_list.h" // Get URL list from csv

#include "./post/post.h" // Post View variable list
#include "./post_url_list/post_url_list.h"

#include "./download/download.h" // Download progress view

#include "./build_http_call/build_http_call.h" // Build Http View

#include "./build_http_list/build_http_list.h" // Build Http CSV list view

#include "./build_http_headers/build_http_headers.h" // Build Http headers view

/** collection of all scene on_enter handlers - in the same order as their enum
 */
void (*const scene_on_enter_handlers[])(void*) = {
    scene_on_enter_setup_dialog,
    scene_on_enter_main_menu,
    scene_on_enter_connect,
    scene_on_enter_connect_details,
    scene_on_enter_text_input,
    scene_on_enter_connect_favs,
    scene_on_enter_get,
    scene_on_enter_display,
    scene_on_enter_get_url_list,
    scene_on_enter_download_progress,
    scene_on_enter_post,
    scene_on_enter_post_url_list,
    scene_on_enter_build_http_call,
    scene_on_enter_build_http_list,
    scene_on_enter_build_http_headers};

/** collection of all scene on event handlers - in the same order as their enum
 */
bool (*const scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    scene_on_event_setup_dialog,
    scene_on_event_main_menu,
    scene_on_event_connect,
    scene_on_event_connect_details,
    scene_on_event_text_input,
    scene_on_event_connect_favs,
    scene_on_event_get,
    scene_on_event_display,
    scene_on_event_get_url_list,
    scene_on_event_download_progress,
    scene_on_event_post,
    scene_on_event_post_url_list,
    scene_on_event_build_http_call,
    scene_on_event_build_http_list,
    scene_on_event_build_http_headers};

/** collection of all scene on exit handlers - in the same order as their enum
 */
void (*const scene_on_exit_handlers[])(void*) = {
    scene_on_exit_setup_dialog,
    scene_on_exit_main_menu,
    scene_on_exit_connect,
    scene_on_exit_connect_details,
    scene_on_exit_text_input,
    scene_on_exit_connect_favs,
    scene_on_exit_get,
    scene_on_exit_display,
    scene_on_exit_get_url_list,
    scene_on_exit_download_progress,
    scene_on_exit_post,
    scene_on_exit_post_url_list,
    scene_on_exit_build_http_call,
    scene_on_exit_build_http_list,
    scene_on_exit_build_http_headers};

/** collection of all on_enter, on_event, on_exit handlers */
const SceneManagerHandlers scene_event_handlers = {
    .on_enter_handlers = scene_on_enter_handlers,
    .on_event_handlers = scene_on_event_handlers,
    .on_exit_handlers = scene_on_exit_handlers,
    .scene_num = Count};

/** custom event handler - passes the event to the scene manager */
bool scene_manager_custom_event_callback(void* context, uint32_t custom_event) {
    FURI_LOG_T(TAG, "scene_manager_custom_event_callback");
    furi_assert(context);
    App* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

/** navigation event handler - passes the event to the scene manager */
bool scene_manager_navigation_event_callback(void* context) {
    FURI_LOG_T(TAG, "scene_manager_navigation_event_callback");
    App* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

/** initialise the scene manager with all handlers */
void scene_manager_init(App* app) {
    FURI_LOG_T(TAG, "scene_manager_init");
    app->scene_manager = scene_manager_alloc(&scene_event_handlers, app);
}

/** initialise the views, and initialise the view dispatcher with all views */
void view_dispatcher_init(App* app) {
    FURI_LOG_T(TAG, "view_dispatcher_init");
    app->view_dispatcher = view_dispatcher_alloc();

    // allocate each view
    FURI_LOG_D(TAG, "view_dispatcher_init allocating views");
    // Start up and board checks
    app->dialog = dialog_ex_alloc();

    // Main menu
    app->menu = menu_alloc();

    // Connect submenu (list wifi's)
    app->submenu = submenu_alloc();

    // Text input for ssid and password
    app->text_input = uart_text_input_alloc();
    app->is_custom_tx_string = false;
    app->selected_tx_string = "";

    //Text box for displaying uart responses
    app->text_box = widget_alloc();

    // Download view
    app->view = view_alloc();

    // Get / Post view
    app->variable_item_list = variable_item_list_alloc();

    // assign callback that pass events from views to the scene manager
    FURI_LOG_D(TAG, "view_dispatcher_init setting callbacks");
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, scene_manager_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, scene_manager_navigation_event_callback);

    // MAIN MENU
    // add views to the dispatcher, indexed by their enum value
    FURI_LOG_D(TAG, "view_dispatcher_init adding view menu");

    view_dispatcher_add_view(
        app->view_dispatcher, AppView_SetupDialog, dialog_ex_get_view(app->dialog));

    view_dispatcher_add_view(app->view_dispatcher, AppView_Menu, menu_get_view(app->menu));

    view_dispatcher_add_view(
        app->view_dispatcher, AppView_Connect, submenu_get_view(app->submenu));

    view_dispatcher_add_view(
        app->view_dispatcher, AppView_Connect_Details, submenu_get_view(app->submenu));

    view_dispatcher_add_view(
        app->view_dispatcher,
        AppView_Connect_Text_Input,
        uart_text_input_get_view(app->text_input));

    view_dispatcher_add_view(
        app->view_dispatcher, AppView_Connect_Favs, submenu_get_view(app->submenu));

    view_dispatcher_add_view(
        app->view_dispatcher, AppView_Get, variable_item_list_get_view(app->variable_item_list));

    view_dispatcher_add_view(
        app->view_dispatcher, AppView_Display, widget_get_view(app->text_box));

    view_dispatcher_add_view(
        app->view_dispatcher, AppView_Get_Url_List, submenu_get_view(app->submenu));

    view_dispatcher_add_view(app->view_dispatcher, AppView_Download, app->view);

    view_dispatcher_add_view(
        app->view_dispatcher, AppView_Post, variable_item_list_get_view(app->variable_item_list));

    view_dispatcher_add_view(
        app->view_dispatcher, AppView_Post_Url_List, submenu_get_view(app->submenu));

    view_dispatcher_add_view(
        app->view_dispatcher,
        AppView_BuildHttpCall,
        variable_item_list_get_view(app->variable_item_list));

    view_dispatcher_add_view(
        app->view_dispatcher, AppView_BuildHttp_Url_List, submenu_get_view(app->submenu));

    view_dispatcher_add_view(
        app->view_dispatcher,
        AppView_BuildHttp_Headers,
        variable_item_list_get_view(app->variable_item_list));
}
