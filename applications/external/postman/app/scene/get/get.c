#include "../../app.h"
#include "../../structs.h"
#include "get.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/modules/variable_item_list.h>
#include "../../csv/csv_get_url/csv_get_url.h"
#include "../../csv/csv_utils/csv_utils.h"

static void get_scene_mode_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    furi_assert(app);
    furi_assert(app->get_state);

    // Toggle mode
    app->get_state->mode = !app->get_state->mode;

    // Update the display text
    variable_item_set_current_value_text(item, app->get_state->mode ? "Save" : "Display");
    draw_get_menu(app);
}

static void get_scene_method_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    furi_assert(app);
    furi_assert(app->get_state);

    // Toggle method
    app->get_state->method = !app->get_state->method;

    // Update the display text
    variable_item_set_current_value_text(item, app->get_state->method ? "Stream" : "Get");
}

static void get_scene_select_callback(void* context, uint32_t index) {
    App* app = context;
    furi_assert(app);
    furi_assert(app->view_dispatcher);

    FURI_LOG_D(TAG, "get_scene_select_callback: %ld", index);
    // index from 0 till 2, pass the index to the dispatcher
    // The first 3 buttons are always there
    if(index == 0 || index == 1 || index == 2) {
        view_dispatcher_send_custom_event(app->view_dispatcher, index);
        return;
    }

    if(strlen(app->get_state->url) > 0 && index == 3) {
        view_dispatcher_send_custom_event(app->view_dispatcher, index);
    }

    bool url_found = url_in_csv(app, app->get_state->url, StateTypeGet);

    if(strlen(app->get_state->url) > 0 && index == 4) {
        if(url_found) {
            view_dispatcher_send_custom_event(app->view_dispatcher, GetItemDeleteFromCsv);
        } else {
            view_dispatcher_send_custom_event(app->view_dispatcher, GetItemSaveToCsv);
        }
    }

    if(app->url_list && app->url_list_count > 0 && strlen(app->url_list[0].url) > 0) {
        if(!(strlen(app->get_state->url) > 0) && index == 3) {
            view_dispatcher_send_custom_event(app->view_dispatcher, GetItemLoadFromCsv);
        }

        if(index == 5) {
            view_dispatcher_send_custom_event(app->view_dispatcher, GetItemLoadFromCsv);
        }
    }
}

void draw_get_menu(App* app) {
    furi_assert(app);
    furi_assert(app->variable_item_list);
    VariableItemList* variable_item_list = app->variable_item_list;

    variable_item_list_reset(variable_item_list);

    if(!app->get_state) {
        app->get_state = malloc(sizeof(GetState));
        if(!app->get_state) {
            FURI_LOG_E(TAG, "Failed to allocate get_state");
            return;
        }
        app->get_state->mode = false; // Default mode: Display
        strcpy(app->get_state->url, ""); // Default URL: empty
    }

    // Add items to the variable item list and set their values
    VariableItem* item;

    item = variable_item_list_add(variable_item_list, "Mode", 2, get_scene_mode_callback, app);
    variable_item_set_current_value_text(item, app->get_state->mode ? "Save" : "Display");

    if(!app->get_state->mode) {
        // if not save to file mode, add method
        item = variable_item_list_add(
            variable_item_list, "Method", 2, get_scene_method_callback, app);
        variable_item_set_current_value_text(item, app->get_state->method ? "Stream" : "Get");
    } else {
        // if save to file mode, add filename
        item = variable_item_list_add(variable_item_list, "Method", 1, NULL, app);
        variable_item_set_current_value_text(item, "Stream to file");
    }

    item = variable_item_list_add(
        variable_item_list, strlen(app->get_state->url) > 0 ? "Edit Url" : "Set Url", 0, NULL, app);

    if(strlen(app->get_state->url) > 0) {
        item = variable_item_list_add(
            variable_item_list,
            app->get_state->mode ? "Save to File" : "Send Request",
            0,
            NULL,
            app);
    }

    // Check if current URL is in the csv list
    bool url_found = url_in_csv(app, app->get_state->url, StateTypeGet);
    FURI_LOG_D(TAG, "URL in CSV: %d", url_found);

    // Check if the URL is in the CSV
    if(strlen(app->get_state->url) > 0) {
        item = variable_item_list_add(
            variable_item_list, url_found ? "Delete from CSV" : "Save to CSV", 0, NULL, app);
    }

    if(app->url_list && app->url_list_count > 0 && strlen(app->url_list[0].url) > 0) {
        item = variable_item_list_add(variable_item_list, "Load from CSV", 0, NULL, app);
    }
}

void scene_on_enter_get(void* context) {
    FURI_LOG_T(TAG, "scene_on_enter_get");
    App* app = context;
    furi_assert(app);
    furi_assert(app->variable_item_list);
    VariableItemList* variable_item_list = app->variable_item_list;

    // Initialize get_state if not already initialized
    if(!app->get_state) {
        app->get_state = malloc(sizeof(GetState));
        if(!app->get_state) {
            FURI_LOG_E(TAG, "Failed to allocate get_state");
            return;
        }
        app->get_state->mode = false; // Default mode: Display
        app->get_state->method = false; // Default method: GET
        strcpy(app->get_state->url, ""); // Default URL: empty
    }

    variable_item_list_set_enter_callback(variable_item_list, get_scene_select_callback, app);
    draw_get_menu(app);
    variable_item_list_set_selected_item(variable_item_list, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_Get);
}

bool scene_on_event_get(void* context, SceneManagerEvent event) {
    FURI_LOG_T(TAG, "scene_on_event_get");
    App* app = context;
    furi_assert(app);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case GetItemSetUrl:
            if(strcmp(app->get_state->url, "") == 0) {
                strcpy(app->get_state->url, "https://");
            }
            app->selected_tx_string = app->get_state->url;
            app->text_input_state = TextInputState_GetUrl;
            scene_manager_next_scene(app->scene_manager, Text_Input);
            consumed = true;
            break;
        case GetItemSaveToCsv:
            FURI_LOG_D(TAG, "Save to CSV");
            const char* url_to_write = app->get_state->url;
            furi_assert(url_to_write);

            if(!write_url_to_csv(app, url_to_write)) {
                FURI_LOG_E(TAG, "Failed to write URL to CSV");
                break;
            }
            sync_csv_get_url_to_mem(app);
            draw_get_menu(app);
            consumed = true;
            break;
        case GetItemToggleViewSave:
            // No logic
            consumed = true;
            break;
        case GetItemDeleteFromCsv:
            FURI_LOG_D(TAG, "Delete from CSV");
            const char* url_to_delete = app->get_state->url;
            furi_assert(url_to_delete);

            if(!delete_url_from_csv(app, url_to_delete)) {
                FURI_LOG_E(TAG, "Failed to delete URL from CSV");
                break;
            }
            sync_csv_get_url_to_mem(app);
            draw_get_menu(app);
            consumed = true;
            break;
        case GetItemLoadFromCsv:
            FURI_LOG_D(TAG, "Load from CSV");
            scene_manager_next_scene(app->scene_manager, Get_Url_List);
            consumed = true;
            break;
        case GetItemAction:
            // Logic to handle action

            // Depending on the mode, either save to file or send request
            // If mode is save to file, save to file

            if(app->get_state->mode) {
                //Set download for post
                app->download_mode = DOWNLOAD_GET;
                // If save to file mode, move to filename input
                furi_string_reset(app->text_box_store);
                app->text_input_state = TextInputState_Filename;
                scene_manager_next_scene(app->scene_manager, Text_Input);

            } else {
                // If mode is send request, send request

                // If method is GET, send GET request to the URL
                // If method is GET_STREAM, send GET_STREAM request to the URL
                if(app->get_state->method) {
                    app->display_mode = DISPLAY_GET_STREAM;
                } else {
                    app->display_mode = DISPLAY_GET;
                }
                scene_manager_next_scene(app->scene_manager, Display);
            }
            consumed = true;
            break;
        default:
            consumed = false;
            break;
        }
    }

    return consumed;
}

void scene_on_exit_get(void* context) {
    FURI_LOG_T(TAG, "scene_on_exit_get");
    App* app = context;
    furi_assert(app);
    furi_assert(app->variable_item_list);
    variable_item_list_reset(app->variable_item_list);
}
