#include "../../app.h"
#include "../../structs.h"
#include "post.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/modules/variable_item_list.h>
#include "../../csv/csv_post_url/csv_post_url.h"
#include "../../csv/csv_utils/csv_utils.h"

static void post_scene_select_callback(void* context, uint32_t index) {
    App* app = context;
    furi_assert(app);
    furi_assert(app->view_dispatcher);

    FURI_LOG_D(TAG, "post_scene_select_callback: %ld", index);

    // The first 4 buttons are always there (Mode, Method, Set/Edit URL, Set Payload)
    if(index <= 3) {
        view_dispatcher_send_custom_event(app->view_dispatcher, index);
        return;
    }

    // Send Request / Save to File
    if(strlen(app->post_state->url) > 0 && index == 4) {
        view_dispatcher_send_custom_event(app->view_dispatcher, PostItemAction);
        return;
    }

    bool url_found = url_in_csv(app, app->post_state->url, StateTypePost);
    FURI_LOG_D(TAG, "URL in CSV: %d", url_found);
    // Save to CSV / Delete from CSV
    if(strlen(app->post_state->url) > 0 && index == 5) {
        if(url_found) {
            view_dispatcher_send_custom_event(app->view_dispatcher, PostItemDeleteFromCsv);
        } else {
            view_dispatcher_send_custom_event(app->view_dispatcher, PostItemSaveToCsv);
        }
        return;
    }

    // Load from CSV
    if(app->post_url_list && app->post_url_list_count > 0 &&
       strlen(app->post_url_list[0].url) > 0) {
        if(!(strlen(app->post_state->url) > 0) && index == 4) {
            view_dispatcher_send_custom_event(app->view_dispatcher, PostItemLoadFromCsv);
        }
        FURI_LOG_D(TAG, "url in the list: %s", app->post_url_list[0].url);
        if(index == 6) {
            view_dispatcher_send_custom_event(app->view_dispatcher, PostItemLoadFromCsv);
        }
    }
}

static void post_scene_mode_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    furi_assert(app);
    app->post_state->mode = !app->post_state->mode;
    draw_post_menu(app);
}

static void post_scene_method_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    furi_assert(app);
    app->post_state->method = !app->post_state->method;
    draw_post_menu(app);
}

void draw_post_menu(App* app) {
    furi_assert(app);
    furi_assert(app->variable_item_list);
    VariableItemList* variable_item_list = app->variable_item_list;

    variable_item_list_reset(variable_item_list);

    VariableItem* item;

    item = variable_item_list_add(variable_item_list, "Mode", 2, post_scene_mode_callback, app);
    variable_item_set_current_value_text(item, app->post_state->mode ? "Save" : "Display");

    if(!app->post_state->mode) {
        item = variable_item_list_add(
            variable_item_list, "Method", 2, post_scene_method_callback, app);
        variable_item_set_current_value_text(item, app->post_state->method ? "Stream" : "Post");
    } else {
        item = variable_item_list_add(variable_item_list, "Method", 1, NULL, app);
        variable_item_set_current_value_text(item, "Stream to file");
    }

    item = variable_item_list_add(
        variable_item_list,
        strlen(app->post_state->url) > 0 ? "Edit Url" : "Set Url",
        0,
        NULL,
        app);

    item = variable_item_list_add(
        variable_item_list,
        furi_string_empty(app->post_state->payload) ? "Set Payload" : "Edit Payload",
        0,
        NULL,
        app);

    if(strlen(app->post_state->url) > 0) {
        item = variable_item_list_add(
            variable_item_list,
            app->post_state->mode ? "Save to File" : "Send Request",
            0,
            NULL,
            app);
    }

    bool url_found = url_in_csv(app, app->post_state->url, StateTypePost);
    if(strlen(app->post_state->url) > 0) {
        if(url_found) {
            item = variable_item_list_add(variable_item_list, "Delete from CSV", 0, NULL, app);
        } else {
            item = variable_item_list_add(variable_item_list, "Save to CSV", 0, NULL, app);
        }
    }

    if(app->post_url_list && app->post_url_list_count > 0 &&
       strlen(app->post_url_list[0].url) > 0) {
        item = variable_item_list_add(variable_item_list, "Load from CSV", 0, NULL, app);
    }
}

void scene_on_enter_post(void* context) {
    FURI_LOG_T(TAG, "scene_on_enter_post");
    App* app = context;
    furi_assert(app);
    furi_assert(app->variable_item_list);
    VariableItemList* variable_item_list = app->variable_item_list;

    if(!app->post_state) {
        FURI_LOG_D(TAG, "Allocating post state");
        app->post_state = malloc(sizeof(PostState));
        app->post_state->mode = false;
        app->post_state->method = false;
        strcpy(app->post_state->url, "");
        app->post_state->payload = furi_string_alloc();
        furi_string_set(app->post_state->payload, "");
    }

    variable_item_list_set_enter_callback(variable_item_list, post_scene_select_callback, app);
    draw_post_menu(app);
    variable_item_list_set_selected_item(variable_item_list, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_Post);
}

bool scene_on_event_post(void* context, SceneManagerEvent event) {
    FURI_LOG_T(TAG, "scene_on_event_post");
    App* app = context;
    furi_assert(app);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case PostItemSetUrl:
            if(strcmp(app->post_state->url, "") == 0) {
                strcpy(app->post_state->url, "https://");
            }
            app->selected_tx_string = app->post_state->url;
            app->text_input_state = TextInputState_PostUrl;
            scene_manager_next_scene(app->scene_manager, Text_Input);
            consumed = true;
            break;
        case PostItemSetPayload:
            app->selected_tx_string = furi_string_get_cstr(app->post_state->payload);
            app->text_input_state = TextInputState_Payload;
            scene_manager_next_scene(app->scene_manager, Text_Input);
            consumed = true;
            break;
        case PostItemSaveToCsv:
            FURI_LOG_D(TAG, "Save to CSV");
            PostUrlList post_entry = {0};
            strncpy(post_entry.url, app->post_state->url, TEXT_STORE_SIZE - 1);
            post_entry.url[TEXT_STORE_SIZE - 1] = '\0'; // Ensure null-termination
            post_entry.payload =
                furi_string_alloc_set(furi_string_get_cstr(app->post_state->payload));

            if(!write_post_url_to_csv(app, &post_entry)) {
                FURI_LOG_E(TAG, "Failed to write URL to CSV");
            }
            furi_string_free(post_entry.payload);
            sync_csv_post_url_to_mem(app);
            draw_post_menu(app);
            consumed = true;
            break;
        case PostItemDeleteFromCsv:
            if(!delete_post_url_from_csv(app, app->post_state->url)) {
                FURI_LOG_E(TAG, "Failed to delete URL from CSV");
            }
            sync_csv_post_url_to_mem(app);
            draw_post_menu(app);
            consumed = true;
            break;
        case PostItemLoadFromCsv:
            scene_manager_next_scene(app->scene_manager, Post_Url_List);
            consumed = true;
            break;
        case PostItemAction:
            FURI_LOG_D(TAG, "Action: Moving to display");
            if(app->post_state->mode) {
                // Set download mode for post
                app->download_mode = DOWNLOAD_POST;
                // If save to file mode, move to filename input
                app->text_input_state = TextInputState_Filename;
                scene_manager_next_scene(app->scene_manager, Text_Input);
            } else {
                app->display_mode = app->post_state->method ? DISPLAY_POST_STREAM : DISPLAY_POST;
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

void scene_on_exit_post(void* context) {
    FURI_LOG_T(TAG, "scene_on_exit_post");
    App* app = context;
    furi_assert(app);
    furi_assert(app->variable_item_list);
    variable_item_list_reset(app->variable_item_list);
}
