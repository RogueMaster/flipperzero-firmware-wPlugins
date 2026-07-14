#include "textInput.h"
#include "../main.h"
#include "../passwordStorage/passwordStorage.h"
#include <string.h>

static void credential_name_callback(void* context) {
    AppContext* app = context;

    strcpy(app->credentials[app->credentials_number].name, app->tmp_credential_name);

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewTextInputUsername);
}

static void username_callback(void* context) {
    AppContext* app = context;

    strcpy(app->credentials[app->credentials_number].username, app->tmp_username);

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewTextInputPassword);
}

static void password_callback(void* context) {
    AppContext* app = context;

    strcpy(app->credentials[app->credentials_number].password, app->tmp_password);

    // Add to file!!!
    write_password_to_file(
        PASS_SAVE_PATH, app->tmp_credential_name, app->tmp_username, app->tmp_password);
    strcpy(app->tmp_credential_name, "");
    strcpy(app->tmp_username, "");
    strcpy(app->tmp_password, "");

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewMainMenu);
}

TextInput* credential_name_TextInput_alloc(void* context) {
    AppContext* app = context;

    TextInput* textInput = text_input_alloc();
    text_input_set_header_text(textInput, "Insert Website:");
    text_input_set_result_callback(
        textInput,
        credential_name_callback, // Callback when user submits
        app, // Context passed to callback
        app->tmp_credential_name, // Buffer where text will be stored
        sizeof(char) * 99,
        true);

    return textInput;
}

TextInput* credential_username_TextInput_alloc(void* context) {
    AppContext* app = context;

    TextInput* textInput = text_input_alloc();
    text_input_set_minimum_length(textInput, 0);
    text_input_set_header_text(textInput, "Insert Username:");
    text_input_set_result_callback(
        textInput,
        username_callback, // Callback when user submits
        app, // Context passed to callback
        app->tmp_username, // Buffer where text will be stored
        sizeof(char) * 99,
        true);

    return textInput;
}

TextInput* credential_password_TextInput_alloc(void* context) {
    AppContext* app = context;

    TextInput* textInput = text_input_alloc();
    text_input_set_header_text(textInput, "Insert Password:");
    text_input_set_result_callback(
        textInput,
        password_callback, // Callback when user submits
        app, // Context passed to callback
        app->tmp_password, // Buffer where text will be stored
        sizeof(char) * 99,
        true);

    return textInput;
}
