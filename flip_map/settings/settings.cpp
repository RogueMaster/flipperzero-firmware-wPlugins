#include "settings.hpp"
#include "app.hpp"

FlipMapSettings::FlipMapSettings(ViewDispatcher **view_dispatcher, void *appContext) : appContext(appContext), view_dispatcher_ref(view_dispatcher)
{
    if (!easy_flipper_set_variable_item_list(&variable_item_list, FlipMapViewSettings,
                                             settingsItemSelectedCallback, callbackToSubmenu, view_dispatcher, this))
    {
        return;
    }

    variable_item_wifi_ssid = variable_item_list_add(variable_item_list, "WiFi SSID", 1, nullptr, nullptr);
    variable_item_wifi_pass = variable_item_list_add(variable_item_list, "WiFi Password", 1, nullptr, nullptr);
    variable_item_connect = variable_item_list_add(variable_item_list, "[Connect To WiFi]", 1, nullptr, nullptr);
    variable_item_user_name = variable_item_list_add(variable_item_list, "User Name", 1, nullptr, nullptr);
    variable_item_user_pass = variable_item_list_add(variable_item_list, "User Password", 1, nullptr, nullptr);
    variable_item_location = variable_item_list_add(variable_item_list, "Location", 2, callbackLocation, nullptr);

    char loaded_ssid[64];
    char loaded_pass[64];
    FlipMapApp *app = static_cast<FlipMapApp *>(appContext);
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

    char locationStatus[16];
    if (app->loadChar("location_status", locationStatus, sizeof(locationStatus)))
    {
        const int index = strcmp(locationStatus, "Enabled") == 0 ? 1 : 0;
        variable_item_set_current_value_index(variable_item_location, index);
        variable_item_set_current_value_text(variable_item_location, locationStatus);
    }
    else
    {
        variable_item_set_current_value_index(variable_item_location, 0);
        variable_item_set_current_value_text(variable_item_location, "Disabled");
    }
}

FlipMapSettings::~FlipMapSettings()
{
    // Free text input first
    freeTextInput();

    if (variable_item_list && view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_remove_view(*view_dispatcher_ref, FlipMapViewSettings);
        variable_item_list_free(variable_item_list);
        variable_item_list = nullptr;
        variable_item_wifi_ssid = nullptr;
        variable_item_wifi_pass = nullptr;
    }
}

uint32_t FlipMapSettings::callbackToSettings(void *context)
{
    UNUSED(context);
    return FlipMapViewSettings;
}

uint32_t FlipMapSettings::callbackToSubmenu(void *context)
{
    FlipMapApp *app = static_cast<FlipMapApp *>(context);
    furi_check(app);

    char locationStatus[16];
    if (app->loadChar("location_status", locationStatus, sizeof(locationStatus)))
    {
        const int index = strcmp(locationStatus, "Enabled") == 0 ? 1 : 0;

        // show warning message if enabled
        if (index == 1)
        {
            easy_flipper_dialog("Warning", "User location is enabled.\n\nOther users may see your\ngeneral location ONLY. Exact\nlocation is NEVER shared!");
        }
    }
    return FlipMapViewSubmenu;
}

void FlipMapSettings::callbackLocation(VariableItem *item)
{

    uint8_t index = variable_item_get_current_value_index(item);
    const char *locationOptions[] = {"Disabled", "Enabled"};
    variable_item_set_current_value_text(item, locationOptions[index]);
    variable_item_set_current_value_index(item, index);
    // manual save here since appContext is not available in static methods
    // which still doesnt make sense to me but probably because it expects a C function
    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    File *file = storage_file_alloc(storage);
    char file_path[256];
    snprintf(file_path, sizeof(file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/%s.txt", APP_ID, "location_status");
    storage_file_open(file, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    size_t data_size = strlen(locationOptions[index]) + 1; // Include null terminator
    storage_file_write(file, locationOptions[index], data_size);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void FlipMapSettings::freeTextInput()
{
    if (text_input && view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_remove_view(*view_dispatcher_ref, FlipMapViewTextInput);
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

bool FlipMapSettings::initTextInput(uint32_t view)
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
    FlipMapApp *app = static_cast<FlipMapApp *>(appContext);
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
        return easy_flipper_set_uart_text_input(&text_input, FlipMapViewTextInput,
                                                "Enter SSID", text_input_temp_buffer.get(), text_input_buffer_size,
                                                textUpdatedSsidCallback, callbackToSettings, view_dispatcher_ref, this);
#else
        return easy_flipper_set_text_input(&text_input, FlipMapViewTextInput,
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
        return easy_flipper_set_uart_text_input(&text_input, FlipMapViewTextInput,
                                                "Enter Password", text_input_temp_buffer.get(), text_input_buffer_size,
                                                textUpdatedPassCallback, callbackToSettings, view_dispatcher_ref, this);
#else
        return easy_flipper_set_text_input(&text_input, FlipMapViewTextInput,
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
        return easy_flipper_set_uart_text_input(&text_input, FlipMapViewTextInput,
                                                "Enter User Name", text_input_temp_buffer.get(), text_input_buffer_size,
                                                textUpdatedUserNameCallback, callbackToSettings, view_dispatcher_ref, this);
#else
        return easy_flipper_set_text_input(&text_input, FlipMapViewTextInput,
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
        return easy_flipper_set_uart_text_input(&text_input, FlipMapViewTextInput,
                                                "Enter User Password", text_input_temp_buffer.get(), text_input_buffer_size,
                                                textUpdatedUserPassCallback, callbackToSettings, view_dispatcher_ref, this);
#else
        return easy_flipper_set_text_input(&text_input, FlipMapViewTextInput,
                                           "Enter User Password", text_input_temp_buffer.get(), text_input_buffer_size,
                                           textUpdatedUserPassCallback, callbackToSettings, view_dispatcher_ref, this);
#endif
    }
    return false;
}

void FlipMapSettings::settingsItemSelected(uint32_t index)
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
        FlipMapApp *app = static_cast<FlipMapApp *>(appContext);
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

void FlipMapSettings::settingsItemSelectedCallback(void *context, uint32_t index)
{
    FlipMapSettings *settings = (FlipMapSettings *)context;
    settings->settingsItemSelected(index);
}

bool FlipMapSettings::startTextInput(uint32_t view)
{
    freeTextInput();
    if (!initTextInput(view))
    {
        FURI_LOG_E(TAG, "Failed to initialize text input for view %lu", view);
        return false;
    }
    if (view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_switch_to_view(*view_dispatcher_ref, FlipMapViewTextInput);
        return true;
    }
    else
    {
        FURI_LOG_E(TAG, "View dispatcher reference is null or invalid");
        return false;
    }
}

void FlipMapSettings::textUpdated(uint32_t view)
{
    // store the entered text
    strncpy(text_input_buffer.get(), text_input_temp_buffer.get(), text_input_buffer_size);

    // Ensure null-termination
    text_input_buffer[text_input_buffer_size - 1] = '\0';

    // app context
    FlipMapApp *app = static_cast<FlipMapApp *>(appContext);

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
        view_dispatcher_switch_to_view(*view_dispatcher_ref, FlipMapViewSettings);
    }
}

void FlipMapSettings::textUpdatedSsidCallback(void *context)
{
    FlipMapSettings *settings = (FlipMapSettings *)context;
    settings->textUpdated(SettingsViewSSID);
}

void FlipMapSettings::textUpdatedPassCallback(void *context)
{
    FlipMapSettings *settings = (FlipMapSettings *)context;
    settings->textUpdated(SettingsViewPassword);
}

void FlipMapSettings::textUpdatedUserNameCallback(void *context)
{
    FlipMapSettings *settings = (FlipMapSettings *)context;
    settings->textUpdated(SettingsViewUserName);
}

void FlipMapSettings::textUpdatedUserPassCallback(void *context)
{
    FlipMapSettings *settings = (FlipMapSettings *)context;
    settings->textUpdated(SettingsViewUserPass);
}
