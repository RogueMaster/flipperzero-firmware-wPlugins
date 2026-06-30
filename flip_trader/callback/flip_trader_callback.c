#include <callback/flip_trader_callback.h>

// Below added by Derek Jamison
// FURI_LOG_DEV will log only during app development. Be sure that Settings/System/Log Device is "LPUART"; so we dont use serial port.
#ifdef DEVELOPMENT
#define FURI_LOG_DEV(tag, format, ...) furi_log_print_format(FuriLogLevelInfo, tag, format, ##__VA_ARGS__)
#define DEV_CRASH() furi_crash()
#else
#define FURI_LOG_DEV(tag, format, ...)
#define DEV_CRASH()
#endif

// hold the price of the asset
char asset_price[64];
bool sent_get_request = false;
bool get_request_success = false;
bool request_processed = false;

static void flip_trader_request_error_draw(Canvas *canvas)
{
    if (canvas == NULL)
    {
        FURI_LOG_E(TAG, "flip_trader_request_error_draw - canvas is NULL");
        DEV_CRASH();
        return;
    }
    if (fhttp.last_response != NULL)
    {
        if (strstr(fhttp.last_response, "[ERROR] Not connected to Wifi. Failed to reconnect.") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[ERROR] Not connected to Wifi.");
            canvas_draw_str(canvas, 0, 22, "Failed to reconnect.");
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
        else if (strstr(fhttp.last_response, "[PONG]") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[STATUS]Connecting to AP...");
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
        canvas_draw_str(canvas, 0, 10, "Failed to receive data.");
        canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
        canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
    }
}

static bool send_price_request(AssetLoaderModel* model)
{
    UNUSED(model);
    if (fhttp.state == INACTIVE)
    {
        return false;
    }
    if (!sent_get_request && fhttp.state == IDLE)
    {
        sent_get_request = true;
        char url[128];
        char *headers = jsmn("Content-Type", "application/json");
        snprintf(
            fhttp.file_path,
            sizeof(fhttp.file_path),
            STORAGE_EXT_PATH_PREFIX "/apps_data/flip_trader/price.txt");

        fhttp.save_received_data = true;
        snprintf(url, 128, "https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=%s&apikey=2X90WLEFMP43OJKE", asset_names[asset_index]);
        get_request_success = flipper_http_get_request_with_headers(url, headers);
        free(headers);
        if (!get_request_success)
        {
            FURI_LOG_E(TAG, "Failed to send GET request");
            fhttp.state = ISSUE;
            return false;
        }
        fhttp.state = RECEIVING;
    }
    return true;
}

static char *process_asset_price(AssetLoaderModel* model)
{
    UNUSED(model);
    if (!request_processed)
    {
        // load the received data from the saved file
        FuriString *price_data = flipper_http_load_from_file(fhttp.file_path);
        if (price_data == NULL)
        {
            FURI_LOG_E(TAG, "Failed to load received data from file.");
            fhttp.state = ISSUE;
            return NULL;
        }
        char *data_cstr = (char *)furi_string_get_cstr(price_data);
        if (data_cstr == NULL)
        {
            FURI_LOG_E(TAG, "Failed to get C-string from FuriString.");
            furi_string_free(price_data);
            fhttp.state = ISSUE;
            return NULL;
        }
        request_processed = true;
        char *global_quote = get_json_value("Global Quote", data_cstr, MAX_TOKENS);
        if (global_quote == NULL)
        {
            FURI_LOG_E(TAG, "Failed to get Global Quote");
            fhttp.state = ISSUE;
            furi_string_free(price_data);
            free(global_quote);
            free(data_cstr);
            return NULL;
        }
        char *price = get_json_value("05. price", global_quote, MAX_TOKENS);
        if (price == NULL)
        {
            FURI_LOG_E(TAG, "Failed to get price");
            fhttp.state = ISSUE;
            furi_string_free(price_data);
            free(global_quote);
            free(price);
            free(data_cstr);
            return NULL;
        }
        // store the price "Asset: $price"
        snprintf(asset_price, 64, "%s: $%s", asset_names[asset_index], price);

        fhttp.state = IDLE;
        furi_string_free(price_data);
        free(global_quote);
        free(price);
        free(data_cstr);
    }
    return asset_price;
}

static void flip_trader_asset_switch_to_view(FlipTraderApp *app)
{
    flip_trader_generic_switch_to_view(app, "Fetching..", send_price_request, process_asset_price, 1, callback_to_assets_submenu, FlipTraderViewLoader);
}

void callback_submenu_choices(void *context, uint32_t index)
{
    FlipTraderApp *app = (FlipTraderApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipTraderApp is NULL");
        return;
    }
    switch (index)
    {
    // view the assets submenu
    case FlipTradeSubmenuIndexAssets:
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipTraderViewAssetsSubmenu);
        break;
    // view the about screen
    case FlipTraderSubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipTraderViewAbout);
        break;
    // view the wifi settings screen
    case FlipTraderSubmenuIndexSettings:
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipTraderViewWiFiSettings);
        break;
    default:
        // handle FlipTraderSubmenuIndexAssetStartIndex + index
        if (index >= FlipTraderSubmenuIndexAssetStartIndex)
        {
            asset_index = index - FlipTraderSubmenuIndexAssetStartIndex;
            flip_trader_asset_switch_to_view(app);
        }
        else
        {
            FURI_LOG_E(TAG, "Unknown submenu index");
        }
        break;
    }
}

void text_updated_ssid(void *context)
{
    FlipTraderApp *app = (FlipTraderApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipTraderApp is NULL");
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
    save_settings(app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_password);

    // save wifi settings to devboard
    if (strlen(app->uart_text_input_buffer_ssid) > 0 && strlen(app->uart_text_input_buffer_password) > 0)
    {
        if (!flipper_http_save_wifi(app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_password))
        {
            FURI_LOG_E(TAG, "Failed to save wifi settings");
        }
    }

    // switch to the settings view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipTraderViewWiFiSettings);
}

void text_updated_password(void *context)
{
    FlipTraderApp *app = (FlipTraderApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipTraderApp is NULL");
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
    save_settings(app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_password);

    // save wifi settings to devboard
    if (strlen(app->uart_text_input_buffer_ssid) > 0 && strlen(app->uart_text_input_buffer_password) > 0)
    {
        if (!flipper_http_save_wifi(app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_password))
        {
            FURI_LOG_E(TAG, "Failed to save wifi settings");
        }
    }

    // switch to the settings view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipTraderViewWiFiSettings);
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
    request_processed = false;
    asset_index = 0;
    return FlipTraderViewMainSubmenu;
}

uint32_t callback_to_wifi_settings(void *context)
{
    if (!context)
    {
        FURI_LOG_E(TAG, "Context is NULL");
        return VIEW_NONE;
    }
    UNUSED(context);
    return FlipTraderViewWiFiSettings;
}

uint32_t callback_to_assets_submenu(void *context)
{
    if (!context)
    {
        FURI_LOG_E(TAG, "Context is NULL");
        return VIEW_NONE;
    }
    UNUSED(context);
    sent_get_request = false;
    get_request_success = false;
    request_processed = false;
    asset_index = 0;
    return FlipTraderViewAssetsSubmenu;
}

void settings_item_selected(void *context, uint32_t index)
{
    FlipTraderApp *app = (FlipTraderApp *)context;
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipTraderApp is NULL");
        return;
    }
    switch (index)
    {
    case 0: // Input SSID
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipTraderViewTextInputSSID);
        break;
    case 1: // Input Password
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipTraderViewTextInputPassword);
        break;
    default:
        FURI_LOG_E(TAG, "Unknown configuration item index");
        break;
    }
}

static void flip_trader_widget_set_text(char *message, Widget **widget)
{
    if (widget == NULL)
    {
        FURI_LOG_E(TAG, "flip_trader_set_widget_text - widget is NULL");
        DEV_CRASH();
        return;
    }
    if (message == NULL)
    {
        FURI_LOG_E(TAG, "flip_trader_set_widget_text - message is NULL");
        DEV_CRASH();
        return;
    }
    widget_reset(*widget);

    uint32_t message_length = strlen(message); // Length of the message
    uint32_t i = 0;                            // Index tracker
    uint32_t formatted_index = 0;              // Tracker for where we are in the formatted message
    char *formatted_message;                   // Buffer to hold the final formatted message
    if (!easy_flipper_set_buffer(&formatted_message, message_length * 2 + 1))
    {
        return;
    }

    while (i < message_length)
    {
        // TODO: Use canvas_glyph_width to calculate the maximum characters for the line
        uint32_t max_line_length = 29;                  // Maximum characters per line
        uint32_t remaining_length = message_length - i; // Remaining characters
        uint32_t line_length = (remaining_length < max_line_length) ? remaining_length : max_line_length;

        // Temporary buffer to hold the current line
        char line[30];
        strncpy(line, message + i, line_length);
        line[line_length] = '\0';

        // Check if the line ends in the middle of a word and adjust accordingly
        if (line_length == 29 && message[i + line_length] != '\0' && message[i + line_length] != ' ')
        {
            // Find the last space within the 30-character segment
            char *last_space = strrchr(line, ' ');
            if (last_space != NULL)
            {
                // Adjust the line length to avoid cutting the word
                line_length = last_space - line;
                line[line_length] = '\0'; // Null-terminate at the space
            }
        }

        // Manually copy the fixed line into the formatted_message buffer
        for (uint32_t j = 0; j < line_length; j++)
        {
            formatted_message[formatted_index++] = line[j];
        }

        // Add a newline character for line spacing
        formatted_message[formatted_index++] = '\n';

        // Move i forward to the start of the next word
        i += line_length;

        // Skip spaces at the beginning of the next line
        while (message[i] == ' ')
        {
            i++;
        }
    }

    // Add the formatted message to the widget
    widget_add_text_scroll_element(*widget, 0, 0, 128, 64, formatted_message);
}

void flip_trader_loader_draw_callback(Canvas *canvas, void *model)
{
    if (!canvas || !model)
    {
        FURI_LOG_E(TAG, "flip_trader_loader_draw_callback - canvas or model is NULL");
        return;
    }

    SerialState http_state = fhttp.state;
    AssetLoaderModel *asset_loader_model = (AssetLoaderModel *)model;
    AssetState asset_state = asset_loader_model->asset_state;
    char *title = asset_loader_model->title;

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

    if (asset_state == AssetStateError || asset_state == AssetStateParseError)
    {
        flip_trader_request_error_draw(canvas);
        return;
    }

    canvas_draw_str(canvas, 0, 7, title);
    canvas_draw_str(canvas, 0, 17, "Loading...");

    if (asset_state == AssetStateInitial)
    {
        return;
    }

    if (http_state == SENDING)
    {
        canvas_draw_str(canvas, 0, 27, "Sending...");
        return;
    }

    if (http_state == RECEIVING || asset_state == AssetStateRequested)
    {
        canvas_draw_str(canvas, 0, 27, "Receiving...");
        return;
    }

    if (http_state == IDLE && asset_state == AssetStateReceived)
    {
        canvas_draw_str(canvas, 0, 27, "Processing...");
        return;
    }

    if (http_state == IDLE && asset_state == AssetStateParsed)
    {
        canvas_draw_str(canvas, 0, 27, "Processed...");
        return;
    }
}

static void flip_trader_loader_process_callback(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "flip_trader_loader_process_callback - context is NULL");
        DEV_CRASH();
        return;
    }

    FlipTraderApp *app = (FlipTraderApp *)context;
    View *view = app->view_loader;

    AssetState current_asset_state;
    with_view_model(view, AssetLoaderModel * model, { current_asset_state = model->asset_state; }, false);

    if (current_asset_state == AssetStateInitial)
    {
        with_view_model(
            view,
            AssetLoaderModel * model,
            {
                model->asset_state = AssetStateRequested;
                AssetLoaderFetch fetch = model->fetcher;
                if (fetch == NULL)
                {
                    FURI_LOG_E(TAG, "Model doesn't have Fetch function assigned.");
                    model->asset_state = AssetStateError;
                    return;
                }

                // Clear any previous responses
                strncpy(fhttp.last_response, "", 1);
                bool request_status = fetch(model);
                if (!request_status)
                {
                    model->asset_state = AssetStateError;
                }
            },
            true);
    }
    else if (current_asset_state == AssetStateRequested || current_asset_state == AssetStateError)
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
                with_view_model(view, AssetLoaderModel * model, { model->asset_state = AssetStateReceived; }, true);
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
            with_view_model(view, AssetLoaderModel * model, { model->asset_state = AssetStateError; }, true);
        }
        else
        {
            FURI_LOG_DEV(TAG, "Unexpected state: %d lastresp: %s", fhttp.state, fhttp.last_response ? fhttp.last_response : "NULL");
            DEV_CRASH();
        }
    }
    else if (current_asset_state == AssetStateReceived)
    {
        with_view_model(
            view,
            AssetLoaderModel * model,
            {
                char *asset_text;
                if (model->parser == NULL)
                {
                    asset_text = NULL;
                    FURI_LOG_DEV(TAG, "Parser is NULL");
                    DEV_CRASH();
                }
                else
                {
                    asset_text = model->parser(model);
                }
                FURI_LOG_DEV(TAG, "Parsed asset: %s\r\ntext: %s", fhttp.last_response ? fhttp.last_response : "NULL", asset_text ? asset_text : "NULL");
                model->asset_text = asset_text;
                if (asset_text == NULL)
                {
                    model->asset_state = AssetStateParseError;
                }
                else
                {
                    model->asset_state = AssetStateParsed;
                }
            },
            true);
    }
    else if (current_asset_state == AssetStateParsed)
    {
        with_view_model(
            view,
            AssetLoaderModel * model,
            {
                if (++model->request_index < model->request_count)
                {
                    model->asset_state = AssetStateInitial;
                }
                else
                {
                    flip_trader_widget_set_text(model->asset_text != NULL ? model->asset_text : "Empty result", &app_instance->widget_result);
                    if (model->asset_text != NULL)
                    {
                        free(model->asset_text);
                        model->asset_text = NULL;
                    }
                    view_set_previous_callback(widget_get_view(app_instance->widget_result), model->back_callback);
                    view_dispatcher_switch_to_view(app_instance->view_dispatcher, FlipTraderViewWidgetResult);
                }
            },
            true);
    }
}

static void flip_trader_loader_timer_callback(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "flip_trader_loader_timer_callback - context is NULL");
        DEV_CRASH();
        return;
    }
    FlipTraderApp *app = (FlipTraderApp *)context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FlipTraderCustomEventProcess);
}

static void flip_trader_loader_on_enter(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "flip_trader_loader_on_enter - context is NULL");
        DEV_CRASH();
        return;
    }
    FlipTraderApp *app = (FlipTraderApp *)context;
    View *view = app->view_loader;
    with_view_model(
        view,
        AssetLoaderModel * model,
        {
            view_set_previous_callback(view, model->back_callback);
            if (model->timer == NULL)
            {
                model->timer = furi_timer_alloc(flip_trader_loader_timer_callback, FuriTimerTypePeriodic, app);
            }
            furi_timer_start(model->timer, 250);
        },
        true);
}

static void flip_trader_loader_on_exit(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "flip_trader_loader_on_exit - context is NULL");
        DEV_CRASH();
        return;
    }
    FlipTraderApp *app = (FlipTraderApp *)context;
    View *view = app->view_loader;
    with_view_model(
        view,
        AssetLoaderModel * model,
        {
            if (model->timer)
            {
                furi_timer_stop(model->timer);
            }
        },
        false);
}

void flip_trader_loader_init(View *view)
{
    if (view == NULL)
    {
        FURI_LOG_E(TAG, "flip_trader_loader_init - view is NULL");
        DEV_CRASH();
        return;
    }
    view_allocate_model(view, ViewModelTypeLocking, sizeof(AssetLoaderModel));
    view_set_enter_callback(view, flip_trader_loader_on_enter);
    view_set_exit_callback(view, flip_trader_loader_on_exit);
}

void flip_trader_loader_free_model(View *view)
{
    if (view == NULL)
    {
        FURI_LOG_E(TAG, "flip_trader_loader_free_model - view is NULL");
        DEV_CRASH();
        return;
    }
    with_view_model(
        view,
        AssetLoaderModel * model,
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

bool flip_trader_custom_event_callback(void *context, uint32_t index)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "flip_trader_custom_event_callback - context is NULL");
        DEV_CRASH();
        return false;
    }

    switch (index)
    {
    case FlipTraderCustomEventProcess:
        flip_trader_loader_process_callback(context);
        return true;
    default:
        FURI_LOG_DEV(TAG, "flip_trader_custom_event_callback. Unknown index: %ld", index);
        return false;
    }
}

void flip_trader_generic_switch_to_view(FlipTraderApp *app, char *title, AssetLoaderFetch fetcher, AssetLoaderParser parser, size_t request_count, ViewNavigationCallback back, uint32_t view_id)
{
    if (app == NULL)
    {
        FURI_LOG_E(TAG, "flip_trader_generic_switch_to_view - app is NULL");
        DEV_CRASH();
        return;
    }

    View *view = app->view_loader;
    if (view == NULL)
    {
        FURI_LOG_E(TAG, "flip_trader_generic_switch_to_view - view is NULL");
        DEV_CRASH();
        return;
    }

    with_view_model(
        view,
        AssetLoaderModel * model,
        {
            model->title = title;
            model->fetcher = fetcher;
            model->parser = parser;
            model->request_index = 0;
            model->request_count = request_count;
            model->back_callback = back;
            model->asset_state = AssetStateInitial;
            model->asset_text = NULL;
        },
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, view_id);
}
