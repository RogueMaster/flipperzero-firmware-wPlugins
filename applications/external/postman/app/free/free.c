#include "../app.h"
#include "../structs.h"
#include "../uart/uart.h"
#include "../csv/csv_build_url/csv_build_url.h"
#include <furi.h>
#include <gui/gui.h>
#include <gui/icon_i.h>
#include <gui/modules/menu.h>
#include <gui/modules/popup.h>
#include <gui/modules/submenu.h>
#include <gui/view.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <storage/storage.h>

void app_free(App* app) {
    FURI_LOG_T(TAG, "free");
    uart_terminal_uart_free(app->uart);
    scene_manager_free(app->scene_manager);

    // Scene on exit handlers
    view_dispatcher_remove_view(app->view_dispatcher, AppView_SetupDialog);
    view_dispatcher_remove_view(app->view_dispatcher, AppView_Menu);
    view_dispatcher_remove_view(app->view_dispatcher, AppView_Connect);
    view_dispatcher_remove_view(app->view_dispatcher, AppView_Connect_Details);
    view_dispatcher_remove_view(app->view_dispatcher, AppView_Connect_Text_Input);
    view_dispatcher_remove_view(app->view_dispatcher, AppView_Connect_Favs);
    view_dispatcher_remove_view(app->view_dispatcher, AppView_Get);
    view_dispatcher_remove_view(app->view_dispatcher, AppView_Display);
    view_dispatcher_remove_view(app->view_dispatcher, AppView_Get_Url_List);
    view_dispatcher_remove_view(app->view_dispatcher, AppView_Download);
    view_dispatcher_remove_view(app->view_dispatcher, AppView_Post);
    view_dispatcher_remove_view(app->view_dispatcher, AppView_Post_Url_List);
    view_dispatcher_remove_view(app->view_dispatcher, AppView_BuildHttpCall);
    view_dispatcher_remove_view(app->view_dispatcher, AppView_BuildHttp_Url_List);

    // Memory for the UI
    menu_free(app->menu);
    submenu_free(app->submenu);
    variable_item_list_free(app->variable_item_list);
    dialog_ex_free(app->dialog);
    widget_free(app->text_box);
    uart_text_input_free(app->text_input);
    view_free(app->view);

    if(app->get_state) {
        free(app->get_state);
        app->get_state = NULL;
    }

    // Memory clear for the GET CSV imported items
    if(app->url_list) {
        for(size_t i = 0; i < app->url_list_count; i++) {
            // No need to free individual URLs if they are not dynamically allocated
        }
        free(app->url_list); // Free the dynamic array itself
        app->url_list = NULL;
        app->url_list_count = 0;
    }

    if(app->post_state) {
        if(app->post_state->payload) {
            furi_string_free(app->post_state->payload);
        }
        free(app->post_state);
        app->post_state = NULL;
    }

    // Memory clear for the Post CSV imported items
    if(app->post_url_list) {
        for(size_t i = 0; i < app->post_url_list_count; i++) {
            if(app->post_url_list[i].payload) {
                furi_string_free(app->post_url_list[i].payload);
            }
            // No need to free individual URLs if they are not dynamically allocated
        }
        free(app->post_url_list); // Free the dynamic array itself
        app->post_url_list = NULL;
        app->post_url_list_count = 0;
    }

    // Memory clear for the WiFi Networks CSV imported items
    if(app->csv_networks) {
        for(size_t i = 0; i < app->csv_networks_count; i++) {
            // No need to free individual SSIDs and passwords if they are not dynamically allocated
        }
        free(app->csv_networks); // Free the dynamic array itself
        app->csv_networks = NULL;
        app->csv_networks_count = 0;
    }

    if(app->build_http_state) {
        if(app->build_http_state->payload) {
            furi_string_free(app->build_http_state->payload);
        }
        free(app->build_http_state);
        app->build_http_state = NULL;
    }

    free_build_http_list(app);

    // Free text box store
    furi_string_free(app->text_box_store);
    furi_timer_free(app->timer);
    furi_record_close(RECORD_STORAGE);
    free(app);
}
