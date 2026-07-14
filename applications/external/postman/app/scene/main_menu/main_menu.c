#include "../../app.h"
#include "../../structs.h"
#include <datetime/datetime.h>
#include <furi.h>

#include <postmanflipx_icons.h>

/** main menu callback - sends a custom event to the scene manager based on the
 * menu selection */
void menu_callback_main_menu(void* context, uint32_t index) {
    FURI_LOG_T(TAG, "menu_callback_main_menu");
    App* app = context;
    switch(index) {
    case MenuSelection_Connect:
        scene_manager_handle_custom_event(app->scene_manager, AppEvent_Connect);
        break;
    case MenuSelection_Get:
        scene_manager_handle_custom_event(app->scene_manager, AppEvent_Get);
        break;
    case MenuSelection_Post:
        scene_manager_handle_custom_event(app->scene_manager, AppEvent_Post);
        break;
    case MenuSelection_Exit:
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        break;
    case MenuSelection_Connect_Favs:
        scene_manager_handle_custom_event(app->scene_manager, AppEvent_Connect_Favs);
        break;
    case MenuSelection_Download:
        scene_manager_handle_custom_event(app->scene_manager, AppEvent_Download);
        break;
    case MenuSelection_Build_http:
        scene_manager_handle_custom_event(app->scene_manager, AppEvent_Build_Http);
    }
}

void scene_on_enter_main_menu(void* context) {
    FURI_LOG_T(TAG, "scene_on_enter_main_menu");
    App* app = context;

    menu_reset(app->menu);

    if(app->status == BOARD_CONNECTED_WIFI_ON && app->wifi_list.connected_ssid[0] != '\0') {
        menu_add_item(
            app->menu,
            "Wifi (Connected)",
            &A_Connect_14,
            MenuSelection_Connect,
            menu_callback_main_menu,
            app);

        if(app->csv_networks[0].ssid[0] != '\0') {
            menu_add_item(
                app->menu,
                "Wifi Favs",
                &A_Bookmark_14,
                MenuSelection_Connect_Favs,
                menu_callback_main_menu,
                app);
        }

        menu_add_item(
            app->menu, "Get", &A_Get_14, MenuSelection_Get, menu_callback_main_menu, app);
        menu_add_item(
            app->menu, "Post", &A_Post_14, MenuSelection_Post, menu_callback_main_menu, app);
        menu_add_item(
            app->menu,
            "Http Builder",
            &A_Settings_14,
            MenuSelection_Build_http,
            menu_callback_main_menu,
            app);
        menu_add_item(
            app->menu,
            "Download Files",
            &A_Get_14,
            MenuSelection_Download,
            menu_callback_main_menu,
            app);
        menu_add_item(
            app->menu,
            "Open listing port",
            &A_Listen_14,
            MenuSelection_Listen,
            menu_callback_main_menu,
            app);

    } else {
        menu_add_item(
            app->menu,
            "Wifi (Disconnected)",
            &A_Connect_14,
            MenuSelection_Connect,
            menu_callback_main_menu,
            app);

        if(app->csv_networks[0].ssid[0] != '\0') {
            menu_add_item(
                app->menu,
                "Wifi Favs",
                &A_Connect_14,
                MenuSelection_Connect_Favs,
                menu_callback_main_menu,
                app);
        }
    }

    menu_add_item(
        app->menu, "About", &A_About_14, MenuSelection_About, menu_callback_main_menu, app);
    menu_add_item(app->menu, "Exit", &A_Exit_14, MenuSelection_Exit, menu_callback_main_menu, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_Menu);
}

/** main menu event handler - switches scene based on the event */
bool scene_on_event_main_menu(void* context, SceneManagerEvent event) {
    FURI_LOG_T(TAG, "scene_on_event_main_menu");
    App* app = context;
    bool consumed = false;

    FURI_LOG_D("DEBUG", "Connected to: %s", app->wifi_list.connected_ssid);
    switch(event.type) {
    case(SceneManagerEventTypeBack):
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
        break;
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case AppEvent_SetupDialog:
            scene_manager_next_scene(app->scene_manager, MainMenu);
            consumed = true;
            break;
        case AppEvent_MainMenu:
            scene_manager_next_scene(app->scene_manager, MainMenu);
            consumed = true;
            break;
        case AppEvent_Connect:
            scene_manager_next_scene(app->scene_manager, Connect);
            consumed = true;
            break;
        case AppEvent_Connect_Favs:
            scene_manager_next_scene(app->scene_manager, Connect_Favs);
            consumed = true;
            break;
        case AppEvent_Get:
            scene_manager_next_scene(app->scene_manager, Get);
            consumed = true;
            break;
        case AppEvent_Post:
            scene_manager_next_scene(app->scene_manager, Post);
            consumed = true;
            break;
        case AppEvent_Download:
            if(!app->get_state) {
                app->get_state = malloc(sizeof(GetState));
                app->get_state->mode = true; // Default mode: Display
                app->get_state->method = true; // Default method: GET
                strcpy(app->get_state->url, ""); // Default URL: empty
            } else {
                app->get_state->mode = true; // Default mode: Save to file
                app->get_state->method = true; // Default method: Stream
            }
            scene_manager_next_scene(app->scene_manager, Get);
            consumed = true;
            break;
        case AppEvent_Build_Http:
            scene_manager_next_scene(app->scene_manager, Build_Http);
            consumed = true;
        }
        break;
    default: // eg. SceneManagerEventTypeBack, SceneManagerEventTypeTick
        consumed = false;
    }

    return consumed;
}

void scene_on_exit_main_menu(void* context) {
    FURI_LOG_T(TAG, "scene_on_exit_main_menu");

    App* app = context;
    FURI_LOG_D("DEBUG", "Connected to: %s", app->wifi_list.connected_ssid);

    menu_reset(app->menu);
}
