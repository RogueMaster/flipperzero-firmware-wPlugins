#include "settings.hpp"
#include "app.hpp"

FlipDownloaderSettings::FlipDownloaderSettings()
{
    // nothing to do
}

FlipDownloaderSettings::~FlipDownloaderSettings()
{
    free();
}

uint32_t FlipDownloaderSettings::callbackToSubmenu(void *context)
{
    UNUSED(context);
    return FlipDownloaderViewSubmenu;
}

uint32_t FlipDownloaderSettings::callbackToSettings(void *context)
{
    UNUSED(context);
    return FlipDownloaderViewSettings;
}

void FlipDownloaderSettings::free()
{
    // Free text input first
    freeTextInput();

    if (variable_item_list && view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_remove_view(*view_dispatcher_ref, FlipDownloaderViewSettings);
        variable_item_list_free(variable_item_list);
        variable_item_list = nullptr;
        variable_item_connect = nullptr;
        variable_item_wifi_ssid = nullptr;
        variable_item_wifi_pass = nullptr;
    }
}

void FlipDownloaderSettings::freeTextInput()
{
    if (text_input && view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_remove_view(*view_dispatcher_ref, FlipDownloaderViewTextInput);
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

bool FlipDownloaderSettings::init(ViewDispatcher **view_dispatcher, void *appContext)
{
    view_dispatcher_ref = view_dispatcher;
    this->appContext = appContext;

    if (!easy_flipper_set_variable_item_list(&variable_item_list, FlipDownloaderViewSettings,
                                             settingsItemSelectedCallback, callbackToSubmenu, view_dispatcher, this))
    {
        return false;
    }

    variable_item_wifi_ssid = variable_item_list_add(variable_item_list, "WiFi SSID", 1, nullptr, nullptr);
    variable_item_wifi_pass = variable_item_list_add(variable_item_list, "WiFi Password", 1, nullptr, nullptr);
    variable_item_connect = variable_item_list_add(variable_item_list, "Connect", 1, nullptr, nullptr);

    char loaded_ssid[64];
    char loaded_pass[64];
    FlipDownloaderApp *app = static_cast<FlipDownloaderApp *>(appContext);
    if (app->load_char("wifi_ssid", loaded_ssid, sizeof(loaded_ssid), "flipper_http"))
    {
        variable_item_set_current_value_text(variable_item_wifi_ssid, loaded_ssid);
    }
    else
    {
        variable_item_set_current_value_text(variable_item_wifi_ssid, "");
    }
    if (app->load_char("wifi_pass", loaded_pass, sizeof(loaded_pass), "flipper_http"))
    {
        variable_item_set_current_value_text(variable_item_wifi_pass, "*****");
    }
    else
    {
        variable_item_set_current_value_text(variable_item_wifi_pass, "");
    }

    variable_item_set_current_value_text(variable_item_connect, "");

    return true;
}

bool FlipDownloaderSettings::initTextInput(uint32_t view)
{
    // check if already initialized
    if (text_input_buffer || text_input_temp_buffer)
    {
        FURI_LOG_E(TAG, "initTextInput: already initialized");
        return false;
    }

    // init buffers
    text_input_buffer_size = 128;
    if (!easy_flipper_set_buffer(reinterpret_cast<char **>(&text_input_buffer), text_input_buffer_size))
    {
        return false;
    }
    if (!easy_flipper_set_buffer(reinterpret_cast<char **>(&text_input_temp_buffer), text_input_buffer_size))
    {
        return false;
    }

    // app context
    FlipDownloaderApp *app = static_cast<FlipDownloaderApp *>(appContext);
    char loaded[256];

    if (view == SettingsViewSSID)
    {
        if (app->load_char("wifi_ssid", loaded, sizeof(loaded), "flipper_http"))
        {
            strncpy(text_input_temp_buffer.get(), loaded, text_input_buffer_size);
        }
        else
        {
            text_input_temp_buffer[0] = '\0'; // Ensure empty if not loaded
        }
        text_input_temp_buffer[text_input_buffer_size - 1] = '\0'; // Ensure null-termination
#ifndef FW_ORIGIN_Momentum
        return easy_flipper_set_uart_text_input(&text_input, FlipDownloaderViewTextInput,
                                                "Enter SSID", text_input_temp_buffer.get(), text_input_buffer_size,
                                                textUpdatedSsidCallback, callbackToSettings, view_dispatcher_ref, this);
#else
        return easy_flipper_set_text_input(&text_input, FlipDownloaderViewTextInput,
                                           "Enter SSID", text_input_temp_buffer.get(), text_input_buffer_size,
                                           textUpdatedSsidCallback, callbackToSettings, view_dispatcher_ref, this);
#endif
    }
    else if (view == SettingsViewPassword)
    {
        if (app->load_char("wifi_pass", loaded, sizeof(loaded), "flipper_http"))
        {
            strncpy(text_input_temp_buffer.get(), loaded, text_input_buffer_size);
        }
        else
        {
            text_input_temp_buffer[0] = '\0'; // Ensure empty if not loaded
        }
        text_input_temp_buffer[text_input_buffer_size - 1] = '\0'; // Ensure null-termination
#ifndef FW_ORIGIN_Momentum
        return easy_flipper_set_uart_text_input(&text_input, FlipDownloaderViewTextInput,
                                                "Enter Password", text_input_temp_buffer.get(), text_input_buffer_size,
                                                textUpdatedPassCallback, callbackToSettings, view_dispatcher_ref, this);
#else
        return easy_flipper_set_text_input(&text_input, FlipDownloaderViewTextInput,
                                           "Enter Password", text_input_temp_buffer.get(), text_input_buffer_size,
                                           textUpdatedPassCallback, callbackToSettings, view_dispatcher_ref, this);
#endif
    }
    return false;
}

void FlipDownloaderSettings::settingsItemSelectedCallback(void *context, uint32_t index)
{
    FlipDownloaderSettings *settings = (FlipDownloaderSettings *)context;
    settings->settingsItemSelected(index);
}

void FlipDownloaderSettings::settingsItemSelected(uint32_t index)
{
    switch (index)
    {
    case SettingsViewSSID:
    case SettingsViewPassword:
        startTextInput(index);
        break;
    case SettingsViewConnect:
    {
        FlipDownloaderApp *app = static_cast<FlipDownloaderApp *>(appContext);
        char loaded_ssid[64];
        char loaded_pass[64];
        if (!app->load_char("wifi_ssid", loaded_ssid, sizeof(loaded_ssid), "flipper_http") ||
            !app->load_char("wifi_pass", loaded_pass, sizeof(loaded_pass), "flipper_http"))
        {
            FURI_LOG_E(TAG, "WiFi credentials not set");
            easy_flipper_dialog("No WiFi Credentials", "Please set your WiFi SSID\nand Password in Settings.");
        }
        else
        {
            app->sendWiFiCredentials(loaded_ssid, loaded_pass);
        }
    }
    break;
    default:
        break;
    };
}

bool FlipDownloaderSettings::startTextInput(uint32_t view)
{
    freeTextInput();
    if (!initTextInput(view))
    {
        FURI_LOG_E(TAG, "Failed to initialize text input for view %lu", view);
        return false;
    }
    if (view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_switch_to_view(*view_dispatcher_ref, FlipDownloaderViewTextInput);
        return true;
    }
    else
    {
        FURI_LOG_E(TAG, "View dispatcher reference is null or invalid");
        return false;
    }
}

// Text input callback wrappers
void FlipDownloaderSettings::textUpdatedSsidCallback(void *context)
{
    FlipDownloaderSettings *settings = (FlipDownloaderSettings *)context;
    settings->textUpdated(SettingsViewSSID);
}

void FlipDownloaderSettings::textUpdatedPassCallback(void *context)
{
    FlipDownloaderSettings *settings = (FlipDownloaderSettings *)context;
    settings->textUpdated(SettingsViewPassword);
}

void FlipDownloaderSettings::textUpdated(uint32_t view)
{
    // store the entered text
    strncpy(text_input_buffer.get(), text_input_temp_buffer.get(), text_input_buffer_size);

    // Ensure null-termination
    text_input_buffer[text_input_buffer_size - 1] = '\0';

    // app context
    FlipDownloaderApp *app = static_cast<FlipDownloaderApp *>(appContext);

    switch (view)
    {
    case SettingsViewSSID:
        if (variable_item_wifi_ssid)
        {
            variable_item_set_current_value_text(variable_item_wifi_ssid, text_input_buffer.get());
        }
        app->save_char("wifi_ssid", text_input_buffer.get(), "flipper_http");
        break;
    case SettingsViewPassword:
        if (variable_item_wifi_pass)
        {
            variable_item_set_current_value_text(variable_item_wifi_pass, text_input_buffer.get());
        }
        app->save_char("wifi_pass", text_input_buffer.get(), "flipper_http");
        break;
    default:
        break;
    }

    // switch to the settings view
    if (view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_switch_to_view(*view_dispatcher_ref, FlipDownloaderViewSettings);
    }
}
