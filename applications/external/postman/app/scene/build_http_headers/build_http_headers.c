#include "../../app.h"
#include "./build_http_headers.h"
#include <gui/modules/variable_item_list.h>

static void build_http_headers_add_callback(void* context) {
    App* app = context;
    furi_assert(app);
    app->text_input_state = TextInputState_BuildHttpHeaderKey;
    app->selected_tx_string = "";
    scene_manager_next_scene(app->scene_manager, Text_Input);
}

static void build_http_headers_reset_callback(void* context) {
    App* app = context;
    furi_assert(app);
    // Reset headers
    if(app->build_http_state->headers) {
        free(app->build_http_state->headers);
        app->build_http_state->headers = NULL;
        app->build_http_state->headers_count = 0;
    }
    draw_build_http_headers_menu(app);
}

static void build_http_headers_select_callback(void* context, uint32_t index) {
    App* app = context;
    furi_assert(app);

    // Headers will push down the add header button and reset button
    // Add header index will always be site_t headers_count except when headers_count is 0
    // Reset button will always be site_t headers_count + 1 only if headers_count is not 0

    bool headers_empty = app->build_http_state->headers_count == 0;

    if(headers_empty) {
        if(index == 0) {
            build_http_headers_add_callback(app);
            return;
        }
    }

    if(!headers_empty) {
        // Edit each header
        if(index < app->build_http_state->headers_count) {
            app->text_input_state = TextInputState_BuildHttpHeaderValue;
            app->selected_tx_string = app->build_http_state->headers[index].key;
            scene_manager_next_scene(app->scene_manager, Text_Input);
            return;
        }

        // Add header
        if(index == app->build_http_state->headers_count) {
            build_http_headers_add_callback(app);
            return;
        }

        // Reset headers
        if(index == app->build_http_state->headers_count + 1) {
            build_http_headers_reset_callback(app);
            return;
        }
    }
}

void draw_build_http_headers_menu(App* app) {
    furi_assert(app);
    furi_assert(app->variable_item_list);
    VariableItemList* variable_item_list = app->variable_item_list;
    bool headers_empty = app->build_http_state->headers_count == 0;

    variable_item_list_reset(variable_item_list);

    if(!headers_empty) {
        // Display headers
        for(size_t i = 0; i < app->build_http_state->headers_count; i++) {
            VariableItem* item = variable_item_list_add(
                variable_item_list, app->build_http_state->headers[i].key, i, NULL, app);
            variable_item_set_current_value_text(item, app->build_http_state->headers[i].value);
        }
    }

    variable_item_list_add(variable_item_list, "Add Header", 0, NULL, app);

    if(!headers_empty) {
        variable_item_list_add(
            variable_item_list, "Reset Headers", app->build_http_state->headers_count, NULL, app);
    }
    variable_item_list_set_enter_callback(
        variable_item_list, build_http_headers_select_callback, app);
}

void scene_on_enter_build_http_headers(void* context) {
    App* app = context;
    furi_assert(app);
    draw_build_http_headers_menu(app);
    variable_item_list_set_selected_item(app->variable_item_list, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_BuildHttp_Headers);
}

void scene_on_exit_build_http_headers(void* context) {
    App* app = context;
    furi_assert(app);
    variable_item_list_reset(app->variable_item_list);
}

bool scene_on_event_build_http_headers(void* context, SceneManagerEvent event) {
    App* app = context;
    furi_assert(app);
    bool consumed = false;
    switch(event.type) {
    case(SceneManagerEventTypeBack):
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, Build_Http);
        consumed = true;
        break;
    default:
        consumed = false;
        break;
    }
    return consumed;
}
