#include "../../app.h"
#include "../../structs.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/modules/widget.h>
#include "../../utils/response/response.h"

typedef void (*DisplayCallback)(App* app, const char* header);

void get_the_header(App* app, FuriString* header) {
    DisplayMode mode = app->display_mode;
    GetState* get_state = app->get_state;
    PostState* post_state = app->post_state;
    BuildHttpState* build_state = app->build_http_state;
    furi_string_reset(header);
    switch(mode) {
    case DISPLAY_GET:
        furi_string_printf(header, "Getting: %s", get_state->url);
        break;
    case DISPLAY_GET_STREAM:
        furi_string_printf(header, "Getting Stream: %s", get_state->url);
        break;
    case DISPLAY_POST:
        furi_string_printf(header, "Posting: %s", post_state->url);
        break;
    case DISPLAY_POST_STREAM:
        furi_string_printf(header, "Posting: %s", post_state->url);
        break;
    case DISPLAY_BUILD_HTTP:
        furi_string_printf(header, "Building HTTP: %s", build_state->url);
        break;
    case DISPLAY_DOWNLOAD:
        furi_string_printf(header, "Downloading: %s", get_state->url);
        break;
    case DISPLAY_LISTEN:
        furi_string_printf(header, "Listening: %s", get_state->url);
        break;
    default:
        furi_string_printf(header, "Unknown: %s", get_state->url);
        break;
    }
}

static void append_to_status_line(FuriString* status_line, const char* append_str) {
    furi_string_cat(status_line, append_str);
}
typedef enum {
    METHOD_GET,
    METHOD_GET_STREAM,
    METHOD_POST,
    METHOD_POST_STREAM,
    METHOD_CUSTOM
} HttpDisplayMethod;

bool sendHttpRequest(App* app, HttpDisplayMethod method, const char* url, FuriString* payload) {
    char command[512];
    const char* method_str;

    switch(method) {
    case METHOD_GET:
        method_str = "GET";
        snprintf(command, sizeof(command), "%s %s\n", method_str, url);
        break;
    case METHOD_GET_STREAM:
        method_str = "GET_STREAM";
        snprintf(command, sizeof(command), "%s %s\n", method_str, url);
        break;
    case METHOD_POST:
        method_str = "POST";
        snprintf(
            command, sizeof(command), "%s %s %s\n", method_str, url, furi_string_get_cstr(payload));
        break;
    case METHOD_POST_STREAM:
        method_str = "POST";
        snprintf(
            command, sizeof(command), "%s %s %s\n", method_str, url, furi_string_get_cstr(payload));
        break;
    case METHOD_CUSTOM:
        method_str = "EXECUTE_HTTP_CALL";
        snprintf(command, sizeof(command), "%s \n", method_str);
        break;
    default:
        FURI_LOG_E(TAG, "Unknown HTTP method");
        return false;
    }

    app->full_response = true;

    // Send the command to the UART
    if(!uart_terminal_uart_tx(app->uart, (uint8_t*)command, strlen(command))) {
        return false;
    }

    // Wait for the response
    uint32_t events = furi_thread_flags_wait(WorkerEvtRxDone, FuriFlagWaitAny, 6000);
    if(events & WorkerEvtRxDone) {
        if(app->full_response) {
            // Check if text_box_store is empty or null
            if(furi_string_size(app->text_box_store) == 0) {
                // Input error message to text_box_store
                char error_message[100];
                snprintf(
                    error_message,
                    sizeof(error_message),
                    "%s_ERROR: Failed to send %s request",
                    method_str,
                    method_str);
                furi_string_set_str(app->text_box_store, error_message);
            }
        }
    } else {
        FURI_LOG_E("UART", "No response received from the board.");
        return false;
    }

    // Check if there is a response otherwise input error message to text_box_store
    if(furi_string_size(app->text_box_store) == 0) {
        FURI_LOG_E(TAG, "%s_ERROR: Failed to send %s request", method_str, method_str);
        char error_message[100];
        snprintf(
            error_message,
            sizeof(error_message),
            "%s_ERROR: Failed to send %s request",
            method_str,
            method_str);
        furi_string_set_str(app->text_box_store, error_message);
        return false;
    }

    return true;
}

void processHttpResponse(App* app, HttpDisplayMethod method) {
    FuriString* status_line = furi_string_alloc();
    if(extract_status_line(app, status_line)) {
        FURI_LOG_I(TAG, "Status line: %s", furi_string_get_cstr(status_line));
    } else {
        furi_string_set_str(status_line, "Unknown status");
    }

    clear_new_lines(app);

    if(method == METHOD_GET_STREAM || method == METHOD_POST_STREAM) {
        append_to_status_line(status_line, " (Direct)");
    } else {
        if(extract_response_text(app, "RESPONSE: ", " RESPONSE_END")) {
            FURI_LOG_I(TAG, "Response text extracted");
        } else {
            FURI_LOG_E(TAG, "Failed to extract response text");
            if(extract_response_text(app, "STREAM: ", " STREAM_END")) {
                FURI_LOG_I(TAG, "Stream extracted");
            } else {
                FURI_LOG_E(TAG, "Failed to extract stream");
            }
        }

        bool isJson = is_json_response(app);
        FURI_LOG_D("POSTMAN", "isJson: %d", isJson);

        if(isJson) {
            FuriString* pretty_json = furi_string_alloc();
            if(prettify_json(app, pretty_json)) {
                furi_string_set(app->text_box_store, pretty_json);
                append_to_status_line(status_line, " (JSON)");
            } else {
                FURI_LOG_W(TAG, "JSON prettification failed");
            }
            furi_string_free(pretty_json);
        }
    }

    widget_reset(app->text_box);
    widget_add_string_element(
        app->text_box, 0, 0, AlignLeft, AlignTop, FontPrimary, furi_string_get_cstr(status_line));

    furi_string_free(status_line);
}

void scene_on_enter_display(void* context) {
    App* app = context;
    furi_assert(app);
    FuriString* header = furi_string_alloc();

    widget_reset(app->text_box);
    get_the_header(app, header);

    widget_add_string_element(
        app->text_box, 0, 0, AlignLeft, AlignTop, FontPrimary, furi_string_get_cstr(header));

    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_Display);

    HttpDisplayMethod method;
    bool success = false;

    switch(app->display_mode) {
    case DISPLAY_GET_STREAM:
        method = METHOD_GET_STREAM;
        success = sendHttpRequest(app, method, app->get_state->url, NULL);
        break;
    case DISPLAY_GET:
        method = METHOD_GET;
        furi_string_reset(app->text_box_store);
        success = sendHttpRequest(app, method, app->get_state->url, NULL);
        break;
    case DISPLAY_POST:
        method = METHOD_POST;
        success = sendHttpRequest(app, method, app->post_state->url, app->post_state->payload);
        break;
    case DISPLAY_POST_STREAM:
        method = METHOD_POST_STREAM;
        success = sendHttpRequest(app, method, app->post_state->url, app->post_state->payload);
        break;
    case DISPLAY_BUILD_HTTP:
        method = METHOD_CUSTOM;
        success = sendHttpRequest(app, method, NULL, NULL);
        break;
    default:
        FURI_LOG_E(TAG, "Unknown display mode");
        break;
    }
    if(success) {
        processHttpResponse(app, method);
    } else {
        furi_string_set_str(app->text_box_store, "HTTP Request failed");
        FURI_LOG_I(TAG, "HTTP Request failed");
    }

    widget_add_text_scroll_element(
        app->text_box, 0, 14, 128, 48, furi_string_get_cstr(app->text_box_store));

    furi_string_free(header);
}

void scene_on_exit_display(void* context) {
    App* app = context;
    furi_assert(app);
    FURI_LOG_D(TAG, "scene_on_exit_display");
    widget_reset(app->text_box);
    // Reset the text_box_store

    app->filename[0] = '\0';
    furi_string_reset(app->text_box_store);
    app->display_mode = DISPLAY_NONE;
}

bool scene_on_event_display(void* context, SceneManagerEvent event) {
    FURI_LOG_T(TAG, "scene_on_event_display");
    App* app = context;
    furi_assert(app);
    furi_assert(event);
    bool consumed = false;
    switch(event.type) {
    case(SceneManagerEventTypeBack):
        if(app->display_mode == DISPLAY_DOWNLOAD) {
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, Get);
            consumed = true;
        }
        break;
    default:
        consumed = false;
        break;
    }

    return consumed;
}
