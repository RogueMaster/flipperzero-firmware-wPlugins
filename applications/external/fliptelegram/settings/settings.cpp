#include "settings.hpp"
#include "app.hpp"

FlipTelegramSettings::FlipTelegramSettings(ViewDispatcher** view_dispatcher, void* appContext)
    : appContext(appContext)
    , view_dispatcher_ref(view_dispatcher) {
    if(!easy_flipper_set_variable_item_list(
           &variable_item_list,
           FlipTelegramViewSettings,
           settingsItemSelectedCallback,
           callbackToSubmenu,
           view_dispatcher,
           this)) {
        return;
    }

    variable_item_wifi_ssid =
        variable_item_list_add(variable_item_list, "WiFi SSID", 1, nullptr, nullptr);
    variable_item_wifi_pass =
        variable_item_list_add(variable_item_list, "WiFi Password", 1, nullptr, nullptr);
    variable_item_connect =
        variable_item_list_add(variable_item_list, "Connect To WiFi", 1, nullptr, nullptr);
    variable_item_token =
        variable_item_list_add(variable_item_list, "Bot Token", 1, nullptr, nullptr);
    variable_item_chat_id =
        variable_item_list_add(variable_item_list, "Chat ID", 1, nullptr, nullptr);

    char loaded_ssid[64];
    char loaded_pass[64];
    char loaded_token[128];
    char loaded_chat_id[64];
    FlipTelegramApp* app = static_cast<FlipTelegramApp*>(appContext);
    if(app->loadChar("wifi_ssid", loaded_ssid, sizeof(loaded_ssid), "flipper_http")) {
        variable_item_set_current_value_text(variable_item_wifi_ssid, loaded_ssid);
    } else {
        variable_item_set_current_value_text(variable_item_wifi_ssid, "");
    }
    if(app->loadChar("wifi_pass", loaded_pass, sizeof(loaded_pass), "flipper_http")) {
        variable_item_set_current_value_text(variable_item_wifi_pass, "*****");
    } else {
        variable_item_set_current_value_text(variable_item_wifi_pass, "");
    }
    variable_item_set_current_value_text(variable_item_connect, "");
    //
    if(app->loadChar("token", loaded_token, sizeof(loaded_token))) {
        variable_item_set_current_value_text(variable_item_token, "*****");
    } else {
        variable_item_set_current_value_text(variable_item_token, "");
    }
    if(app->loadChar("chat_id", loaded_chat_id, sizeof(loaded_chat_id))) {
        variable_item_set_current_value_text(variable_item_chat_id, "*****");
    } else {
        variable_item_set_current_value_text(variable_item_chat_id, "");
    }
}

FlipTelegramSettings::~FlipTelegramSettings() {
    // Free text input first
    freeTextInput();

    if(variable_item_list && view_dispatcher_ref && *view_dispatcher_ref) {
        view_dispatcher_remove_view(*view_dispatcher_ref, FlipTelegramViewSettings);
        variable_item_list_free(variable_item_list);
        variable_item_list = nullptr;
        variable_item_wifi_ssid = nullptr;
        variable_item_wifi_pass = nullptr;
        variable_item_connect = nullptr;
        variable_item_token = nullptr;
        variable_item_chat_id = nullptr;
    }
}

uint32_t FlipTelegramSettings::callbackToSettings(void* context) {
    UNUSED(context);
    return FlipTelegramViewSettings;
}

uint32_t FlipTelegramSettings::callbackToSubmenu(void* context) {
    UNUSED(context);
    return FlipTelegramViewSubmenu;
}

void FlipTelegramSettings::freeTextInput() {
    if(text_input && view_dispatcher_ref && *view_dispatcher_ref) {
        view_dispatcher_remove_view(*view_dispatcher_ref, FlipTelegramViewTextInput);
#ifndef FW_ORIGIN_Momentum
        uart_text_input_free(text_input);
#else
        text_input_free(text_input);
#endif
        text_input = nullptr;
    }
    text_input_buffer.reset();
    text_input_temp_buffer.reset();
}

bool FlipTelegramSettings::initTextInput(uint32_t view) {
    // check if already initialized
    if(text_input_buffer || text_input_temp_buffer) {
        FURI_LOG_E(TAG, "initTextInput: already initialized");
        return false;
    }

    // init buffers
    text_input_buffer_size = 128;
    if(!easy_flipper_set_buffer(
           reinterpret_cast<char**>(&text_input_buffer), text_input_buffer_size)) {
        return false;
    }
    if(!easy_flipper_set_buffer(
           reinterpret_cast<char**>(&text_input_temp_buffer), text_input_buffer_size)) {
        return false;
    }

    // app context
    FlipTelegramApp* app = static_cast<FlipTelegramApp*>(appContext);
    char loaded[256];

    if(view == SettingsViewSSID) {
        if(app->loadChar("wifi_ssid", loaded, sizeof(loaded), "flipper_http")) {
            strncpy(text_input_temp_buffer.get(), loaded, text_input_buffer_size);
        } else {
            text_input_temp_buffer[0] = '\0'; // Ensure empty if not loaded
        }
        text_input_temp_buffer[text_input_buffer_size - 1] = '\0'; // Ensure null-termination
#ifndef FW_ORIGIN_Momentum
        return easy_flipper_set_uart_text_input(
            &text_input,
            FlipTelegramViewTextInput,
            "Enter SSID",
            text_input_temp_buffer.get(),
            text_input_buffer_size,
            textUpdatedSsidCallback,
            callbackToSettings,
            view_dispatcher_ref,
            this);
#else
        return easy_flipper_set_text_input(
            &text_input,
            FlipTelegramViewTextInput,
            "Enter SSID",
            text_input_temp_buffer.get(),
            text_input_buffer_size,
            textUpdatedSsidCallback,
            callbackToSettings,
            view_dispatcher_ref,
            this);
#endif
    } else if(view == SettingsViewPassword) {
        if(app->loadChar("wifi_pass", loaded, sizeof(loaded), "flipper_http")) {
            strncpy(text_input_temp_buffer.get(), loaded, text_input_buffer_size);
        } else {
            text_input_temp_buffer[0] = '\0'; // Ensure empty if not loaded
        }
        text_input_temp_buffer[text_input_buffer_size - 1] = '\0'; // Ensure null-termination
#ifndef FW_ORIGIN_Momentum
        return easy_flipper_set_uart_text_input(
            &text_input,
            FlipTelegramViewTextInput,
            "Enter Password",
            text_input_temp_buffer.get(),
            text_input_buffer_size,
            textUpdatedPassCallback,
            callbackToSettings,
            view_dispatcher_ref,
            this);
#else
        return easy_flipper_set_text_input(
            &text_input,
            FlipTelegramViewTextInput,
            "Enter Password",
            text_input_temp_buffer.get(),
            text_input_buffer_size,
            textUpdatedPassCallback,
            callbackToSettings,
            view_dispatcher_ref,
            this);
#endif
    } else if(view == SettingsViewToken) {
        if(app->loadChar("token", loaded, sizeof(loaded))) {
            strncpy(text_input_temp_buffer.get(), loaded, text_input_buffer_size);
        } else {
            text_input_temp_buffer[0] = '\0'; // Ensure empty if not loaded
        }
        text_input_temp_buffer[text_input_buffer_size - 1] = '\0'; // Ensure null-termination
#ifndef FW_ORIGIN_Momentum
        return easy_flipper_set_uart_text_input(
            &text_input,
            FlipTelegramViewTextInput,
            "Enter Bot Token",
            text_input_temp_buffer.get(),
            text_input_buffer_size,
            textUpdatedTokenCallback,
            callbackToSettings,
            view_dispatcher_ref,
            this);
#else
        return easy_flipper_set_text_input(
            &text_input,
            FlipTelegramViewTextInput,
            "Enter Bot Token",
            text_input_temp_buffer.get(),
            text_input_buffer_size,
            textUpdatedTokenCallback,
            callbackToSettings,
            view_dispatcher_ref,
            this);
#endif
    } else if(view == SettingsViewChatID) {
        if(app->loadChar("chat_id", loaded, sizeof(loaded))) {
            strncpy(text_input_temp_buffer.get(), loaded, text_input_buffer_size);
        } else {
            text_input_temp_buffer[0] = '\0'; // Ensure empty if not loaded
        }
        text_input_temp_buffer[text_input_buffer_size - 1] = '\0'; // Ensure null-termination
#ifndef FW_ORIGIN_Momentum
        return easy_flipper_set_uart_text_input(
            &text_input,
            FlipTelegramViewTextInput,
            "Enter Chat ID",
            text_input_temp_buffer.get(),
            text_input_buffer_size,
            textUpdatedChatIDCallback,
            callbackToSettings,
            view_dispatcher_ref,
            this);
#else
        return easy_flipper_set_text_input(
            &text_input,
            FlipTelegramViewTextInput,
            "Enter Chat ID",
            text_input_temp_buffer.get(),
            text_input_buffer_size,
            textUpdatedChatIDCallback,
            callbackToSettings,
            view_dispatcher_ref,
            this);
#endif
    }
    return false;
}

void FlipTelegramSettings::settingsItemSelected(uint32_t index) {
    switch(index) {
    case SettingsViewSSID:
    case SettingsViewPassword:
    case SettingsViewToken:
    case SettingsViewChatID:
        startTextInput(index);
        break;
    case SettingsViewConnect: {
        FlipTelegramApp* app = static_cast<FlipTelegramApp*>(appContext);
        char loaded_ssid[64];
        char loaded_pass[64];
        if(!app->loadChar("wifi_ssid", loaded_ssid, sizeof(loaded_ssid), "flipper_http") ||
           !app->loadChar("wifi_pass", loaded_pass, sizeof(loaded_pass), "flipper_http")) {
            FURI_LOG_E(TAG, "WiFi credentials not set");
            easy_flipper_dialog(
                "No WiFi Credentials", "Please set your WiFi SSID\nand Password in Settings.");
        } else {
            app->sendWiFiCredentials(loaded_ssid, loaded_pass);
        }
    } break;
    default:
        break;
    };
}

void FlipTelegramSettings::settingsItemSelectedCallback(void* context, uint32_t index) {
    FlipTelegramSettings* settings = (FlipTelegramSettings*)context;
    settings->settingsItemSelected(index);
}

bool FlipTelegramSettings::startTextInput(uint32_t view) {
    freeTextInput();
    if(!initTextInput(view)) {
        FURI_LOG_E(TAG, "Failed to initialize text input for view %lu", view);
        return false;
    }
    if(view_dispatcher_ref && *view_dispatcher_ref) {
        view_dispatcher_switch_to_view(*view_dispatcher_ref, FlipTelegramViewTextInput);
        return true;
    } else {
        FURI_LOG_E(TAG, "View dispatcher reference is null or invalid");
        return false;
    }
}

void FlipTelegramSettings::textUpdated(uint32_t view) {
    // store the entered text
    strncpy(text_input_buffer.get(), text_input_temp_buffer.get(), text_input_buffer_size);

    // Ensure null-termination
    text_input_buffer[text_input_buffer_size - 1] = '\0';

    // app context
    FlipTelegramApp* app = static_cast<FlipTelegramApp*>(appContext);

    switch(view) {
    case SettingsViewSSID:
        if(variable_item_wifi_ssid) {
            variable_item_set_current_value_text(variable_item_wifi_ssid, text_input_buffer.get());
        }
        app->saveChar("wifi_ssid", text_input_buffer.get(), "flipper_http");
        break;
    case SettingsViewPassword:
        if(variable_item_wifi_pass) {
            variable_item_set_current_value_text(variable_item_wifi_pass, text_input_buffer.get());
        }
        app->saveChar("wifi_pass", text_input_buffer.get(), "flipper_http");
        break;
    case SettingsViewToken:
        if(variable_item_token) {
            variable_item_set_current_value_text(variable_item_token, text_input_buffer.get());
        }
        app->saveChar("token", text_input_buffer.get());
        break;
    case SettingsViewChatID:
        if(variable_item_chat_id) {
            variable_item_set_current_value_text(variable_item_chat_id, text_input_buffer.get());
        }
        app->saveChar("chat_id", text_input_buffer.get());
        break;
    default:
        break;
    }

    // switch to the settings view
    if(view_dispatcher_ref && *view_dispatcher_ref) {
        view_dispatcher_switch_to_view(*view_dispatcher_ref, FlipTelegramViewSettings);
    }
}

void FlipTelegramSettings::textUpdatedSsidCallback(void* context) {
    FlipTelegramSettings* settings = (FlipTelegramSettings*)context;
    settings->textUpdated(SettingsViewSSID);
}

void FlipTelegramSettings::textUpdatedPassCallback(void* context) {
    FlipTelegramSettings* settings = (FlipTelegramSettings*)context;
    settings->textUpdated(SettingsViewPassword);
}

void FlipTelegramSettings::textUpdatedTokenCallback(void* context) {
    FlipTelegramSettings* settings = (FlipTelegramSettings*)context;
    settings->textUpdated(SettingsViewToken);
}

void FlipTelegramSettings::textUpdatedChatIDCallback(void* context) {
    FlipTelegramSettings* settings = (FlipTelegramSettings*)context;
    settings->textUpdated(SettingsViewChatID);
}
