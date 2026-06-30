#include <callback/web_crawler_callback.h>
#include <storage/storage.h>
#include <html/html_furi.h>

// Below added by Derek Jamison
// FURI_LOG_DEV will log only during app development. Be sure that Settings/System/Log Device is "LPUART"; so we dont use serial port.
#ifdef DEVELOPMENT
#define FURI_LOG_DEV(tag, format, ...) furi_log_print_format(FuriLogLevelInfo, tag, format, ##__VA_ARGS__)
#define DEV_CRASH() furi_crash()
#else
#define FURI_LOG_DEV(tag, format, ...)
#define DEV_CRASH()
#endif
static uint32_t web_crawler_back_to_file_callback(void *context);
static bool alloc_widget(WebCrawlerApp *app, uint32_t view)
{
    furi_check(app, "alloc_widget: app is NULL");
    if (!app->widget)
    {
        switch (view)
        {
        case WebCrawlerViewAbout:
            return easy_flipper_set_widget(&app->widget, WebCrawlerViewWidget, "Web Crawler App\n---\nBrowse the web, fetch API data, and more..\n---\nVisit github.com/jblanked for more details.\n---\nPress BACK to return.", web_crawler_back_to_main_callback, &app->view_dispatcher);
        case WebCrawlerViewFileRead:
            return easy_flipper_set_widget(&app->widget, WebCrawlerViewWidget, "Data will be displayed here.", web_crawler_back_to_file_callback, &app->view_dispatcher);
        case WebCrawlerViewFileDelete:
            return easy_flipper_set_widget(&app->widget, WebCrawlerViewWidget, "File deleted.", web_crawler_back_to_file_callback, &app->view_dispatcher);
        }
    }
    return false;
}
static void free_widget(WebCrawlerApp *app)
{
    if (app->widget)
    {
        view_dispatcher_remove_view(app->view_dispatcher, WebCrawlerViewWidget);
        free(app->widget);
        app->widget = NULL;
    }
}
static bool alloc_submenu_config(WebCrawlerApp *app)
{
    furi_check(app, "alloc_submenu_config: WebCrawlerApp is NULL");
    if (app->submenu_config)
    {
        FURI_LOG_E(TAG, "alloc_submenu_config: Submenu already allocated");
        return false;
    }
    if (easy_flipper_set_submenu(&app->submenu_config, WebCrawlerViewSubmenuConfig, "Settings", web_crawler_back_to_main_callback, &app->view_dispatcher))
    {
        submenu_add_item(app->submenu_config, "WiFi", WebCrawlerSubmenuIndexWifi, web_crawler_submenu_callback, app);
        submenu_add_item(app->submenu_config, "File", WebCrawlerSubmenuIndexFile, web_crawler_submenu_callback, app);
        submenu_add_item(app->submenu_config, "Request", WebCrawlerSubmenuIndexRequest, web_crawler_submenu_callback, app);
        return true;
    }
    return false;
}
static void free_submenu_config(WebCrawlerApp *app)
{
    furi_check(app, "free_submenu_config: WebCrawlerApp is NULL");
    if (app->submenu_config)
    {
        view_dispatcher_remove_view(app->view_dispatcher, WebCrawlerViewSubmenuConfig);
        submenu_free(app->submenu_config);
        app->submenu_config = NULL;
    }
}

static bool alloc_variable_item_list(WebCrawlerApp *app, uint32_t view)
{
    furi_check(app, "alloc_variable_item_list: WebCrawlerApp is NULL");
    if (app->variable_item_list)
    {
        FURI_LOG_E(TAG, "Variable Item List already allocated");
        return false;
    }
    bool settings_loaded = true;
    // load settings
    char path[128];
    char ssid[64];
    char password[64];
    char file_rename[128];
    char file_type[16];
    char http_method[16];
    char headers[256];
    char payload[256];
    if (!load_settings(path, 128, ssid, 64, password, 64, file_rename, 128, file_type, 16, http_method, 16, headers, 256, payload, 256, app))
    {
        FURI_LOG_E(TAG, "Failed to load settings");
        settings_loaded = false;
    }
    switch (view)
    {
    case WebCrawlerViewVariableItemListWifi:
        if (!easy_flipper_set_variable_item_list(&app->variable_item_list, WebCrawlerViewVariableItemList, web_crawler_wifi_enter_callback, web_crawler_back_to_configure_callback, &app->view_dispatcher, app))
        {
            return false;
        }
        if (!app->ssid_item)
        {
            app->ssid_item = variable_item_list_add(app->variable_item_list, "SSID", 0, NULL, NULL); // index 0
            variable_item_set_current_value_text(app->ssid_item, "");                                // Initialize
        }
        if (!app->password_item)
        {
            app->password_item = variable_item_list_add(app->variable_item_list, "Password", 0, NULL, NULL); // index 1
            variable_item_set_current_value_text(app->password_item, "");                                    // Initialize
        }
        if (settings_loaded)
        {
            variable_item_set_current_value_text(app->ssid_item, ssid);
            // variable_item_set_current_value_text(app->password_item, password);
        }
        else
        {
            variable_item_set_current_value_text(app->ssid_item, ""); // Initialize
                                                                      // variable_item_set_current_value_text(app->password_item, "wifi-Password"); // Initialize

            snprintf(ssid, 64, "%s", "wifi-SSID");
            snprintf(password, 64, "%s", "wifi-Password");
        }
        // save for updating temp buffers later
        save_char("wifi-ssid", ssid);
        save_char("wifi-password", password);
        // strncpy(app->temp_buffer_ssid, app->ssid, app->temp_buffer_size_ssid - 1);
        // app->temp_buffer_ssid[app->temp_buffer_size_ssid - 1] = '\0';
        // strncpy(app->temp_buffer_password, app->password, app->temp_buffer_size_password - 1);
        // app->temp_buffer_password[app->temp_buffer_size_password - 1] = '\0';
        break;
    case WebCrawlerViewVariableItemListFile:
        if (!easy_flipper_set_variable_item_list(&app->variable_item_list, WebCrawlerViewVariableItemList, web_crawler_file_enter_callback, web_crawler_back_to_configure_callback, &app->view_dispatcher, app))
        {
            return false;
        }
        if (!app->file_read_item)
        {
            app->file_read_item = variable_item_list_add(app->variable_item_list, "Read File", 0, NULL, NULL); // index 0
            variable_item_set_current_value_text(app->file_read_item, "");                                     // Initialize
        }
        if (!app->file_type_item)
        {
            app->file_type_item = variable_item_list_add(app->variable_item_list, "Set File Type", 0, NULL, NULL); // index 1
            variable_item_set_current_value_text(app->file_type_item, "");                                         // Initialize
        }
        if (!app->file_rename_item)
        {
            app->file_rename_item = variable_item_list_add(app->variable_item_list, "Rename File", 0, NULL, NULL); // index 2
            variable_item_set_current_value_text(app->file_rename_item, "");                                       // Initialize
        }
        if (!app->file_delete_item)
        {
            app->file_delete_item = variable_item_list_add(app->variable_item_list, "Delete File", 0, NULL, NULL); // index 3
            variable_item_set_current_value_text(app->file_delete_item, "");                                       // Initialize
        }
        if (settings_loaded)
        {
            variable_item_set_current_value_text(app->file_type_item, file_type);
            variable_item_set_current_value_text(app->file_rename_item, file_rename);
        }
        else
        {
            variable_item_set_current_value_text(app->file_type_item, ".txt");            // Initialize
            variable_item_set_current_value_text(app->file_rename_item, "received_data"); // Initialize

            snprintf(file_type, 16, "%s", ".txt");
            snprintf(file_rename, 128, "%s", "received_data");
        }
        // save for updating temp buffers later
        save_char("file_type", file_type);
        save_char("file_rename", file_rename);
        // strncpy(app->temp_buffer_file_type, app->file_type, app->temp_buffer_size_file_type - 1);
        // app->temp_buffer_file_type[app->temp_buffer_size_file_type - 1] = '\0';
        // strncpy(app->temp_buffer_file_rename, app->file_rename, app->temp_buffer_size_file_rename - 1);
        // app->temp_buffer_file_rename[app->temp_buffer_size_file_rename - 1] = '\0';
        break;
    case WebCrawlerViewVariableItemListRequest:
        if (!easy_flipper_set_variable_item_list(&app->variable_item_list, WebCrawlerViewVariableItemList, web_crawler_request_enter_callback, web_crawler_back_to_configure_callback, &app->view_dispatcher, app))
        {
            return false;
        }
        if (!app->path_item)
        {
            app->path_item = variable_item_list_add(app->variable_item_list, "Path", 0, NULL, NULL);
            variable_item_set_current_value_text(app->path_item, ""); // Initialize
        }
        if (!app->http_method_item)
        {
            app->http_method_item = variable_item_list_add(app->variable_item_list, "HTTP Method", 6, web_crawler_http_method_change, app);
            variable_item_set_current_value_text(app->http_method_item, ""); // Initialize
            variable_item_set_current_value_index(app->http_method_item, 0); // Initialize
        }
        if (!app->headers_item)
        {
            app->headers_item = variable_item_list_add(app->variable_item_list, "Headers", 0, NULL, NULL);
            variable_item_set_current_value_text(app->headers_item, ""); // Initialize
        }
        if (!app->payload_item)
        {
            app->payload_item = variable_item_list_add(app->variable_item_list, "Payload", 0, NULL, NULL);
            variable_item_set_current_value_text(app->payload_item, ""); // Initialize
        }
        //
        //
        if (settings_loaded)
        {
            variable_item_set_current_value_text(app->path_item, path);
            variable_item_set_current_value_text(app->http_method_item, http_method);
            variable_item_set_current_value_text(app->headers_item, headers);
            variable_item_set_current_value_text(app->payload_item, payload);
            //
            variable_item_set_current_value_index(
                app->http_method_item,
                strstr(http_method, "GET") != NULL ? 0 : strstr(http_method, "POST") != NULL   ? 1
                                                     : strstr(http_method, "PUT") != NULL      ? 2
                                                     : strstr(http_method, "DELETE") != NULL   ? 3
                                                     : strstr(http_method, "DOWNLOAD") != NULL ? 4
                                                     : strstr(http_method, "BROWSE") != NULL   ? 5
                                                                                               : 0);
        }
        else
        {
            variable_item_set_current_value_text(app->path_item, "https://httpbin.org/get");                     // Initialize
            variable_item_set_current_value_text(app->http_method_item, "GET");                                  // Initialize
            variable_item_set_current_value_text(app->headers_item, "{\"Content-Type\": \"application/json\"}"); // Initialize
            variable_item_set_current_value_text(app->payload_item, "{\"key\": \"value\"}");                     // Initialize

            snprintf(path, 128, "%s", "https://httpbin.org/get");
            snprintf(http_method, 16, "%s", "GET");
            snprintf(headers, 256, "%s", "{\"Content-Type\": \"application/json\"}");
            snprintf(payload, 256, "%s", "{\"key\": \"value\"}");
        }
        // save for updating temp buffers later
        save_char("path", path);
        save_char("http_method", http_method);
        save_char("headers", headers);
        save_char("payload", payload);
        // strncpy(app->temp_buffer_path, app->path, app->temp_buffer_size_path - 1);
        // app->temp_buffer_path[app->temp_buffer_size_path - 1] = '\0';
        // strncpy(app->temp_buffer_http_method, app->http_method, app->temp_buffer_size_http_method - 1);
        // app->temp_buffer_http_method[app->temp_buffer_size_http_method - 1] = '\0';
        // strncpy(app->temp_buffer_headers, app->headers, app->temp_buffer_size_headers - 1);
        // app->temp_buffer_headers[app->temp_buffer_size_headers - 1] = '\0';
        // strncpy(app->temp_buffer_payload, app->payload, app->temp_buffer_size_payload - 1);
        // app->temp_buffer_payload[app->temp_buffer_size_payload - 1] = '\0';
        break;
    default:
        FURI_LOG_E(TAG, "Invalid view");
        return false;
    }
    return true;
}
static void free_variable_item_list(WebCrawlerApp *app)
{
    furi_check(app, "free_variable_item_list: WebCrawlerApp is NULL");
    if (app->variable_item_list)
    {
        view_dispatcher_remove_view(app->view_dispatcher, WebCrawlerViewVariableItemList);
        variable_item_list_free(app->variable_item_list);
        app->variable_item_list = NULL;
    }
    // check and free variable items
    if (app->ssid_item)
    {
        free(app->ssid_item);
        app->ssid_item = NULL;
    }
    if (app->password_item)
    {
        free(app->password_item);
        app->password_item = NULL;
    }
    if (app->file_type_item)
    {
        free(app->file_type_item);
        app->file_type_item = NULL;
    }
    if (app->file_rename_item)
    {
        free(app->file_rename_item);
        app->file_rename_item = NULL;
    }
    if (app->file_read_item)
    {
        free(app->file_read_item);
        app->file_read_item = NULL;
    }
    if (app->file_delete_item)
    {
        free(app->file_delete_item);
        app->file_delete_item = NULL;
    }
    if (app->path_item)
    {
        free(app->path_item);
        app->path_item = NULL;
    }
    if (app->http_method_item)
    {
        free(app->http_method_item);
        app->http_method_item = NULL;
    }
    if (app->headers_item)
    {
        free(app->headers_item);
        app->headers_item = NULL;
    }
    if (app->payload_item)
    {
        free(app->payload_item);
        app->payload_item = NULL;
    }
}
static bool alloc_text_input(WebCrawlerApp *app, uint32_t view)
{
    furi_check(app, "alloc_text_input: WebCrawlerApp is NULL");
    if (app->uart_text_input)
    {
        FURI_LOG_E(TAG, "Text Input already allocated");
        return false;
    }
    switch (view)
    {
    case WebCrawlerViewTextInput:
        app->temp_buffer_size_path = 128;
        if (!easy_flipper_set_buffer(&app->temp_buffer_path, app->temp_buffer_size_path) || !easy_flipper_set_buffer(&app->path, app->temp_buffer_size_path))
        {
            return false;
        }
        if (!easy_flipper_set_uart_text_input(&app->uart_text_input, WebCrawlerViewInput, "Enter URL", app->temp_buffer_path, app->temp_buffer_size_path, web_crawler_set_path_updated, web_crawler_back_to_request_callback, &app->view_dispatcher, app))
        {
            return false;
        }
        if (load_char("path", app->path, app->temp_buffer_size_path))
        {
            snprintf(app->temp_buffer_path, app->temp_buffer_size_path, "%s", app->path);
        }
        break;
    case WebCrawlerViewTextInputSSID:
        app->temp_buffer_size_ssid = 64;
        if (!easy_flipper_set_buffer(&app->temp_buffer_ssid, app->temp_buffer_size_ssid) || !easy_flipper_set_buffer(&app->ssid, app->temp_buffer_size_ssid))
        {
            return false;
        }
        if (!easy_flipper_set_uart_text_input(&app->uart_text_input, WebCrawlerViewInput, "Enter SSID", app->temp_buffer_ssid, app->temp_buffer_size_ssid, web_crawler_set_ssid_updated, web_crawler_back_to_wifi_callback, &app->view_dispatcher, app))
        {
            return false;
        }
        if (load_char("wifi-ssid", app->ssid, app->temp_buffer_size_ssid))
        {
            snprintf(app->temp_buffer_ssid, app->temp_buffer_size_ssid, "%s", app->ssid);
        }
        break;
    case WebCrawlerViewTextInputPassword:
        app->temp_buffer_size_password = 64;
        if (!easy_flipper_set_buffer(&app->temp_buffer_password, app->temp_buffer_size_password) || !easy_flipper_set_buffer(&app->password, app->temp_buffer_size_password))
        {
            return false;
        }
        if (!easy_flipper_set_uart_text_input(&app->uart_text_input, WebCrawlerViewInput, "Enter Password", app->temp_buffer_password, app->temp_buffer_size_password, web_crawler_set_password_update, web_crawler_back_to_wifi_callback, &app->view_dispatcher, app))
        {
            return false;
        }
        if (load_char("wifi-password", app->password, app->temp_buffer_size_password))
        {
            snprintf(app->temp_buffer_password, app->temp_buffer_size_password, "%s", app->password);
        }
        break;
    case WebCrawlerViewTextInputFileType:
        app->temp_buffer_size_file_type = 16;
        if (!easy_flipper_set_buffer(&app->temp_buffer_file_type, app->temp_buffer_size_file_type) || !easy_flipper_set_buffer(&app->file_type, app->temp_buffer_size_file_type))
        {
            return false;
        }
        if (!easy_flipper_set_uart_text_input(&app->uart_text_input, WebCrawlerViewInput, "Enter File Type", app->temp_buffer_file_type, app->temp_buffer_size_file_type, web_crawler_set_file_type_update, web_crawler_back_to_file_callback, &app->view_dispatcher, app))
        {
            return false;
        }
        if (load_char("file_type", app->file_type, app->temp_buffer_size_file_type))
        {
            snprintf(app->temp_buffer_file_type, app->temp_buffer_size_file_type, "%s", app->file_type);
        }
        break;
    case WebCrawlerViewTextInputFileRename:
        app->temp_buffer_size_file_rename = 128;
        if (!easy_flipper_set_buffer(&app->temp_buffer_file_rename, app->temp_buffer_size_file_rename) || !easy_flipper_set_buffer(&app->file_rename, app->temp_buffer_size_file_rename))
        {
            return false;
        }
        if (!easy_flipper_set_uart_text_input(&app->uart_text_input, WebCrawlerViewInput, "Enter File Rename", app->temp_buffer_file_rename, app->temp_buffer_size_file_rename, web_crawler_set_file_rename_update, web_crawler_back_to_file_callback, &app->view_dispatcher, app))
        {
            return false;
        }
        if (load_char("file_rename", app->file_rename, app->temp_buffer_size_file_rename))
        {
            snprintf(app->temp_buffer_file_rename, app->temp_buffer_size_file_rename, "%s", app->file_rename);
        }
        break;
    case WebCrawlerViewTextInputHeaders:
        app->temp_buffer_size_headers = 256;
        if (!easy_flipper_set_buffer(&app->temp_buffer_headers, app->temp_buffer_size_headers) || !easy_flipper_set_buffer(&app->headers, app->temp_buffer_size_headers))
        {
            return false;
        }
        if (!easy_flipper_set_uart_text_input(&app->uart_text_input, WebCrawlerViewInput, "Enter Headers", app->temp_buffer_headers, app->temp_buffer_size_headers, web_crawler_set_headers_updated, web_crawler_back_to_request_callback, &app->view_dispatcher, app))
        {
            return false;
        }
        if (load_char("headers", app->headers, app->temp_buffer_size_headers))
        {
            snprintf(app->temp_buffer_headers, app->temp_buffer_size_headers, "%s", app->headers);
        }
        break;
    case WebCrawlerViewTextInputPayload:
        app->temp_buffer_size_payload = 256;
        if (!easy_flipper_set_buffer(&app->temp_buffer_payload, app->temp_buffer_size_payload) || !easy_flipper_set_buffer(&app->payload, app->temp_buffer_size_payload))
        {
            return false;
        }
        if (!easy_flipper_set_uart_text_input(&app->uart_text_input, WebCrawlerViewInput, "Enter Payload", app->temp_buffer_payload, app->temp_buffer_size_payload, web_crawler_set_payload_updated, web_crawler_back_to_request_callback, &app->view_dispatcher, app))
        {
            return false;
        }
        if (load_char("payload", app->payload, app->temp_buffer_size_payload))
        {
            snprintf(app->temp_buffer_payload, app->temp_buffer_size_payload, "%s", app->payload);
        }
        break;
    default:
        FURI_LOG_E(TAG, "Invalid view");
        return false;
    }
    return true;
}
static void free_text_input(WebCrawlerApp *app)
{
    furi_check(app, "free_text_input: WebCrawlerApp is NULL");
    if (app->uart_text_input)
    {
        view_dispatcher_remove_view(app->view_dispatcher, WebCrawlerViewInput);
        text_input_free(app->uart_text_input);
        app->uart_text_input = NULL;
    }
    // check and free path
    if (app->temp_buffer_path)
    {
        free(app->temp_buffer_path);
        app->temp_buffer_path = NULL;
    }
    if (app->path)
    {
        free(app->path);
        app->path = NULL;
    }
    // check and free ssid
    if (app->temp_buffer_ssid)
    {
        free(app->temp_buffer_ssid);
        app->temp_buffer_ssid = NULL;
    }
    if (app->ssid)
    {
        free(app->ssid);
        app->ssid = NULL;
    }
    // check and free password
    if (app->temp_buffer_password)
    {
        free(app->temp_buffer_password);
        app->temp_buffer_password = NULL;
    }
    if (app->password)
    {
        free(app->password);
        app->password = NULL;
    }
    // check and free file type
    if (app->temp_buffer_file_type)
    {
        free(app->temp_buffer_file_type);
        app->temp_buffer_file_type = NULL;
    }
    if (app->file_type)
    {
        free(app->file_type);
        app->file_type = NULL;
    }
    // check and free file rename
    if (app->temp_buffer_file_rename)
    {
        free(app->temp_buffer_file_rename);
        app->temp_buffer_file_rename = NULL;
    }
    if (app->file_rename)
    {
        free(app->file_rename);
        app->file_rename = NULL;
    }
    // check and free headers
    if (app->temp_buffer_headers)
    {
        free(app->temp_buffer_headers);
        app->temp_buffer_headers = NULL;
    }
    if (app->headers)
    {
        free(app->headers);
        app->headers = NULL;
    }
    // check and free payload
    if (app->temp_buffer_payload)
    {
        free(app->temp_buffer_payload);
        app->temp_buffer_payload = NULL;
    }
    if (app->payload)
    {
        free(app->payload);
        app->payload = NULL;
    }
}

void free_all(WebCrawlerApp *app)
{
    furi_check(app, "free_all: app is NULL");
    free_widget(app);
    free_submenu_config(app);
    free_variable_item_list(app);
    free_text_input(app);
}
static void web_crawler_draw_error(Canvas *canvas, DataLoaderModel *model)
{
    furi_check(model, "web_crawler_draw_error: DataLoaderModel is NULL");
    furi_check(model->fhttp, "web_crawler_draw_error: FlipperHTTP is NULL");
    furi_check(canvas, "Canvas is NULL");
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    if (model->fhttp->state == INACTIVE)
    {
        canvas_draw_str(canvas, 0, 7, "Wifi Dev Board disconnected.");
        canvas_draw_str(canvas, 0, 17, "Please connect to the board.");
        canvas_draw_str(canvas, 0, 32, "If your board is connected,");
        canvas_draw_str(canvas, 0, 42, "make sure you have flashed");
        canvas_draw_str(canvas, 0, 52, "your WiFi Devboard with the");
        canvas_draw_str(canvas, 0, 62, "latest FlipperHTTP flash.");
        return;
    }

    if (model->fhttp->last_response)
    {
        if (strstr(model->fhttp->last_response, "[ERROR] Not connected to Wifi. Failed to reconnect.") != NULL)
        {
            canvas_draw_str(canvas, 0, 10, "[ERROR] Not connected to Wifi.");
            canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
            canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
            return;
        }
        if (strstr(model->fhttp->last_response, "[ERROR] Failed to connect to Wifi.") != NULL)
        {
            canvas_draw_str(canvas, 0, 10, "[ERROR] Not connected to Wifi.");
            canvas_draw_str(canvas, 0, 50, "Update your WiFi settings.");
            canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
            return;
        }
        if (strstr(model->fhttp->last_response, "request failed with error: connection refused") != NULL)
        {
            canvas_draw_str(canvas, 0, 10, "[ERROR] Connection refused.");
            canvas_draw_str(canvas, 0, 50, "Choose another URL.");
            canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
            return;
        }
        if (strstr(model->fhttp->last_response, "[PONG]") != NULL)
        {
            canvas_clear(canvas);
            canvas_draw_str(canvas, 0, 10, "[STATUS]Connecting to AP...");
            return;
        }
        // handle failed requests
        if (strstr(model->fhttp->last_response, "request failed or returned empty data.") != NULL)
        {
            canvas_draw_str(canvas, 0, 10, "[ERROR] Request failed.");
            canvas_draw_str(canvas, 0, 50, "If this is your third attempt,");
            canvas_draw_str(canvas, 0, 60, "it's likely your URL is not");
            canvas_draw_str(canvas, 0, 70, "compabilbe or correct.");
            canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
            return;
        }

        canvas_draw_str(canvas, 0, 10, "[ERROR] Failed to sync data.");
        canvas_draw_str(canvas, 0, 30, "If this is your third attempt,");
        canvas_draw_str(canvas, 0, 40, "it's likely your URL is not");
        canvas_draw_str(canvas, 0, 50, "compabilbe or correct.");
        canvas_draw_str(canvas, 0, 60, "Press BACK to return.");
        return;
    }

    canvas_draw_str(canvas, 0, 10, "HTTP request failed.");
    canvas_draw_str(canvas, 0, 20, "Press BACK to return.");
}

static void save_simply()
{
    char path[128];
    char ssid[64];
    char password[64];
    char file_rename[128];
    char file_type[16];
    char http_method[16];
    char headers[256];
    char payload[256];

    if (!load_char("path", path, 128))
    {
        snprintf(path, 128, "%s", "https://httpbin.org/get");
    }
    if (!load_char("wifi-ssid", ssid, 64))
    {
        snprintf(ssid, 64, "%s", "WIFI-SSID");
    }
    if (!load_char("wifi-password", password, 64))
    {
        snprintf(password, 64, "%s", "wifi-Password");
    }
    if (!load_char("file_rename", file_rename, 128))
    {
        snprintf(file_rename, 128, "%s", "received_data");
    }
    if (!load_char("file_type", file_type, 16))
    {
        snprintf(file_type, 16, "%s", ".txt");
    }
    if (!load_char("http_method", http_method, 16))
    {
        snprintf(http_method, 16, "%s", "GET");
    }
    if (!load_char("headers", headers, 256))
    {
        snprintf(headers, 256, "%s", "{\"Content-Type\": \"application/json\"}");
    }
    if (!load_char("payload", payload, 256))
    {
        snprintf(payload, 256, "%s", "{\"key\": \"value\"}");
    }

    save_settings(path, ssid, password, file_rename, file_type, http_method, headers, payload);
}

void web_crawler_http_method_change(VariableItem *item)
{
    WebCrawlerApp *app = (WebCrawlerApp *)variable_item_get_context(item);
    furi_check(app, "web_crawler_http_method_change: WebCrawlerApp is NULL");
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, http_method_names[index]);
    variable_item_set_current_value_index(item, index);

    // save the http method
    save_char("http_method", http_method_names[index]);
    save_simply();
}

static bool web_crawler_fetch(DataLoaderModel *model)
{
    WebCrawlerApp *app = (WebCrawlerApp *)model->parser_context;
    furi_check(app, "web_crawler_fetch: WebCrawlerApp is NULL");
    furi_check(model->fhttp, "web_crawler_fetch: FlipperHTTP is NULL");
    char url[128];
    if (!load_char("path", url, 128))
    {
        easy_flipper_dialog("Error", "Failed to load URL.\nGo into Settings -> Request\n and enter a Path.");
        return false;
    }
    char file_type[16];
    if (!load_char("file_type", file_type, 16))
    {
        easy_flipper_dialog("Error", "Failed to load file type.\nGo into settings and\nre-save the file type.");
        return false;
    }
    char file_rename[128];
    if (!load_char("file_rename", file_rename, 128))
    {
        easy_flipper_dialog("Error", "Failed to load file rename.\nGo into Settings -> File\n and Rename the file.");
        return false;
    }
    char http_method[16];
    if (!load_char("http_method", http_method, 16))
    {
        easy_flipper_dialog("Error", "Failed to load http method.\nGo into Settings -> Request\n and select an HTTP Method.");
        return false;
    }
    char headers[256];
    if (!load_char("headers", headers, 256))
    {
        easy_flipper_dialog("Error", "Failed to load headers.\nGo into Settings -> Request\n and add Headers.");
        return false;
    }
    char payload[256];
    if (!load_char("payload", payload, 256))
    {
        easy_flipper_dialog("Error", "Failed to load payload.\nGo into Settings -> Request\n and add a Payload.");
        return false;
    }

    if (strlen(file_rename) > 0 && strlen(file_type) > 0)
    {
        snprintf(
            model->fhttp->file_path,
            sizeof(model->fhttp->file_path),
            STORAGE_EXT_PATH_PREFIX "/apps_data/web_crawler/%s%s",
            file_rename,
            file_type);
    }
    else
    {
        snprintf(
            model->fhttp->file_path,
            sizeof(model->fhttp->file_path),
            STORAGE_EXT_PATH_PREFIX "/apps_data/web_crawler/received_data.txt");
    }

    if (strstr(http_method, "GET") != NULL)
    {
        model->fhttp->save_received_data = true;
        model->fhttp->is_bytes_request = false;

        // Perform GET request and handle the response
        if (strlen(headers) == 0)
        {
            return flipper_http_get_request(model->fhttp, url);
        }
        else
        {
            return flipper_http_get_request_with_headers(model->fhttp, url, headers);
        }
    }
    else if (strstr(http_method, "POST") != NULL)
    {
        model->fhttp->save_received_data = true;
        model->fhttp->is_bytes_request = false;

        // Perform POST request and handle the response
        return flipper_http_post_request_with_headers(model->fhttp, url, headers, payload);
    }
    else if (strstr(http_method, "PUT") != NULL)
    {
        model->fhttp->save_received_data = true;
        model->fhttp->is_bytes_request = false;

        // Perform PUT request and handle the response
        return flipper_http_put_request_with_headers(model->fhttp, url, headers, payload);
    }
    else if (strstr(http_method, "DELETE") != NULL)
    {
        model->fhttp->save_received_data = true;
        model->fhttp->is_bytes_request = false;

        // Perform DELETE request and handle the response
        return flipper_http_delete_request_with_headers(model->fhttp, url, headers, payload);
    }
    else if (strstr(http_method, "DOWNLOAD") != NULL)
    {
        model->fhttp->save_received_data = false;
        model->fhttp->is_bytes_request = true;

        // Perform GET request and handle the response
        return flipper_http_get_request_bytes(model->fhttp, url, "{\"Content-Type\": \"application/octet-stream\"}");
    }
    else // BROWSE
    {
        model->fhttp->save_received_data = false;
        model->fhttp->is_bytes_request = true;

        // download HTML response since the html could be large
        return flipper_http_get_request_bytes(model->fhttp, url, "{\"Content-Type\": \"application/octet-stream\"}");
    }
    return false;
}

static char *web_crawler_parse(DataLoaderModel *model)
{
    UNUSED(model);
    // parse HTML response if BROWSE request
    char http_method[16];
    if (!load_char("http_method", http_method, 16))
    {
        FURI_LOG_E(TAG, "Failed to load http method");
    }
    else
    {
        if (strstr(http_method, "BROWSE") != NULL)
        {
            // parse HTML then return response
            FuriString *returned_data = flipper_http_load_from_file(model->fhttp->file_path);
            if (returned_data == NULL || furi_string_size(returned_data) == 0)
            {
                return "Failed to load HTML response.\n\n\n\n\nPress BACK to return.";
            }

            // head is mandatory,
            bool head_exists = html_furi_tag_exists("<head>", returned_data, 0);
            if (!head_exists)
            {
                FURI_LOG_E(TAG, "Invalid HTML response");
                return "Invalid HTML response.\n\n\n\n\nPress BACK to return.";
            }

            // optional tags but we'll append them the response in order (title -> h1 -> h2 -> h3 -> p)
            FuriString *response = furi_string_alloc();
            if (html_furi_tag_exists("<title>", returned_data, 0))
            {
                FuriString *title = html_furi_find_tag("<title>", returned_data, 0);
                furi_string_trim(title);
                furi_string_cat_str(response, "Title: ");
                furi_string_cat(response, title);
                furi_string_cat_str(response, "\n\n");
                furi_string_free(title);
            }
            if (html_furi_tag_exists("<h1>", returned_data, 0))
            {
                FuriString *h1 = html_furi_find_tag("<h1>", returned_data, 0);
                furi_string_trim(h1);
                furi_string_cat(response, h1);
                furi_string_cat_str(response, "\n\n");
                furi_string_free(h1);
            }
            if (html_furi_tag_exists("<h2>", returned_data, 0))
            {
                FuriString *h2 = html_furi_find_tag("<h2>", returned_data, 0);
                furi_string_trim(h2);
                furi_string_cat(response, h2);
                furi_string_cat_str(response, "\n");
                furi_string_free(h2);
            }
            if (html_furi_tag_exists("<h3>", returned_data, 0))
            {
                FuriString *h3 = html_furi_find_tag("<h3>", returned_data, 0);
                furi_string_trim(h3);
                furi_string_cat(response, h3);
                furi_string_cat_str(response, "\n");
                furi_string_free(h3);
            }
            if (html_furi_tag_exists("<p>", returned_data, 0))
            {
                FuriString *p = html_furi_find_tags("<p>", returned_data);
                furi_string_trim(p);
                furi_string_cat(response, p);
                furi_string_free(p);
            }
            furi_string_free(returned_data);
            if (response && furi_string_size(response) > 0)
            {
                return (char *)furi_string_get_cstr(response);
            }
            return "No HTML tags found.\nTry another URL...\n\n\n\nPress BACK to return.";
        }
    }
    return "Data saved to file.\n\n\n\n\nPress BACK to return.";
}

static void web_crawler_data_switch_to_view(WebCrawlerApp *app)
{
    furi_check(app, "web_crawler_data_switch_to_view: WebCrawlerApp is NULL");

    // Allocate title on the heap.
    char *title = malloc(32);
    if (title == NULL)
    {
        FURI_LOG_E(TAG, "Failed to allocate memory for title");
        return; // or handle the error as needed
    }

    char http_method[16];
    if (!load_char("http_method", http_method, sizeof(http_method)))
    {
        FURI_LOG_E(TAG, "Failed to load http method");
        snprintf(title, 32, "Request");
    }
    else
    {
        if (strstr(http_method, "GET") != NULL)
        {
            snprintf(title, 32, "GET Request");
        }
        else if (strstr(http_method, "POST") != NULL)
        {
            snprintf(title, 32, "POST Request");
        }
        else if (strstr(http_method, "PUT") != NULL)
        {
            snprintf(title, 32, "PUT Request");
        }
        else if (strstr(http_method, "DELETE") != NULL)
        {
            snprintf(title, 32, "DELETE Request");
        }
        else if (strstr(http_method, "DOWNLOAD") != NULL)
        {
            snprintf(title, 32, "File Download");
        }
        else if (strstr(http_method, "BROWSE") != NULL)
        {
            snprintf(title, 32, "Browse URL");
        }
        else
        {
            // Provide a default title if no known http method is found.
            snprintf(title, 32, "Request");
        }
    }
    web_crawler_generic_switch_to_view(app, title, web_crawler_fetch, web_crawler_parse, 1, web_crawler_back_to_main_callback, WebCrawlerViewLoader);
}

/**
 * @brief      Navigation callback to handle exiting from other views to the submenu.
 * @param      context   The context - WebCrawlerApp object.
 * @return     WebCrawlerViewSubmenu
 */
uint32_t web_crawler_back_to_configure_callback(void *context)
{
    WebCrawlerApp *app = (WebCrawlerApp *)context;
    furi_check(app, "web_crawler_back_to_configure_callback: WebCrawlerApp is NULL");
    return WebCrawlerViewSubmenuConfig; // Return to the configure screen
}

/**
 * @brief      Navigation callback to handle returning to the Wifi Settings screen.
 * @param      context   The context - WebCrawlerApp object.
 * @return     WebCrawlerViewSubmenu
 */
uint32_t web_crawler_back_to_main_callback(void *context)
{
    WebCrawlerApp *app = (WebCrawlerApp *)context;
    furi_check(app, "web_crawler_back_to_main_callback: WebCrawlerApp is NULL");
    return WebCrawlerViewSubmenuMain; // Return to the main submenu
}

static uint32_t web_crawler_back_to_file_callback(void *context)
{
    UNUSED(context);
    return WebCrawlerViewVariableItemList; // Return to the file submenu
}

uint32_t web_crawler_back_to_wifi_callback(void *context)
{
    UNUSED(context);
    return WebCrawlerViewVariableItemList; // Return to the wifi submenu
}

uint32_t web_crawler_back_to_request_callback(void *context)
{
    UNUSED(context);
    return WebCrawlerViewVariableItemList; // Return to the request submenu
}

/**
 * @brief      Navigation callback to handle exiting the app from the main submenu.
 * @param      context   The context - unused
 * @return     VIEW_NONE to exit the app
 */
uint32_t web_crawler_exit_app_callback(void *context)
{
    UNUSED(context);
    return VIEW_NONE;
}

/**
 * @brief      Handle submenu item selection.
 * @param      context   The context - WebCrawlerApp object.
 * @param      index     The WebCrawlerSubmenuIndex item that was clicked.
 */
void web_crawler_submenu_callback(void *context, uint32_t index)
{
    WebCrawlerApp *app = (WebCrawlerApp *)context;
    furi_check(app, "WebCrawlerApp is NULL");
    if (app->view_dispatcher)
    {
        switch (index)
        {
        case WebCrawlerSubmenuIndexRun:
            web_crawler_data_switch_to_view(app);
            break;
        case WebCrawlerSubmenuIndexAbout:
            free_all(app);
            if (!alloc_widget(app, WebCrawlerViewAbout))
            {
                FURI_LOG_E(TAG, "Failed to allocate widget");
                return;
            }
            view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewWidget);
            break;
        case WebCrawlerSubmenuIndexConfig:
            free_all(app);
            if (!alloc_submenu_config(app))
            {
                FURI_LOG_E(TAG, "Failed to allocate submenu");
                return;
            }
            view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewSubmenuConfig);
            break;
        case WebCrawlerSubmenuIndexWifi:
            free_variable_item_list(app);
            if (!alloc_variable_item_list(app, WebCrawlerViewVariableItemListWifi))
            {
                FURI_LOG_E(TAG, "Failed to allocate variable item list");
                return;
            }
            view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewVariableItemList);
            break;
        case WebCrawlerSubmenuIndexRequest:
            free_variable_item_list(app);
            if (!alloc_variable_item_list(app, WebCrawlerViewVariableItemListRequest))
            {
                FURI_LOG_E(TAG, "Failed to allocate variable item list");
                return;
            }
            view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewVariableItemList);
            break;
        case WebCrawlerSubmenuIndexFile:
            free_variable_item_list(app);
            if (!alloc_variable_item_list(app, WebCrawlerViewVariableItemListFile))
            {
                FURI_LOG_E(TAG, "Failed to allocate variable item list");
                return;
            }
            view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewVariableItemList);
            break;
        default:
            FURI_LOG_E(TAG, "Unknown submenu index");
            break;
        }
    }
}

/**
 * @brief      Configuration enter callback to handle different items.
 * @param      context   The context - WebCrawlerApp object.
 * @param      index     The index of the item that was clicked.
 */
void web_crawler_wifi_enter_callback(void *context, uint32_t index)
{
    WebCrawlerApp *app = (WebCrawlerApp *)context;
    furi_check(app, "web_crawler_wifi_enter_callback: WebCrawlerApp is NULL");
    switch (index)
    {
    case 0: // SSID
        free_text_input(app);
        if (!alloc_text_input(app, WebCrawlerViewTextInputSSID))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewInput);
        break;
    case 1: // Password
        free_text_input(app);
        if (!alloc_text_input(app, WebCrawlerViewTextInputPassword))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewInput);
        break;
    default:
        FURI_LOG_E(TAG, "Unknown configuration item index");
        break;
    }
}

/**
 * @brief      Configuration enter callback to handle different items.
 * @param      context   The context - WebCrawlerApp object.
 * @param      index     The index of the item that was clicked.
 */
void web_crawler_file_enter_callback(void *context, uint32_t index)
{
    WebCrawlerApp *app = (WebCrawlerApp *)context;
    furi_check(app, "web_crawler_file_enter_callback: WebCrawlerApp is NULL");
    switch (index)
    {
    case 0: // File Read
        web_crawler_setting_item_file_read_clicked(context, index);
        break;
    case 1: // FIle Type
        free_text_input(app);
        if (!alloc_text_input(app, WebCrawlerViewTextInputFileType))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewInput);
        break;
    case 2: // File Rename
        free_text_input(app);
        if (!alloc_text_input(app, WebCrawlerViewTextInputFileRename))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewInput);
        break;
    case 3: // File Delete
        web_crawler_setting_item_file_delete_clicked(context, index);
        break;
    default:
        FURI_LOG_E(TAG, "Unknown configuration item index");
        break;
    }
}

/**
 * @brief      Configuration enter callback to handle different items.
 * @param      context   The context - WebCrawlerApp object.
 * @param      index     The index of the item that was clicked.
 */
void web_crawler_request_enter_callback(void *context, uint32_t index)
{
    WebCrawlerApp *app = (WebCrawlerApp *)context;
    furi_check(app, "web_crawler_request_enter_callback: WebCrawlerApp is NULL");
    switch (index)
    {
    case 0: // URL
        free_text_input(app);
        if (!alloc_text_input(app, WebCrawlerViewTextInput))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewInput);
        break;
    case 1:
        // HTTP Method
        break;
    case 2:
        // Headers
        free_text_input(app);
        if (!alloc_text_input(app, WebCrawlerViewTextInputHeaders))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewInput);
        break;
    case 3:
        // Payload
        free_text_input(app);
        if (!alloc_text_input(app, WebCrawlerViewTextInputPayload))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewInput);
        break;
    default:
        FURI_LOG_E(TAG, "Unknown configuration item index");
        break;
    }
}

/**
 * @brief      Callback for when the user finishes entering the URL.
 * @param      context   The context - WebCrawlerApp object.
 */
void web_crawler_set_path_updated(void *context)
{
    WebCrawlerApp *app = (WebCrawlerApp *)context;
    furi_check(app, "WebCrawlerApp is NULL");
    snprintf(app->path, app->temp_buffer_size_path, "%s", app->temp_buffer_path);
    if (app->path_item)
    {
        variable_item_set_current_value_text(app->path_item, app->path);
    }
    save_char("path", app->temp_buffer_path);
    save_simply();
    view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewVariableItemList);
}

/**
 * @brief      Callback for when the user finishes entering the headers
 * @param      context   The context - WebCrawlerApp object.
 */
void web_crawler_set_headers_updated(void *context)
{
    WebCrawlerApp *app = (WebCrawlerApp *)context;
    furi_check(app, "WebCrawlerApp is NULL");
    snprintf(app->headers, app->temp_buffer_size_headers, "%s", app->temp_buffer_headers);
    if (app->headers_item)
    {
        variable_item_set_current_value_text(app->headers_item, app->headers);
    }
    save_char("headers", app->temp_buffer_headers);
    save_simply();
    view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewVariableItemList);
}

/**
 * @brief      Callback for when the user finishes entering the payload.
 * @param      context   The context - WebCrawlerApp object.
 */
void web_crawler_set_payload_updated(void *context)
{
    WebCrawlerApp *app = (WebCrawlerApp *)context;
    furi_check(app, "WebCrawlerApp is NULL");
    snprintf(app->payload, app->temp_buffer_size_payload, "%s", app->temp_buffer_payload);
    if (app->payload_item)
    {
        variable_item_set_current_value_text(app->payload_item, app->payload);
    }
    save_char("payload", app->temp_buffer_payload);
    save_simply();
    view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewVariableItemList);
}

/**
 * @brief      Callback for when the user finishes entering the SSID.
 * @param      context   The context - WebCrawlerApp object.
 */
void web_crawler_set_ssid_updated(void *context)
{
    WebCrawlerApp *app = (WebCrawlerApp *)context;
    furi_check(app, "WebCrawlerApp is NULL");
    FlipperHTTP *fhttp = flipper_http_alloc();
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "Failed to allocate FlipperHTTP");
        easy_flipper_dialog("[ERROR]", "Failed to allocate FlipperHTTP\nand save wifi settings\nRestart your Flipper");
        return;
    }
    char password[64];
    snprintf(app->ssid, app->temp_buffer_size_ssid, "%s", app->temp_buffer_ssid);
    if (app->ssid_item)
    {
        variable_item_set_current_value_text(app->ssid_item, app->ssid);

        if (load_char("wifi-password", password, 64))
        {
            // send to UART
            if (!flipper_http_save_wifi(fhttp, app->ssid, password))
            {
                FURI_LOG_E(TAG, "Failed to save wifi settings via UART");
                FURI_LOG_E(TAG, "Make sure the Flipper is connected to the Wifi Dev Board");
            }
            // wait until save is done
            while (fhttp->state != IDLE)
            {
                furi_delay_ms(100);
            }
        }
    }
    save_char("wifi-ssid", app->temp_buffer_ssid);
    save_simply();
    flipper_http_free(fhttp);
    view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewVariableItemList);
}

/**
 * @brief      Callback for when the user finishes entering the Password.
 * @param      context   The context - WebCrawlerApp object.
 */
void web_crawler_set_password_update(void *context)
{
    WebCrawlerApp *app = (WebCrawlerApp *)context;
    furi_check(app, "WebCrawlerApp is NULL");
    FlipperHTTP *fhttp = flipper_http_alloc();
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "Failed to allocate FlipperHTTP");
        easy_flipper_dialog("[ERROR]", "Failed to allocate FlipperHTTP\nand save wifi settings");
        return;
    }
    char ssid[64];
    snprintf(app->password, app->temp_buffer_size_password, "%s", app->temp_buffer_password);
    if (app->password_item)
    {
        variable_item_set_current_value_text(app->password_item, app->password);

        // send to UART
        if (load_char("wifi-ssid", ssid, 64))
        {
            if (!flipper_http_save_wifi(fhttp, ssid, app->password))
            {
                FURI_LOG_E(TAG, "Failed to save wifi settings via UART");
                FURI_LOG_E(TAG, "Make sure the Flipper is connected to the Wifi Dev Board");
            }
            // wait until save is done
            while (fhttp->state != IDLE)
            {
                furi_delay_ms(100);
            }
        }
    }
    save_char("wifi-password", app->temp_buffer_password);
    save_simply();
    flipper_http_free(fhttp);
    view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewVariableItemList);
}

/**
 * @brief      Callback for when the user finishes entering the File Type.
 * @param      context   The context - WebCrawlerApp object.
 */
void web_crawler_set_file_type_update(void *context)
{
    WebCrawlerApp *app = (WebCrawlerApp *)context;
    furi_check(app, "WebCrawlerApp is NULL");
    char old_file_type[16];
    snprintf(old_file_type, sizeof(old_file_type), "%s", app->file_type);
    snprintf(app->file_type, app->temp_buffer_size_file_type, "%s", app->temp_buffer_file_type);
    if (app->file_type_item)
    {
        variable_item_set_current_value_text(app->file_type_item, app->file_type);
    }
    char file_rename[128];
    if (load_char("file_rename", file_rename, 128))
    {
        rename_received_data(file_rename, file_rename, app->file_type, old_file_type);
    }
    save_char("file_type", app->file_type);
    save_simply();
    view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewVariableItemList);
}

/**
 * @brief      Callback for when the user finishes entering the File Rename.
 * @param      context   The context - WebCrawlerApp object.
 */
void web_crawler_set_file_rename_update(void *context)
{
    WebCrawlerApp *app = (WebCrawlerApp *)context;
    furi_check(app, "WebCrawlerApp is NULL");
    char old_name[256];
    snprintf(old_name, sizeof(old_name), "%s", app->file_rename);
    snprintf(app->file_rename, app->temp_buffer_size_file_rename, "%s", app->temp_buffer_file_rename);
    if (app->file_rename_item)
    {
        variable_item_set_current_value_text(app->file_rename_item, app->file_rename);
    }
    rename_received_data(old_name, app->file_rename, app->file_type, app->file_type);
    save_char("file_rename", app->file_rename);
    save_simply();
    view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewVariableItemList);
}

/**
 * @brief      Handler for File Delete configuration item click.
 * @param      context  The context - WebCrawlerApp object.
 * @param      index    The index of the item that was clicked.
 */
void web_crawler_setting_item_file_delete_clicked(void *context, uint32_t index)
{
    WebCrawlerApp *app = (WebCrawlerApp *)context;
    furi_check(app, "WebCrawlerApp is NULL");
    UNUSED(index);

    free_widget(app);
    if (!alloc_widget(app, WebCrawlerViewFileDelete))
    {
        FURI_LOG_E(TAG, "web_crawler_setting_item_file_delete_clicked: Failed to allocate widget");
        return;
    }

    if (!delete_received_data())
    {
        FURI_LOG_E(TAG, "Failed to delete file");
    }

    // Show text input dialog
    view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewWidget);
}

void web_crawler_setting_item_file_read_clicked(void *context, uint32_t index)
{
    WebCrawlerApp *app = (WebCrawlerApp *)context;
    furi_check(app, "WebCrawlerApp is NULL");
    UNUSED(index);
    free_widget(app);
    if (!alloc_widget(app, WebCrawlerViewFileRead))
    {
        FURI_LOG_E(TAG, "web_crawler_setting_item_file_read_clicked: Failed to allocate widget");
        return;
    }
    widget_reset(app->widget);
    char file_path[256];
    char file_rename[128];
    char file_type[16];
    if (load_char("file_rename", file_rename, 128) && load_char("file_type", file_type, 16))
    {
        snprintf(file_path, sizeof(file_path), "%s%s%s", RECEIVED_DATA_PATH, file_rename, file_type);
    }
    else
    {
        snprintf(file_path, sizeof(file_path), "%s%s%s", RECEIVED_DATA_PATH, "received_data", ".txt");
    }

    // load the received data from the saved file
    FuriString *received_data = flipper_http_load_from_file(file_path);
    if (received_data == NULL)
    {
        FURI_LOG_E(TAG, "Failed to load received data from file.");
        if (app->widget)
        {
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64, "File is empty.");
            view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewWidget);
        }
        return;
    }
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(received_data));
    furi_string_free(received_data);

    // Show text input dialog
    view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewWidget);
}

static void web_crawler_widget_set_text(char *message, Widget **widget)
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

void web_crawler_loader_draw_callback(Canvas *canvas, void *model)
{
    if (!canvas || !model)
    {
        FURI_LOG_E(TAG, "web_crawler_loader_draw_callback - canvas or model is NULL");
        return;
    }

    DataLoaderModel *data_loader_model = (DataLoaderModel *)model;
    SerialState http_state = data_loader_model->fhttp->state;
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
        web_crawler_draw_error(canvas, data_loader_model);
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
        canvas_draw_str(canvas, 0, 27, "Fetching...");
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

static void web_crawler_loader_process_callback(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "web_crawler_loader_process_callback - context is NULL");
        DEV_CRASH();
        return;
    }

    WebCrawlerApp *app = (WebCrawlerApp *)context;
    View *view = app->view_loader;

    DataState current_data_state;
    DataLoaderModel *loader_model = NULL;
    with_view_model(view, DataLoaderModel * model, { current_data_state = model->data_state; loader_model = model; }, false);
    if (!loader_model || !loader_model->fhttp)
    {
        FURI_LOG_E(TAG, "Model or fhttp is NULL");
        DEV_CRASH();
        return;
    }

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
                strncpy(model->fhttp->last_response, "", 1);
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
        if (loader_model->fhttp->state == IDLE && loader_model->fhttp->last_response != NULL)
        {
            if (strstr(loader_model->fhttp->last_response, "[PONG]") != NULL)
            {
                FURI_LOG_DEV(TAG, "PONG received.");
            }
            else if (strncmp(loader_model->fhttp->last_response, "[SUCCESS]", 9) == 0)
            {
                FURI_LOG_DEV(TAG, "SUCCESS received. %s", loader_model->fhttp->last_response ? loader_model->fhttp->last_response : "NULL");
            }
            else if (strncmp(loader_model->fhttp->last_response, "[ERROR]", 9) == 0)
            {
                FURI_LOG_DEV(TAG, "ERROR received. %s", loader_model->fhttp->last_response ? loader_model->fhttp->last_response : "NULL");
            }
            else if (strlen(loader_model->fhttp->last_response) == 0)
            {
                // Still waiting on response
            }
            else
            {
                with_view_model(view, DataLoaderModel * model, { model->data_state = DataStateReceived; }, true);
            }
        }
        else if (loader_model->fhttp->state == SENDING || loader_model->fhttp->state == RECEIVING)
        {
            // continue waiting
        }
        else if (loader_model->fhttp->state == INACTIVE)
        {
            // inactive. try again
        }
        else if (loader_model->fhttp->state == ISSUE)
        {
            with_view_model(view, DataLoaderModel * model, { model->data_state = DataStateError; }, true);
        }
        else
        {
            FURI_LOG_DEV(TAG, "Unexpected state: %d lastresp: %s", loader_model->fhttp->state, loader_model->fhttp->last_response ? loader_model->fhttp->last_response : "NULL");
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
                FURI_LOG_DEV(TAG, "Parsed data: %s\r\ntext: %s", model->fhttp->last_response ? model->fhttp->last_response : "NULL", data_text ? data_text : "NULL");
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
                    web_crawler_widget_set_text(model->data_text != NULL ? model->data_text : "", &app->widget_result);
                    if (model->data_text != NULL)
                    {
                        free(model->data_text);
                        model->data_text = NULL;
                    }
                    view_set_previous_callback(widget_get_view(app->widget_result), model->back_callback);
                    view_dispatcher_switch_to_view(app->view_dispatcher, WebCrawlerViewWidgetResult);
                }
            },
            true);
    }
}

static void web_crawler_loader_timer_callback(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "web_crawler_loader_timer_callback - context is NULL");
        DEV_CRASH();
        return;
    }
    WebCrawlerApp *app = (WebCrawlerApp *)context;
    view_dispatcher_send_custom_event(app->view_dispatcher, WebCrawlerCustomEventProcess);
}

static void web_crawler_loader_on_enter(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "web_crawler_loader_on_enter - context is NULL");
        DEV_CRASH();
        return;
    }
    WebCrawlerApp *app = (WebCrawlerApp *)context;
    View *view = app->view_loader;
    with_view_model(
        view,
        DataLoaderModel * model,
        {
            view_set_previous_callback(view, model->back_callback);
            if (model->timer == NULL)
            {
                model->timer = furi_timer_alloc(web_crawler_loader_timer_callback, FuriTimerTypePeriodic, app);
            }
            furi_timer_start(model->timer, 250);
        },
        true);
}

static void web_crawler_loader_on_exit(void *context)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "web_crawler_loader_on_exit - context is NULL");
        DEV_CRASH();
        return;
    }
    WebCrawlerApp *app = (WebCrawlerApp *)context;
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

void web_crawler_loader_init(View *view)
{
    if (view == NULL)
    {
        FURI_LOG_E(TAG, "web_crawler_loader_init - view is NULL");
        DEV_CRASH();
        return;
    }
    view_allocate_model(view, ViewModelTypeLocking, sizeof(DataLoaderModel));
    view_set_enter_callback(view, web_crawler_loader_on_enter);
    view_set_exit_callback(view, web_crawler_loader_on_exit);
}

void web_crawler_loader_free_model(View *view)
{
    if (view == NULL)
    {
        FURI_LOG_E(TAG, "web_crawler_loader_free_model - view is NULL");
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
                // do not free the context here, it is the app context
                // free(model->parser_context);
                // model->parser_context = NULL;
            }
            if (model->fhttp)
            {
                flipper_http_free(model->fhttp);
                model->fhttp = NULL;
            }
        },
        false);
}

bool web_crawler_custom_event_callback(void *context, uint32_t index)
{
    if (context == NULL)
    {
        FURI_LOG_E(TAG, "web_crawler_custom_event_callback - context is NULL");
        DEV_CRASH();
        return false;
    }

    switch (index)
    {
    case WebCrawlerCustomEventProcess:
        web_crawler_loader_process_callback(context);
        return true;
    default:
        FURI_LOG_DEV(TAG, "web_crawler_custom_event_callback. Unknown index: %ld", index);
        return false;
    }
}

void web_crawler_generic_switch_to_view(WebCrawlerApp *app, char *title, DataLoaderFetch fetcher, DataLoaderParser parser, size_t request_count, ViewNavigationCallback back, uint32_t view_id)
{
    if (app == NULL)
    {
        FURI_LOG_E(TAG, "web_crawler_generic_switch_to_view - app is NULL");
        DEV_CRASH();
        return;
    }

    View *view = app->view_loader;
    if (view == NULL)
    {
        FURI_LOG_E(TAG, "web_crawler_generic_switch_to_view - view is NULL");
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
            //
            model->parser_context = app;
            if (!model->fhttp)
            {
                model->fhttp = flipper_http_alloc();
            }
        },
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, view_id);
}
