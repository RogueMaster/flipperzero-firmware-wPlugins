#include "settings.hpp"
#include "app.hpp"

FlipGeminiSettings::FlipGeminiSettings(ViewDispatcher** view_dispatcher, void* appContext)
    : appContext(appContext)
    , view_dispatcher_ref(view_dispatcher) {
    if(!easy_flipper_set_variable_item_list(
           &variable_item_list,
           FlipGeminiViewSettings,
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
        variable_item_list_add(variable_item_list, "Connect", 1, nullptr, nullptr);
    variable_item_api_key =
        variable_item_list_add(variable_item_list, "API Key", 1, nullptr, nullptr);

    char loaded_ssid[64];
    char loaded_pass[64];
    char loaded_api_key[128];
    FlipGeminiApp* app = static_cast<FlipGeminiApp*>(appContext);
    if(app->loadChar("wifi_ssid", loaded_ssid, sizeof(loaded_ssid)), "flipper_http") {
        variable_item_set_current_value_text(variable_item_wifi_ssid, loaded_ssid);
    } else {
        variable_item_set_current_value_text(variable_item_wifi_ssid, "");
    }
    if(app->loadChar("wifi_pass", loaded_pass, sizeof(loaded_pass)), "flipper_http") {
        variable_item_set_current_value_text(variable_item_wifi_pass, "*****");
    } else {
        variable_item_set_current_value_text(variable_item_wifi_pass, "");
    }
    variable_item_set_current_value_text(variable_item_connect, "");
    if(app->loadChar("api_key", loaded_api_key, sizeof(loaded_api_key))) {
        variable_item_set_current_value_text(variable_item_api_key, "*****");
    } else {
        variable_item_set_current_value_text(variable_item_api_key, "");
    }
}

FlipGeminiSettings::~FlipGeminiSettings() {
    // Free text input first
    freeTextInput();

    if(variable_item_list && view_dispatcher_ref && *view_dispatcher_ref) {
        view_dispatcher_remove_view(*view_dispatcher_ref, FlipGeminiViewSettings);
        variable_item_list_free(variable_item_list);
        variable_item_list = nullptr;
        variable_item_api_key = nullptr;
        variable_item_connect = nullptr;
        variable_item_wifi_pass = nullptr;
        variable_item_wifi_ssid = nullptr;
    }
}

uint32_t FlipGeminiSettings::callbackToSettings(void* context) {
    UNUSED(context);
    return FlipGeminiViewSettings;
}

uint32_t FlipGeminiSettings::callbackToSubmenu(void* context) {
    UNUSED(context);
    return FlipGeminiViewSubmenu;
}

void FlipGeminiSettings::freeTextInput() {
    if(text_input && view_dispatcher_ref && *view_dispatcher_ref) {
        view_dispatcher_remove_view(*view_dispatcher_ref, FlipGeminiViewTextInput);
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

bool FlipGeminiSettings::initTextInput(uint32_t view) {
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
    FlipGeminiApp* app = static_cast<FlipGeminiApp*>(appContext);
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
            FlipGeminiViewTextInput,
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
            FlipGeminiViewTextInput,
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
            FlipGeminiViewTextInput,
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
            FlipGeminiViewTextInput,
            "Enter Password",
            text_input_temp_buffer.get(),
            text_input_buffer_size,
            textUpdatedPassCallback,
            callbackToSettings,
            view_dispatcher_ref,
            this);
#endif
    } else if(view == SettingsViewAPIKey) {
        if(app->loadChar("api_key", loaded, sizeof(loaded))) {
            strncpy(text_input_temp_buffer.get(), loaded, text_input_buffer_size);
        } else {
            text_input_temp_buffer[0] = '\0'; // Ensure empty if not loaded
        }
        text_input_temp_buffer[text_input_buffer_size - 1] = '\0'; // Ensure null-termination
#ifndef FW_ORIGIN_Momentum
        return easy_flipper_set_uart_text_input(
            &text_input,
            FlipGeminiViewTextInput,
            "Enter API Key",
            text_input_temp_buffer.get(),
            text_input_buffer_size,
            textUpdatedAPIKeyCallback,
            callbackToSettings,
            view_dispatcher_ref,
            this);
#else
        return easy_flipper_set_text_input(
            &text_input,
            FlipGeminiViewTextInput,
            "Enter API Key",
            text_input_temp_buffer.get(),
            text_input_buffer_size,
            textUpdatedAPIKeyCallback,
            callbackToSettings,
            view_dispatcher_ref,
            this);
#endif
    }
    return false;
}

void FlipGeminiSettings::settingsItemSelected(uint32_t index) {
    switch(index) {
    case SettingsViewAPIKey:
    case SettingsViewPassword:
    case SettingsViewSSID:
        startTextInput(index);
        break;
    case SettingsViewConnect: {
        FlipGeminiApp* app = static_cast<FlipGeminiApp*>(appContext);
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

void FlipGeminiSettings::settingsItemSelectedCallback(void* context, uint32_t index) {
    FlipGeminiSettings* settings = (FlipGeminiSettings*)context;
    settings->settingsItemSelected(index);
}

bool FlipGeminiSettings::startTextInput(uint32_t view) {
    freeTextInput();
    if(!initTextInput(view)) {
        FURI_LOG_E(TAG, "Failed to initialize text input for view %lu", view);
        return false;
    }
    if(view_dispatcher_ref && *view_dispatcher_ref) {
        view_dispatcher_switch_to_view(*view_dispatcher_ref, FlipGeminiViewTextInput);
        return true;
    } else {
        FURI_LOG_E(TAG, "View dispatcher reference is null or invalid");
        return false;
    }
}

void FlipGeminiSettings::textUpdated(uint32_t view) {
    // store the entered text
    strncpy(text_input_buffer.get(), text_input_temp_buffer.get(), text_input_buffer_size);

    // Ensure null-termination
    text_input_buffer[text_input_buffer_size - 1] = '\0';

    // app context
    FlipGeminiApp* app = static_cast<FlipGeminiApp*>(appContext);

    switch(view) {
    case SettingsViewAPIKey:
        if(variable_item_api_key) {
            variable_item_set_current_value_text(variable_item_api_key, text_input_buffer.get());
        }
        app->saveChar("api_key", text_input_buffer.get());
        break;
    case SettingsViewPassword:
        if(variable_item_wifi_pass) {
            variable_item_set_current_value_text(variable_item_wifi_pass, text_input_buffer.get());
        }
        app->saveChar("wifi_pass", text_input_buffer.get(), "flipper_http");
        break;
    case SettingsViewSSID:
        if(variable_item_wifi_ssid) {
            variable_item_set_current_value_text(variable_item_wifi_ssid, text_input_buffer.get());
        }
        app->saveChar("wifi_ssid", text_input_buffer.get(), "flipper_http");
        break;
    default:
        break;
    }

    // switch to the settings view
    if(view_dispatcher_ref && *view_dispatcher_ref) {
        view_dispatcher_switch_to_view(*view_dispatcher_ref, FlipGeminiViewSettings);
    }
}
void FlipGeminiSettings::textUpdatedAPIKeyCallback(void* context) {
    FlipGeminiSettings* settings = (FlipGeminiSettings*)context;
    settings->textUpdated(SettingsViewAPIKey);
}
void FlipGeminiSettings::textUpdatedPassCallback(void* context) {
    FlipGeminiSettings* settings = (FlipGeminiSettings*)context;
    settings->textUpdated(SettingsViewPassword);
}
void FlipGeminiSettings::textUpdatedSsidCallback(void* context) {
    FlipGeminiSettings* settings = (FlipGeminiSettings*)context;
    settings->textUpdated(SettingsViewSSID);
}
