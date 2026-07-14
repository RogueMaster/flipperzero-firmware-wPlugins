#include "../../app.h"
#include "../../structs.h"
#include "../../utils/trimwhitespace/trimwhitespace.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/modules/submenu.h>

void submenu_callback_select_build_http_url(void* context, uint32_t index) {
    FURI_LOG_T(TAG, "submenu_callback_select_build_http_url");
    furi_assert(context);
    App* app = context;

    // Ensure build_http_state is initialized
    if(!app->build_http_state) {
        app->build_http_state = malloc(sizeof(BuildHttpState));
        if(!app->build_http_state) {
            FURI_LOG_E(TAG, "Failed to allocate build_http_state");
            return;
        }
        app->build_http_state->mode = false;
        app->build_http_state->http_method = GET;
        app->build_http_state->show_response_headers = false;
        strcpy(app->build_http_state->url, "");
        app->build_http_state->payload = furi_string_alloc();
        app->build_http_state->headers =
            malloc(sizeof(HttpBuildHeader)); // Allocate a small amount of memory
        app->build_http_state->headers_count = 0;
    }

    // Copy the entire entry from build_http_list to build_http_state
    strncpy(
        app->build_http_state->url,
        app->build_http_list[index].url,
        sizeof(app->build_http_state->url) - 1);
    app->build_http_state->url[sizeof(app->build_http_state->url) - 1] =
        '\0'; // Ensure null-termination

    app->build_http_state->mode = app->build_http_list[index].mode;
    app->build_http_state->http_method = app->build_http_list[index].http_method;

    // Free existing headers in build_http_state
    if(app->build_http_state->headers) {
        free(app->build_http_state->headers);
        app->build_http_state->headers =
            malloc(sizeof(HttpBuildHeader)); // Allocate a small amount of memory
        app->build_http_state->headers_count = 0;
    }

    // Allocate memory for headers if they exist
    size_t headers_count = app->build_http_list[index].headers_count;
    FURI_LOG_D(TAG, "Headers count: %zu", headers_count);
    if(headers_count > 0) {
        app->build_http_state->headers = malloc(headers_count * sizeof(HttpBuildHeader));
        if(!app->build_http_state->headers) {
            FURI_LOG_E(TAG, "Failed to allocate memory for headers");
            return;
        }
        app->build_http_state->headers_count = headers_count;

        // Copy headers
        for(size_t i = 0; i < headers_count; i++) {
            strncpy(
                app->build_http_state->headers[i].key,
                app->build_http_list[index].headers[i].key,
                TEXT_STORE_SIZE - 1);
            app->build_http_state->headers[i].key[TEXT_STORE_SIZE - 1] = '\0';
            strncpy(
                app->build_http_state->headers[i].value,
                app->build_http_list[index].headers[i].value,
                TEXT_STORE_SIZE - 1);
            app->build_http_state->headers[i].value[TEXT_STORE_SIZE - 1] = '\0';
        }
    } else {
        app->build_http_state->headers =
            malloc(sizeof(HttpBuildHeader)); // Allocate a small amount of memory
        app->build_http_state->headers_count = 0;
    }

    // Copy payload if it exists
    if(app->build_http_list[index].payload &&
       furi_string_size(app->build_http_list[index].payload) > 0) {
        furi_string_set(app->build_http_state->payload, app->build_http_list[index].payload);
    } else {
        furi_string_reset(app->build_http_state->payload);
    }
    FURI_LOG_D(TAG, "Payload: %s", furi_string_get_cstr(app->build_http_list[index].payload));

    app->build_http_state->show_response_headers =
        app->build_http_list[index].show_response_headers;

    // Handle the custom event to move to the Build HTTP details scene
    scene_manager_handle_custom_event(app->scene_manager, AppEvent_Build_Http_Url_List);
}

void submenu_callback_no_build_http_url(void* context, uint32_t index) {
    FURI_LOG_T(TAG, "submenu_callback_no_build_http_url");
    furi_assert(index);
    App* app = context;
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, Build_Http);
}

void scene_on_enter_build_http_list(void* context) {
    FURI_LOG_T(TAG, "scene_on_enter_build_http_url_list");
    App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Build HTTP URLs");
    submenu_add_item(app->submenu, "Loading ...", 0, NULL, NULL);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_BuildHttp_Url_List);

    // Clear the submenu and add the actual URLs
    if(app->build_http_list_size > 0 && app->build_http_list[0].url[0] != '\0') {
        submenu_reset(app->submenu);
        submenu_set_header(app->submenu, "Build HTTP URLs");

        // Add URLs to submenu
        for(size_t i = 0; i < app->build_http_list_size; i++) {
            if(app->build_http_list[i].url[0] != '\0') {
                char display_name[TEXT_STORE_SIZE];

                // Trim URLs
                trim_whitespace(app->build_http_list[i].url);

                snprintf(display_name, sizeof(display_name), "%s", app->build_http_list[i].url);
                submenu_add_item(
                    app->submenu, display_name, i, submenu_callback_select_build_http_url, app);
            }
        }
    } else {
        submenu_reset(app->submenu);
        submenu_add_item(
            app->submenu, "No Build HTTP URLs found", 0, submenu_callback_no_build_http_url, app);
    }
}
bool scene_on_event_build_http_list(void* context, SceneManagerEvent event) {
    FURI_LOG_T(TAG, "scene_on_event_build_http_url_list");
    App* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case AppEvent_Build_Http_Url_List:
            // Add log before switching to Build HTTP details
            FURI_LOG_I(TAG, "Switching to Build HTTP details");
            scene_manager_next_scene(app->scene_manager, Build_Http);
            consumed = true;
            break;
        }
        break;

    default: // eg. SceneManagerEventTypeBack, SceneManagerEventTypeTick
        consumed = false;
        break;
    }
    return consumed;
}

void scene_on_exit_build_http_list(void* context) {
    FURI_LOG_T(TAG, "scene_on_exit_build_http_url_list");
    App* app = context;

    submenu_reset(app->submenu);
}
