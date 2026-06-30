#include "settings.hpp"
#include "app.hpp"

FlipSocialSettings::FlipSocialSettings(ViewDispatcher **view_dispatcher, void *appContext) : appContext(appContext), view_dispatcher_ref(view_dispatcher)
{
    if (!easy_flipper_set_variable_item_list(&variable_item_list, FlipSocialViewSettings,
                                             settingsItemSelectedCallback, callbackToSubmenu, view_dispatcher, this))
    {
        return;
    }

    variable_item_wifi_ssid = variable_item_list_add(variable_item_list, "WiFi SSID", 1, nullptr, nullptr);
    variable_item_wifi_pass = variable_item_list_add(variable_item_list, "WiFi Password", 1, nullptr, nullptr);
    variable_item_connect = variable_item_list_add(variable_item_list, "[Connect To WiFi]", 1, nullptr, nullptr);
    variable_item_user_name = variable_item_list_add(variable_item_list, "User Name", 1, nullptr, nullptr);
    variable_item_user_pass = variable_item_list_add(variable_item_list, "User Password", 1, nullptr, nullptr);

    char loaded_ssid[64];
    char loaded_pass[64];
    FlipSocialApp *app = static_cast<FlipSocialApp *>(appContext);
    if (app->loadChar("wifi_ssid", loaded_ssid, sizeof(loaded_ssid), "flipper_http"))
    {
        variable_item_set_current_value_text(variable_item_wifi_ssid, loaded_ssid);
    }
    else
    {
        variable_item_set_current_value_text(variable_item_wifi_ssid, "");
    }
    if (app->loadChar("wifi_pass", loaded_pass, sizeof(loaded_pass), "flipper_http"))
    {
        variable_item_set_current_value_text(variable_item_wifi_pass, "*****");
    }
    else
    {
        variable_item_set_current_value_text(variable_item_wifi_pass, "");
    }
    variable_item_set_current_value_text(variable_item_connect, "");
    if (app->loadChar("user_name", loaded_ssid, sizeof(loaded_ssid), "flipper_http"))
    {
        variable_item_set_current_value_text(variable_item_user_name, loaded_ssid);
    }
    else
    {
        variable_item_set_current_value_text(variable_item_user_name, "");
    }
    if (app->loadChar("user_pass", loaded_pass, sizeof(loaded_pass), "flipper_http"))
    {
        variable_item_set_current_value_text(variable_item_user_pass, "*****");
    }
    else
    {
        variable_item_set_current_value_text(variable_item_user_pass, "");
    }
}

FlipSocialSettings::~FlipSocialSettings()
{
    // Free text input first
    freeTextInput();

    if (variable_item_list && view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_remove_view(*view_dispatcher_ref, FlipSocialViewSettings);
        variable_item_list_free(variable_item_list);
        variable_item_list = nullptr;
        variable_item_wifi_ssid = nullptr;
        variable_item_wifi_pass = nullptr;
    }
}

uint32_t FlipSocialSettings::callbackToSettings(void *context)
{
    UNUSED(context);
    return FlipSocialViewSettings;
}

uint32_t FlipSocialSettings::callbackToSubmenu(void *context)
{
    UNUSED(context);
    return FlipSocialViewSubmenu;
}

void FlipSocialSettings::freeTextInput()
{
    if (text_input && view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_remove_view(*view_dispatcher_ref, FlipSocialViewTextInput);
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

bool FlipSocialSettings::initTextInput(uint32_t view)
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
    FlipSocialApp *app = static_cast<FlipSocialApp *>(appContext);
    char loaded[256];

    if (view == SettingsViewSSID)
    {
        if (app->loadChar("wifi_ssid", loaded, sizeof(loaded), "flipper_http"))
        {
            strncpy(text_input_temp_buffer.get(), loaded, text_input_buffer_size);
        }
        else
        {
            text_input_temp_buffer[0] = '\0'; // Ensure empty if not loaded
        }
        text_input_temp_buffer[text_input_buffer_size - 1] = '\0'; // Ensure null-termination
#ifndef FW_ORIGIN_Momentum
        return easy_flipper_set_uart_text_input(&text_input, FlipSocialViewTextInput,
                                                "Enter SSID", text_input_temp_buffer.get(), text_input_buffer_size,
                                                textUpdatedSsidCallback, callbackToSettings, view_dispatcher_ref, this);
#else
        return easy_flipper_set_text_input(&text_input, FlipSocialViewTextInput,
                                           "Enter SSID", text_input_temp_buffer.get(), text_input_buffer_size,
                                           textUpdatedSsidCallback, callbackToSettings, view_dispatcher_ref, this);
#endif
    }
    else if (view == SettingsViewPassword)
    {
        if (app->loadChar("wifi_pass", loaded, sizeof(loaded), "flipper_http"))
        {
            strncpy(text_input_temp_buffer.get(), loaded, text_input_buffer_size);
        }
        else
        {
            text_input_temp_buffer[0] = '\0'; // Ensure empty if not loaded
        }
        text_input_temp_buffer[text_input_buffer_size - 1] = '\0'; // Ensure null-termination
#ifndef FW_ORIGIN_Momentum
        return easy_flipper_set_uart_text_input(&text_input, FlipSocialViewTextInput,
                                                "Enter Password", text_input_temp_buffer.get(), text_input_buffer_size,
                                                textUpdatedPassCallback, callbackToSettings, view_dispatcher_ref, this);
#else
        return easy_flipper_set_text_input(&text_input, FlipSocialViewTextInput,
                                           "Enter Password", text_input_temp_buffer.get(), text_input_buffer_size,
                                           textUpdatedPassCallback, callbackToSettings, view_dispatcher_ref, this);
#endif
    }
    else if (view == SettingsViewUserName)
    {
        if (app->loadChar("user_name", loaded, sizeof(loaded), "flipper_http"))
        {
            strncpy(text_input_temp_buffer.get(), loaded, text_input_buffer_size);
        }
        else
        {
            text_input_temp_buffer[0] = '\0'; // Ensure empty if not loaded
        }
        text_input_temp_buffer[text_input_buffer_size - 1] = '\0'; // Ensure null-termination
#ifndef FW_ORIGIN_Momentum
        return easy_flipper_set_uart_text_input(&text_input, FlipSocialViewTextInput,
                                                "Enter User Name", text_input_temp_buffer.get(), text_input_buffer_size,
                                                textUpdatedUserNameCallback, callbackToSettings, view_dispatcher_ref, this);
#else
        return easy_flipper_set_text_input(&text_input, FlipSocialViewTextInput,
                                           "Enter User Name", text_input_temp_buffer.get(), text_input_buffer_size,
                                           textUpdatedUserNameCallback, callbackToSettings, view_dispatcher_ref, this);
#endif
    }
    else if (view == SettingsViewUserPass)
    {
        if (app->loadChar("user_pass", loaded, sizeof(loaded), "flipper_http"))
        {
            strncpy(text_input_temp_buffer.get(), loaded, text_input_buffer_size);
        }
        else
        {
            text_input_temp_buffer[0] = '\0'; // Ensure empty if not loaded
        }
        text_input_temp_buffer[text_input_buffer_size - 1] = '\0'; // Ensure null-termination
#ifndef FW_ORIGIN_Momentum
        return easy_flipper_set_uart_text_input(&text_input, FlipSocialViewTextInput,
                                                "Enter User Password", text_input_temp_buffer.get(), text_input_buffer_size,
                                                textUpdatedUserPassCallback, callbackToSettings, view_dispatcher_ref, this);
#else
        return easy_flipper_set_text_input(&text_input, FlipSocialViewTextInput,
                                           "Enter User Password", text_input_temp_buffer.get(), text_input_buffer_size,
                                           textUpdatedUserPassCallback, callbackToSettings, view_dispatcher_ref, this);
#endif
    }
    return false;
}

void FlipSocialSettings::settingsItemSelected(uint32_t index)
{
    switch (index)
    {
    case SettingsViewSSID:
    case SettingsViewPassword:
    case SettingsViewUserName:
    case SettingsViewUserPass:
        startTextInput(index);
        break;
    case SettingsViewConnect:
    {
        FlipSocialApp *app = static_cast<FlipSocialApp *>(appContext);
        char loaded_ssid[64];
        char loaded_pass[64];
        if (!app->loadChar("wifi_ssid", loaded_ssid, sizeof(loaded_ssid), "flipper_http") ||
            !app->loadChar("wifi_pass", loaded_pass, sizeof(loaded_pass), "flipper_http"))
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

void FlipSocialSettings::settingsItemSelectedCallback(void *context, uint32_t index)
{
    FlipSocialSettings *settings = (FlipSocialSettings *)context;
    settings->settingsItemSelected(index);
}

bool FlipSocialSettings::startTextInput(uint32_t view)
{
    freeTextInput();
    if (!initTextInput(view))
    {
        FURI_LOG_E(TAG, "Failed to initialize text input for view %lu", view);
        return false;
    }
    if (view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_switch_to_view(*view_dispatcher_ref, FlipSocialViewTextInput);
        return true;
    }
    else
    {
        FURI_LOG_E(TAG, "View dispatcher reference is null or invalid");
        return false;
    }
}

void FlipSocialSettings::textUpdated(uint32_t view)
{
    // store the entered text
    strncpy(text_input_buffer.get(), text_input_temp_buffer.get(), text_input_buffer_size);

    // Ensure null-termination
    text_input_buffer[text_input_buffer_size - 1] = '\0';

    // app context
    FlipSocialApp *app = static_cast<FlipSocialApp *>(appContext);

    switch (view)
    {
    case SettingsViewSSID:
        if (variable_item_wifi_ssid)
        {
            variable_item_set_current_value_text(variable_item_wifi_ssid, text_input_buffer.get());
        }
        app->saveChar("wifi_ssid", text_input_buffer.get(), "flipper_http");
        break;
    case SettingsViewPassword:
        if (variable_item_wifi_pass)
        {
            variable_item_set_current_value_text(variable_item_wifi_pass, text_input_buffer.get());
        }
        app->saveChar("wifi_pass", text_input_buffer.get(), "flipper_http");
        break;
    case SettingsViewUserName:
        if (variable_item_user_name)
        {
            variable_item_set_current_value_text(variable_item_user_name, text_input_buffer.get());
        }
        app->saveChar("user_name", text_input_buffer.get(), "flipper_http");
        break;
    case SettingsViewUserPass:
        if (variable_item_user_pass)
        {
            variable_item_set_current_value_text(variable_item_user_pass, text_input_buffer.get());
        }
        app->saveChar("user_pass", text_input_buffer.get(), "flipper_http");
        break;
    default:
        break;
    }

    // switch to the settings view
    if (view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_switch_to_view(*view_dispatcher_ref, FlipSocialViewSettings);
    }
}

void FlipSocialSettings::textUpdatedSsidCallback(void *context)
{
    FlipSocialSettings *settings = (FlipSocialSettings *)context;
    settings->textUpdated(SettingsViewSSID);
}

void FlipSocialSettings::textUpdatedPassCallback(void *context)
{
    FlipSocialSettings *settings = (FlipSocialSettings *)context;
    settings->textUpdated(SettingsViewPassword);
}

void FlipSocialSettings::textUpdatedUserNameCallback(void *context)
{
    FlipSocialSettings *settings = (FlipSocialSettings *)context;
    settings->textUpdated(SettingsViewUserName);
}

void FlipSocialSettings::textUpdatedUserPassCallback(void *context)
{
    FlipSocialSettings *settings = (FlipSocialSettings *)context;
    settings->textUpdated(SettingsViewUserPass);
}
