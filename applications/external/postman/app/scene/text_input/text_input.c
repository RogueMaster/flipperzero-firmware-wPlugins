#include "../../app.h"

void uart_terminal_scene_text_input_callback(void* context) {
    App* app = context;

    switch(app->text_input_state) {
    case TextInputState_GetUrl:
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEvent_Get);
        break;
    case TextInputState_WifiPassword:
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEvent_Input_Text);
        break;
    case TextInputState_PostUrl:
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEvent_Post);
        break;
    case TextInputState_Filename:
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEvent_Set_Filename);
        break;
    case TextInputState_Payload:
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEvent_Payload);
        break;
    case TextInputState_BuildHttpUrl:
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEvent_Build_Http_Url);
        break;
    case TextInputState_BuildHttpPayload:
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEvent_Build_Http_Payload);
        break;
    case TextInputState_BuildHttpHeaderKey:
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEvent_Build_Http_Header_Key);
        break;
    case TextInputState_BuildHttpHeaderValue:
        view_dispatcher_send_custom_event(app->view_dispatcher, AppEvent_Build_Http_Header_Value);
        break;
    case TextInputState_Message:
        // Handle message input completion
        break;
    default:
        break;
    }
}

void scene_on_enter_text_input(void* context) {
    App* app = context;

    if(false == app->is_custom_tx_string) {
        // Fill text input with selected string so that user can add to it
        size_t length = strlen(app->selected_tx_string);
        furi_assert(length < UART_TERMINAL_TEXT_INPUT_STORE_SIZE);
        strncpy(app->text_input_store, app->selected_tx_string, length);

        // Add space - because flipper keyboard currently doesn't have a space
        // app->text_input_store[length] = ' ';
        app->text_input_store[length + 1] = '\0';
        app->is_custom_tx_string = true;
    }

    // Setup view
    UART_TextInput* text_input = app->text_input;
    // Add help message to header
    const char* header_text = "";
    switch(app->text_input_state) {
    case TextInputState_GetUrl:
        header_text = "Enter URL";
        break;
    case TextInputState_WifiPassword:
        header_text = "Enter Password";
        break;
    case TextInputState_PostUrl:
        header_text = "Enter Post URL";
        break;
    case TextInputState_BuildHttpUrl:
        header_text = "Enter URL";
        break;
    case TextInputState_Filename:
        header_text = "Enter Filename";
        break;
    case TextInputState_Payload:
        header_text = "Enter Payload";
        break;
    case TextInputState_BuildHttpPayload:
        header_text = "Enter Payload";
        break;
    case TextInputState_BuildHttpHeaderKey:
        header_text = "Enter Header Key";
        break;
    case TextInputState_BuildHttpHeaderValue:
        header_text = "Enter Header Value";
        break;
    case TextInputState_Message:
        header_text = "Enter Message";
        break;
    default:
        break;
    }
    uart_text_input_set_header_text(text_input, header_text);
    uart_text_input_set_result_callback(
        text_input,
        uart_terminal_scene_text_input_callback,
        app,
        app->text_input_store,
        UART_TERMINAL_TEXT_INPUT_STORE_SIZE,
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppView_Connect_Text_Input);
}

bool scene_on_event_text_input(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(app->text_input_state) {
        case TextInputState_WifiPassword:
            if(event.event == AppEvent_Input_Text) {
                // Handle SSID and password input completion
                app->selected_tx_string = app->text_input_store;
                strncpy(
                    app->wifi_list.password_ssid,
                    app->text_input_store,
                    sizeof(app->wifi_list.password_ssid) - 1);
                app->wifi_list.password_ssid[sizeof(app->wifi_list.password_ssid) - 1] =
                    '\0'; // Ensure null-termination
                consumed = true;
                scene_manager_previous_scene(app->scene_manager);
            }
            break;
        case TextInputState_GetUrl:
            if(event.event == AppEvent_Get) {
                // Handle URL input completion
                strncpy(
                    app->get_state->url, app->text_input_store, sizeof(app->get_state->url) - 1);
                app->get_state->url[sizeof(app->get_state->url) - 1] =
                    '\0'; // Ensure null-termination
                scene_manager_previous_scene(app->scene_manager);
                consumed = true;
            }
            break;
        case TextInputState_Filename:
            if(event.event == AppEvent_Set_Filename) {
                // Handle Filename input completion
                strncpy(app->filename, app->text_input_store, sizeof(app->filename) - 1);
                app->filename[sizeof(app->filename) - 1] = '\0'; // Ensure null-termination
                scene_manager_next_scene(app->scene_manager, Download);
                consumed = true;
            }
            break;
        case TextInputState_PostUrl:
            if(event.event == AppEvent_Post) {
                // Handle URL input completion
                strncpy(
                    app->post_state->url, app->text_input_store, sizeof(app->post_state->url) - 1);
                app->post_state->url[sizeof(app->post_state->url) - 1] =
                    '\0'; // Ensure null-termination
                scene_manager_previous_scene(app->scene_manager);
                consumed = true;
            }
            break;
        case TextInputState_Payload:
            if(event.event == AppEvent_Payload) {
                // Clear the existing content of the payload FuriString
                furi_string_reset(app->post_state->payload);
                // Set the new content from text_input_store
                furi_string_set_str(app->post_state->payload, app->text_input_store);
                scene_manager_previous_scene(app->scene_manager);
                consumed = true;
            }
            break;
        case TextInputState_BuildHttpUrl:
            if(event.event == AppEvent_Build_Http_Url) {
                // Handle URL input completion
                strncpy(
                    app->build_http_state->url,
                    app->text_input_store,
                    sizeof(app->build_http_state->url) - 1);
                app->build_http_state->url[sizeof(app->build_http_state->url) - 1] =
                    '\0'; // Ensure null-termination
                scene_manager_previous_scene(app->scene_manager);
                consumed = true;
            }
            break;
        case TextInputState_BuildHttpPayload:
            if(event.event == AppEvent_Build_Http_Payload) {
                // Clear the existing content of the payload FuriString
                furi_string_reset(app->build_http_state->payload);
                // Set the new content from text_input_store
                furi_string_set_str(app->build_http_state->payload, app->text_input_store);
                scene_manager_previous_scene(app->scene_manager);
                consumed = true;
            }
            break;
        case TextInputState_BuildHttpHeaderKey:
            if(event.event == AppEvent_Build_Http_Header_Key) {
                // Handle Header Key input completion,
                // Copy the key to the headers dynamic array of size_t headers_count
                // Allocate memory for headers
                if(app->build_http_state->headers_count == 0) {
                    app->build_http_state->headers = malloc(sizeof(HttpBuildHeader));
                    if(!app->build_http_state->headers) {
                        app->build_http_state->headers_count = 0;
                        return false;
                    }
                    memset(app->build_http_state->headers, 0, sizeof(HttpBuildHeader));
                } else {
                    app->build_http_state->headers = realloc(
                        app->build_http_state->headers,
                        (app->build_http_state->headers_count + 1) * sizeof(HttpBuildHeader));
                    if(!app->build_http_state->headers) {
                        app->build_http_state->headers_count = 0;
                        return false;
                    }
                }

                strncpy(
                    app->build_http_state->headers[app->build_http_state->headers_count].key,
                    app->text_input_store,
                    sizeof(
                        app->build_http_state->headers[app->build_http_state->headers_count].key) -
                        1);
                app->build_http_state->headers[app->build_http_state->headers_count].key
                    [sizeof(
                         app->build_http_state->headers[app->build_http_state->headers_count].key) -
                     1] = '\0'; // Ensure null-termination

                // Initialize the value to an empty string
                app->build_http_state->headers[app->build_http_state->headers_count].value[0] =
                    '\0';

                // Switch to Header Value input
                app->text_input_state = TextInputState_BuildHttpHeaderValue;
                scene_manager_next_scene(app->scene_manager, Text_Input);
                consumed = true;
            }
            break;

        case TextInputState_BuildHttpHeaderValue:
            if(event.event == AppEvent_Build_Http_Header_Value) {
                // Handle Header Value input completion
                strncpy(
                    app->build_http_state->headers[app->build_http_state->headers_count].value,
                    app->text_input_store,
                    sizeof(app->build_http_state->headers[app->build_http_state->headers_count]
                               .value) -
                        1);
                app->build_http_state->headers[app->build_http_state->headers_count].value
                    [sizeof(app->build_http_state->headers[app->build_http_state->headers_count]
                                .value) -
                     1] = '\0'; // Ensure null-termination

                // Increment headers_count
                app->build_http_state->headers_count++;

                // Switch back to Header Key input
                app->text_input_state = TextInputState_BuildHttpHeaderKey;
                scene_manager_next_scene(app->scene_manager, Build_Http_Headers);
                consumed = true;
            }
            break;
        default:
            break;
        }
    }

    return consumed;
}

void scene_on_exit_text_input(void* context) {
    App* app = context;
    app->is_custom_tx_string = false;
    app->text_input_store[0] = '\0';
    // Set selected_tx_string to point to an empty string
    app->selected_tx_string = "";
    uart_text_input_reset(app->text_input);
}
