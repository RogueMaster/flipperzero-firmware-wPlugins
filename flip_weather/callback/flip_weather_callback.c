#include "callback/flip_weather_callback.h"

// Below added by Derek Jamison
// FURI_LOG_DEV will log only during app development. Be sure that Settings/System/Log Device is "LPUART"; so we dont use serial port.
#ifdef DEVELOPMENT
#define FURI_LOG_DEV(tag, format, ...) furi_log_print_format(FuriLogLevelInfo, tag, format, ##__VA_ARGS__)
#define DEV_CRASH() furi_crash()
#else
#define FURI_LOG_DEV(tag, format, ...)
#define DEV_CRASH()
#endif

bool weather_request_success = false;
bool sent_weather_request = false;
bool got_weather_data = false;

static void flip_weather_request_error_draw(Canvas *canvas)
{
    if (canvas == NULL)
    {
        FURI_LOG_E(TAG, "flip_weather_request_error_draw - canvas is NULL");
        DEV_CRASH();
        return;
    }
    if (fhttp.last_response != NULL)
    {
        if (strstr(fhttp.last_response, "[ERROR] Not connected to Wifi. Failed to reconnect.") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[ERROR] Not connected to Wifi.");
            canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
            canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
        }
        else if (strstr(fhttp.last_response, "[ERROR] Failed to connect to Wifi.") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[ERROR] Not connected to Wifi.");
            canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
            canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
        }
        else if (strstr(fhttp.last_response, "[ERROR] GET request failed or returned empty data.") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[ERROR] WiFi error.");
            canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
            canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
        }
        else if (strstr(fhttp.last_response, "[PONG]") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[STATUS]Connecting to AP...");
        }
        else if (strstr(fhttp.last_response, "generationtime_ms") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[ERROR] Location not found.");
            canvas_draw_str(canvas, 0, 50, "Check your location setting.");
            canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
        }
        else
        {
            canvas_clear(canvas);
            FURI_LOG_E(TAG, "Received an error: %s", fhttp.last_response);
            canvas_draw_str(canvas, 0, 10, "[ERROR] Unusual error...");
            canvas_draw_str(canvas, 0, 60, "Press BACK and retry.");
        }
    }
    else
    {
        canvas_clear(canvas);
        canvas_draw_str(canvas, 0, 10, "[ERROR] Unknown error.");
        canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
        canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
    }
}
static void flip_weather_gps_switch_to_view(FlipWeatherApp *app)
{
    flip_weather_generic_switch_to_view(app, "Fetching GPS data..", send_geo_location_request, process_geo_location, 1, callback_to_submenu, FlipWeatherViewLoader);
}

static void flip_weather_weather_switch_to_view(FlipWeatherApp *app)
{
    flip_weather_generic_switch_to_view(app, "Fetching Weather data..", send_geo_weather_request, process_weather, 1, callback_to_submenu, FlipWeatherViewLoader);
}

void temperature_unit_change(VariableItem *item)
{
    uint8_t index = variable_item_get_current_value_index(item);
    use_fahrenheit = (index == 1);
    variable_item_set_current_value_text(item, use_fahrenheit ? "Fahrenheit" : "Celsius");
    save_settings(
        app_instance->uart_text_input_buffer_ssid,
        app_instance->uart_text_input_buffer_password,
        app_instance->uart_text_input_buffer_location,
        use_fahrenheit);
}

void callback_submenu_choices(void *context, uint32_t index)
{
    FlipWeatherApp *app = (FlipWeatherApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWeatherApp is NULL");
        return;
    }
    switch (index)
    {
    case FlipWeatherSubmenuIndexWeather:
        flipper_http_loading_task(send_geo_location_request, process_geo_location_2, FlipWeatherViewSubmenu, FlipWeatherViewSubmenu, &app->view_dispatcher);
        flip_weather_weather_switch_to_view(app);
        break;
    case FlipWeatherSubmenuIndexGPS:
        flip_weather_gps_switch_to_view(app);
        break;
    case FlipWeatherSubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWeatherViewAbout);
        break;
    case FlipWeatherSubmenuIndexSettings:
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWeatherViewSettings);
        break;
    default:
        break;
    }
}

void text_updated_ssid(void *context)
{
    FlipWeatherApp *app = (FlipWeatherApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWeatherApp is NULL");
        return;
    }

    // store the entered text
    strncpy(app->uart_text_input_buffer_ssid, app->uart_text_input_temp_buffer_ssid, app->uart_text_input_buffer_size_ssid);

    // Ensure null-termination
    app->uart_text_input_buffer_ssid[app->uart_text_input_buffer_size_ssid - 1] = '\0';

    // update the variable item text
    if (app->variable_item_ssid)
    {
        variable_item_set_current_value_text(app->variable_item_ssid, app->uart_text_input_buffer_ssid);
    }

    // save settings
    save_settings(app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_password, app->uart_text_input_buffer_location, use_fahrenheit);

    // save wifi settings to devboard
    if (strlen(app->uart_text_input_buffer_ssid) > 0 && strlen(app->uart_text_input_buffer_password) > 0)
    {
        if (!flipper_http_save_wifi(app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_password))
        {
            FURI_LOG_E(TAG, "Failed to save wifi settings");
        }
    }

    // switch to the settings view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWeatherViewSettings);
}

void text_updated_password(void *context)
{
    FlipWeatherApp *app = (FlipWeatherApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWeatherApp is NULL");
        return;
    }

    // store the entered text
    strncpy(app->uart_text_input_buffer_password, app->uart_text_input_temp_buffer_password, app->uart_text_input_buffer_size_password);

    // Ensure null-termination
    app->uart_text_input_buffer_password[app->uart_text_input_buffer_size_password - 1] = '\0';

    // update the variable item text
    if (app->variable_item_password)
    {
        variable_item_set_current_value_text(app->variable_item_password, app->uart_text_input_buffer_password);
    }

    // save settings
    save_settings(app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_password, app->uart_text_input_buffer_location, use_fahrenheit);

    // save wifi settings to devboard
    if (strlen(app->uart_text_input_buffer_ssid) > 0 && strlen(app->uart_text_input_buffer_password) > 0)
    {
        if (!flipper_http_save_wifi(app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_password))
        {
            FURI_LOG_E(TAG, "Failed to save wifi settings");
        }
    }

    // switch to the settings view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWeatherViewSettings);
}

void text_updated_location(void *context)
{
    FlipWeatherApp *app = (FlipWeatherApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWeatherApp is NULL");
        return;
    }

    // store the entered text
    strncpy(app->uart_text_input_buffer_location, app->uart_text_input_temp_buffer_location, app->uart_text_input_buffer_size_location);

    // Ensure null-termination
    app->uart_text_input_buffer_location[app->uart_text_input_buffer_size_location - 1] = '\0';

    // update the global custom_location
    strncpy(custom_location, app->uart_text_input_buffer_location, sizeof(custom_location) - 1);
    custom_location[sizeof(custom_location) - 1] = '\0';

    // update the variable item text
    if (app->variable_item_location)
    {
        variable_item_set_current_value_text(app->variable_item_location, app->uart_text_input_buffer_location);
    }

    // save settings
    save_settings(app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_password, app->uart_text_input_buffer_location, use_fahrenheit);

    // switch to the settings view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWeatherViewSettings);
}

uint32_t callback_to_submenu(void *context)
{
    if (!context)
    {
        FURI_LOG_E(TAG, "Context is NULL");
        return VIEW_NONE;
    }
    UNUSED(context);
    sent_get_request = false;
    get_request_success = false;
    got_ip_address = false;
    got_weather_data = false;
    geo_information_processed = false;
    weather_information_processed = false;
    sent_weather_request = false;
    weather_request_success = false;
    if (weather_data != NULL)
    {
        free(weather_data);
        weather_data = NULL;
    }
    if (total_data != NULL)
    {
        free(total_data);
        total_data = NULL;
    }
    return FlipWeatherViewSubmenu;
}

void settings_item_selected(void *context, uint32_t index)
{
    FlipWeatherApp *app = (FlipWeatherApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipWeatherApp is NULL");
        return;
    }
    switch (index)
    {
    case 0: // Input SSID
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWeatherViewTextInputSSID);
        break;
    case 1: // Input Password
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWeatherViewTextInputPassword);
        break;
    case 2: // Input Location
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWeatherViewTextInputLocation);
        break;
    default:
        FURI_LOG_E(TAG, "Unknown configuration item index");
        break;
    }
}

/**
 * @brief Navigation callback for exiting the application
 * @param context The context - unused
 * @return next view id (VIEW_NONE to exit the app)
 */
uint32_t callback_exit_app(void *context)
{
    // Exit the application
    if (!context)
    {
        FURI_LOG_E(TAG, "Context is NULL");
        return VIEW_NONE;
    }
    UNUSED(context);
    return VIEW_NONE; // Return VIEW_NONE to exit the app
}

uint32_t callback_to_wifi_settings(void *context)
{
    if (!context)
    {
        FURI_LOG_E(TAG, "Context is NULL");
        return VIEW_NONE;
    }
    UNUSED(context);
    return FlipWeatherViewSettings;
}

static void flip_weather_widget_set_text(char *message, Widget **widget)
{
    if (widget == NULL)
    {
        FURI_LOG_E(TAG, "flip_weather_set_widget_text - widget is NULL");
        DEV_CRASH();
        return;
    }
    if (message == NULL)
    {
        FURI_LOG_E(TAG, "flip_weather_set_widget_text - message is NULL");
        DEV_CRASH();
        return;
    }
    widget_reset(*widget);

    uint32_t message_length = strlen(message); // Length of the message
    uint32_t i = 0;                            // Index tracker
    uint32_t formatted_index = 0;              // Tracker for where we are in the formatted message
    char *formatted_message;                   // Buffer to hold the final formatted message

    // Allocate buffer with double the message length plus one for safety
    if (!easy_flipper_set_buffer(&formatted_message, message_length * 2 + 1))
    {
        return;
    }

    while (i < message_length)
    {
        uint32_t max_line_length = 31;                  // Maximum characters per line
        uint32_t remaining_length = message_length - i; // Remaining characters
        uint32_t line_length = (remaining_length < max_line_length) ? remaining_length : max_line_length;

        // Check for newline character within the current segment
        uint32_t newline_pos = i;
        bool found_newline = false;
        for (; newline_pos < i + line_length && newline_pos < message_length; newline_pos++)
        {
            if (message[newline_pos] == '\n')
            {
                found_newline = true;
                break;
            }
        }

        if (found_newline)
        {
            // If newline found, set line_length up to the newline
            line_length = newline_pos - i;
        }

        // Temporary buffer to hold the current line
        char line[32];
        strncpy(line, message + i, line_length);
        line[line_length] = '\0';

        // If newline was found, skip it for the next iteration
        if (found_newline)
        {
            i += line_length + 1; // +1 to skip the '\n' character
        }
        else
        {
            // Check if the line ends in the middle of a word and adjust accordingly
            if (line_length == max_line_length && message[i + line_length] != '\0' && message[i + line_length] != ' ')
            {
                // Find the last space within the current line to avoid breaking a word
                char *last_space = strrchr(line, ' ');
                if (last_space != NULL)
                {
                    // Adjust the line_length to avoid cutting the word
                    line_length = last_space - line;
                    line[line_length] = '\0'; // Null-terminate at the space
                }
            }

            // Move the index forward by the determined line_length
            i += line_length;

            // Skip any spaces at the beginning of the next line
            while (i < message_length && message[i] == ' ')
            {
                i++;
            }
        }

        // Manually copy the fixed line into the formatted_message buffer
        for (uint32_t j = 0; j < line_length; j++)
        {
            formatted_message[formatted_index++] = line[j];
        }

        // Add a newline character for line spacing
        formatted_message[formatted_index++] = '\n';
    }

    // Null-terminate the formatted_message
    formatted_message[formatted_index] = '\0';

    // Add the formatted message to the widget
    widget_add_text_scroll_element(*widget, 0, 0, 128, 64, formatted_message);
}

void flip_weather_loader_draw_callback(Canvas *canvas, void *model)
{
    if (!canvas || !model)
    {
        FURI_LOG_E(TAG, "flip_weather_loader_draw_callback - canvas or model is NULL");
        return;
    }

    SerialState http_state = fhttp.state;
    DataLoaderModel *data_loader_model = (DataLoaderModel *)model;
    DataState data_state = data_loader_model->data_state;
    char *title = data_loader_model->title;

    canvas_set_font(canvas, FontSecondary);

    if (http_state == INACTIVE)
    {
        canvas_draw_str(canvas, 0, 7, "Wifi Dev Board disconnected.");
        canvas_draw_str(canvas, 0, 17, "Please connect to the board.");
        canvas_draw_str(canvas, 0, 32, "If your board is connected,");
        canvas_draw_str(canvas, 0, 42, "make sure you have flashed");
        canvas_draw_str(canvas, 0, 52, "your WiFi Devboard with the");
        canvas_draw_str(canvas, 0, 62, "latest FlipperHTTP flash.");
        return;
    }

    if (data_state == DataStateError || data_state == DataStateParseError)
    {
        flip_weather_request_error_draw(canvas);
        return;
    }

    canvas_draw_str(canvas, 0, 7, title);
    canvas_draw_str(canvas, 0, 17, "Loading...");

    if (data_state == DataStateInitial)
    {
        return;
    }

    if (http_state == SENDING)
    {
        canvas_draw_str(canvas, 0, 27, "Sending...");
        return;
    }

    if (http_state == RECEIVING || data_state == DataStateRequested)
    {
        canvas_draw_str(canvas, 0, 27, "Receiving...");
        return;
    }

    if (http_state == IDLE && data_state == DataStateReceived)
    {
        canvas_draw_str(canvas, 0, 27, "Processing...");
        return;
    }

    if (http_state == IDLE && data_state == DataStateParsed)
    {
        canvas_draw_str(canvas, 0, 27, "Processed...");
        return;
    }
}

static void flip_weather_loader_process_callback(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "flip_weather_loader_process_callback - context is NULL");
        DEV_CRASH();
        return;
    }

    FlipWeatherApp *app = (FlipWeatherApp *)context;
    View *view = app->view_loader;

    DataState current_data_state;
    with_view_model(view, DataLoaderModel * model, { current_data_state = model->data_state; }, false);

    if (current_data_state == DataStateInitial)
    {
        with_view_model(
            view,
            DataLoaderModel * model,
            {
                model->data_state = DataStateRequested;
                DataLoaderFetch fetch = model->fetcher;
                if (fetch == NULL)
                {
                    FURI_LOG_E(TAG, "Model doesn't have Fetch function assigned.");
                    model->data_state = DataStateError;
                    return;
                }

                // Clear any previous responses
                strncpy(fhttp.last_response, "", 1);
                bool request_status = fetch(model);
                if (!request_status)
                {
                    model->data_state = DataStateError;
                }
            },
            true);
    }
    else if (current_data_state == DataStateRequested || current_data_state == DataStateError)
    {
        if (fhttp.state == IDLE && fhttp.last_response != NULL)
        {
            if (strstr(fhttp.last_response, "[PONG]") != NULL)
            {
                FURI_LOG_DEV(TAG, "PONG received.");
            }
            else if (strncmp(fhttp.last_response, "[SUCCESS]", 9) == 0)
            {
                FURI_LOG_DEV(TAG, "SUCCESS received. %s", fhttp.last_response ? fhttp.last_response : "NULL");
            }
            else if (strncmp(fhttp.last_response, "[ERROR]", 9) == 0)
            {
                FURI_LOG_DEV(TAG, "ERROR received. %s", fhttp.last_response ? fhttp.last_response : "NULL");
            }
            else if (strlen(fhttp.last_response) == 0)
            {
                // Still waiting on response
            }
            else
            {
                with_view_model(view, DataLoaderModel * model, { model->data_state = DataStateReceived; }, true);
            }
        }
        else if (fhttp.state == SENDING || fhttp.state == RECEIVING)
        {
            // continue waiting
        }
        else if (fhttp.state == INACTIVE)
        {
            // inactive. try again
        }
        else if (fhttp.state == ISSUE)
        {
            with_view_model(view, DataLoaderModel * model, { model->data_state = DataStateError; }, true);
        }
        else
        {
            FURI_LOG_DEV(TAG, "Unexpected state: %d lastresp: %s", fhttp.state, fhttp.last_response ? fhttp.last_response : "NULL");
            DEV_CRASH();
        }
    }
    else if (current_data_state == DataStateReceived)
    {
        with_view_model(
            view,
            DataLoaderModel * model,
            {
                char *data_text;
                if (model->parser == NULL)
                {
                    data_text = NULL;
                    FURI_LOG_DEV(TAG, "Parser is NULL");
                    DEV_CRASH();
                }
                else
                {
                    data_text = model->parser(model);
                }
                FURI_LOG_DEV(TAG, "Parsed data: %s\r\ntext: %s", fhttp.last_response ? fhttp.last_response : "NULL", data_text ? data_text : "NULL");
                model->data_text = data_text;
                if (data_text == NULL)
                {
                    model->data_state = DataStateParseError;
                }
                else
                {
                    model->data_state = DataStateParsed;
                }
            },
            true);
    }
    else if (current_data_state == DataStateParsed)
    {
        with_view_model(
            view,
            DataLoaderModel * model,
            {
                if (++model->request_index < model->request_count)
                {
                    model->data_state = DataStateInitial;
                }
                else
                {
                    flip_weather_widget_set_text(model->data_text != NULL ? model->data_text : "Empty result", &app_instance->widget_result);
                    if (model->data_text != NULL)
                    {
                        free(model->data_text);
                        model->data_text = NULL;
                    }
                    view_set_previous_callback(widget_get_view(app_instance->widget_result), model->back_callback);
                    view_dispatcher_switch_to_view(app_instance->view_dispatcher, FlipWeatherViewWidgetResult);
                }
            },
            true);
    }
}

static void flip_weather_loader_timer_callback(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "flip_weather_loader_timer_callback - context is NULL");
        DEV_CRASH();
        return;
    }
    FlipWeatherApp *app = (FlipWeatherApp *)context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FlipWeatherCustomEventProcess);
}

static void flip_weather_loader_on_enter(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "flip_weather_loader_on_enter - context is NULL");
        DEV_CRASH();
        return;
    }
    FlipWeatherApp *app = (FlipWeatherApp *)context;
    View *view = app->view_loader;
    with_view_model(
        view,
        DataLoaderModel * model,
        {
            view_set_previous_callback(view, model->back_callback);
            if (model->timer == NULL)
            {
                model->timer = furi_timer_alloc(flip_weather_loader_timer_callback, FuriTimerTypePeriodic, app);
            }
            furi_timer_start(model->timer, 250);
        },
        true);
}

static void flip_weather_loader_on_exit(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "flip_weather_loader_on_exit - context is NULL");
        DEV_CRASH();
        return;
    }
    FlipWeatherApp *app = (FlipWeatherApp *)context;
    View *view = app->view_loader;
    with_view_model(
        view,
        DataLoaderModel * model,
        {
            if (model->timer)
            {
                furi_timer_stop(model->timer);
            }
        },
        false);
}

void flip_weather_loader_init(View *view)
{
    if (view == NULL)
    {
        FURI_LOG_E(TAG, "flip_weather_loader_init - view is NULL");
        DEV_CRASH();
        return;
    }
    view_allocate_model(view, ViewModelTypeLocking, sizeof(DataLoaderModel));
    view_set_enter_callback(view, flip_weather_loader_on_enter);
    view_set_exit_callback(view, flip_weather_loader_on_exit);
}

void flip_weather_loader_free_model(View *view)
{
    if (view == NULL)
    {
        FURI_LOG_E(TAG, "flip_weather_loader_free_model - view is NULL");
        DEV_CRASH();
        return;
    }
    with_view_model(
        view,
        DataLoaderModel * model,
        {
            if (model->timer)
            {
                furi_timer_free(model->timer);
                model->timer = NULL;
            }
            if (model->parser_context)
            {
                free(model->parser_context);
                model->parser_context = NULL;
            }
        },
        false);
}

bool flip_weather_custom_event_callback(void *context, uint32_t index)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "flip_weather_custom_event_callback - context is NULL");
        DEV_CRASH();
        return false;
    }

    switch (index)
    {
    case FlipWeatherCustomEventProcess:
        flip_weather_loader_process_callback(context);
        return true;
    default:
        FURI_LOG_DEV(TAG, "flip_weather_custom_event_callback. Unknown index: %ld", index);
        return false;
    }
}

void flip_weather_generic_switch_to_view(FlipWeatherApp *app, char *title, DataLoaderFetch fetcher, DataLoaderParser parser, size_t request_count, ViewNavigationCallback back, uint32_t view_id)
{
    if (app == NULL)
    {
        FURI_LOG_E(TAG, "flip_weather_generic_switch_to_view - app is NULL");
        DEV_CRASH();
        return;
    }

    View *view = app->view_loader;
    if (view == NULL)
    {
        FURI_LOG_E(TAG, "flip_weather_generic_switch_to_view - view is NULL");
        DEV_CRASH();
        return;
    }

    with_view_model(
        view,
        DataLoaderModel * model,
        {
            model->title = title;
            model->fetcher = fetcher;
            model->parser = parser;
            model->request_index = 0;
            model->request_count = request_count;
            model->back_callback = back;
            model->data_state = DataStateInitial;
            model->data_text = NULL;
        },
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, view_id);
}
