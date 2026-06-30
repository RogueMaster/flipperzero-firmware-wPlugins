#include <callback/callback.h>
#include <callback/loader.h>
#include <callback/free.h>
#include <callback/alloc.h>

uint32_t callback_exit_app(void *context)
{
    UNUSED(context);
    return VIEW_NONE;
}

uint32_t callback_submenu_ap(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);
    back_from_ap = true;
    return FlipWiFiViewSubmenu;
}

uint32_t callback_to_submenu_main(void *context)
{
    UNUSED(context);
    ssid_index = 0;
    return FlipWiFiViewSubmenuMain;
}
uint32_t callback_to_submenu_scan(void *context)
{
    UNUSED(context);
    ssid_index = 0;
    return FlipWiFiViewSubmenu;
}
uint32_t callback_to_submenu_saved(void *context)
{
    UNUSED(context);
    ssid_index = 0;
    return FlipWiFiViewSubmenu;
}

void callback_custom_command_updated(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);
    if (!app->fhttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is NULL");
        return;
    }
    if (!app->uart_text_input_temp_buffer)
    {
        FURI_LOG_E(TAG, "Text input buffer is NULL");
        return;
    }
    if (!app->uart_text_input_temp_buffer[0])
    {
        FURI_LOG_E(TAG, "Text input buffer is empty");
        return;
    }
    // Send the custom command
    flipper_http_send_data(app->fhttp, app->uart_text_input_temp_buffer);
    uint32_t timeout = 50; // 5 seconds / 100ms iterations
    while ((app->fhttp->last_response == NULL || strlen(app->fhttp->last_response) == 0) && timeout > 0)
    {
        furi_delay_ms(100);
        timeout--;
    }
    // Switch to the view
    char response[100];
    snprintf(response, sizeof(response), "%s", app->fhttp->last_response);
    easy_flipper_dialog("", response);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenu);
}
void callback_ap_ssid_updated(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);
    if (!app->uart_text_input_temp_buffer)
    {
        FURI_LOG_E(TAG, "Text input buffer is NULL");
        return;
    }
    if (!app->uart_text_input_temp_buffer[0])
    {
        FURI_LOG_E(TAG, "Text input buffer is empty");
        return;
    }
    save_char("ap_ssid", app->uart_text_input_temp_buffer);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenu);
}
void callback_text_updated_deauth(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);
    if (!app->uart_text_input_temp_buffer)
    {
        FURI_LOG_E(TAG, "Text input buffer is NULL");
        return;
    }
    if (!app->uart_text_input_temp_buffer[0])
    {
        FURI_LOG_E(TAG, "Text input buffer is empty");
        return;
    }
    save_char("deauth_ssid", app->uart_text_input_temp_buffer);
    if (!app->fhttp)
    {
        app->fhttp = flipper_http_alloc();
        if (!app->fhttp)
        {
            easy_flipper_dialog("[ERROR]", "Failed to initialize flipper http");
            return;
        }
    }
    Loading *loading = loading_alloc();
    int32_t loading_view_id = 87654321; // Random ID
    view_dispatcher_add_view(app->view_dispatcher, loading_view_id, loading_get_view(loading));
    view_dispatcher_switch_to_view(app->view_dispatcher, loading_view_id);
    if (!flipper_http_deauth_start(app->fhttp, app->uart_text_input_temp_buffer))
    {
        easy_flipper_dialog("[ERROR]", "Failed to start deauth attack");
        flipper_http_free(app->fhttp);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenuMain);
        view_dispatcher_remove_view(app->view_dispatcher, loading_view_id);
        loading_free(loading);
        loading = NULL;
        return;
    }
    while (app->fhttp->last_response == NULL || strlen(app->fhttp->last_response) == 0)
    {
        furi_delay_ms(100);
    }
    if (strstr(app->fhttp->last_response, "[DEAUTH/STARTING]") == NULL)
    {
        char response[256];
        snprintf(response, sizeof(response), "Failed to start deauth attack:\n%s", app->fhttp->last_response);
        easy_flipper_dialog("[ERROR]", response);
        flipper_http_free(app->fhttp);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenuMain);
        view_dispatcher_remove_view(app->view_dispatcher, loading_view_id);
        loading_free(loading);
        loading = NULL;
        return;
    }
    while (strstr(app->fhttp->last_response, "[DEAUTH/STARTING]") != NULL)
    {
        furi_delay_ms(100);
    }
    if (strstr(app->fhttp->last_response, "[ERROR]") != NULL)
    {
        char response[256];
        snprintf(response, sizeof(response), "Failed to start deauth attack:\n%s", app->fhttp->last_response);
        easy_flipper_dialog("[ERROR]", response);
        flipper_http_free(app->fhttp);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenuMain);
        view_dispatcher_remove_view(app->view_dispatcher, loading_view_id);
        loading_free(loading);
        loading = NULL;
        return;
    }

    if (!alloc_views(app, FlipWiFiViewWiFiDeauth))
    {
        FURI_LOG_E(TAG, "Failed to allocate view for WiFi Deauth");
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenuMain);
        view_dispatcher_remove_view(app->view_dispatcher, loading_view_id);
        loading_free(loading);
        loading = NULL;
        return;
    }

    // switch to a canvas view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewGeneric);
    view_dispatcher_remove_view(app->view_dispatcher, loading_view_id);
    loading_free(loading);
    loading = NULL;
}

void callback_redraw_submenu_saved(void *context)
{
    // re draw the saved submenu
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);
    if (!app->submenu_wifi)
    {
        FURI_LOG_E(TAG, "Submenu is NULL");
        return;
    }
    if (!wifi_playlist)
    {
        FURI_LOG_E(TAG, "WiFi Playlist is NULL");
        return;
    }
    submenu_reset(app->submenu_wifi);
    submenu_set_header(app->submenu_wifi, "Saved APs");
    submenu_add_item(app->submenu_wifi, "[Add Network]", FlipWiFiSubmenuIndexWiFiSavedAddSSID, callback_submenu_choices, app);
    for (size_t i = 0; i < wifi_playlist->count; i++)
    {
        submenu_add_item(app->submenu_wifi, wifi_playlist->ssids[i], FlipWiFiSubmenuIndexWiFiSavedStart + i, callback_submenu_choices, app);
    }
}
// Callback for drawing the main screen
void callback_view_draw_callback_scan(Canvas *canvas, void *model)
{
    UNUSED(model);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, ssid_list[ssid_index]);
    canvas_draw_icon(canvas, 0, 53, &I_ButtonBACK_10x8);
    canvas_draw_str_aligned(canvas, 12, 54, AlignLeft, AlignTop, "Back");
    canvas_draw_icon(canvas, 96, 53, &I_ButtonRight_4x7);
    canvas_draw_str_aligned(canvas, 103, 54, AlignLeft, AlignTop, "Add");
}

void callback_view_draw_callback_saved(Canvas *canvas, void *model)
{
    UNUSED(model);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, current_ssid);
    canvas_set_font(canvas, FontSecondary);
    char password[72];
    snprintf(password, sizeof(password), "Pass: %s", current_password);
    canvas_draw_str(canvas, 0, 20, password);
    canvas_draw_icon(canvas, 0, 54, &I_ButtonLeft_4x7);
    canvas_draw_str_aligned(canvas, 7, 54, AlignLeft, AlignTop, "Delete");
    canvas_draw_icon(canvas, 37, 53, &I_ButtonBACK_10x8);
    canvas_draw_str_aligned(canvas, 49, 54, AlignLeft, AlignTop, "Back");
    canvas_draw_icon(canvas, 73, 54, &I_ButtonOK_7x7);
    canvas_draw_str_aligned(canvas, 81, 54, AlignLeft, AlignTop, "Set");
    canvas_draw_icon(canvas, 100, 54, &I_ButtonRight_4x7);
    canvas_draw_str_aligned(canvas, 107, 54, AlignLeft, AlignTop, "Edit");
}

// Callback for drawing the deauth screen
void callback_view_draw_callback_deauth(Canvas *canvas, void *model)
{
    UNUSED(model);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "Deauthing...");
    canvas_draw_icon(canvas, 0, 53, &I_ButtonBACK_10x8);
    canvas_draw_str_aligned(canvas, 12, 54, AlignLeft, AlignTop, "Hit Back to stop");
}

// Input callback for the view (async input handling)
bool callback_view_input_callback_scan(InputEvent *event, void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (event->type == InputTypePress && event->key == InputKeyRight)
    {
        // switch to text input to set password
        free_text_inputs(app);
        if (!alloc_text_inputs(app, FlipWiFiViewTextInputScan))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Saved Add Password");
            return false;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewTextInput);
        return true;
    }
    return false;
}

// Input callback for the view (async input handling)
bool callback_view_input_callback_deauth(InputEvent *event, void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    if (event->type == InputTypePress && event->key == InputKeyBack)
    {
        if (app->fhttp)
        {
            flipper_http_deauth_stop(app->fhttp);
            furi_delay_ms(200);
            flipper_http_free(app->fhttp);
            app->fhttp = NULL;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenuMain);
        return true;
    }
    return false;
}

// Input callback for the view (async input handling)
bool callback_view_input_callback_saved(InputEvent *event, void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);
    if (event->type == InputTypePress && event->key == InputKeyRight)
    {
        // set text input buffer as the selected password
        strncpy(app->uart_text_input_temp_buffer, wifi_playlist->passwords[ssid_index], app->uart_text_input_buffer_size);
        // switch to text input to set password
        free_text_inputs(app);
        if (!alloc_text_inputs(app, FlipWiFiViewTextInputSaved))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Saved");
            return false;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewTextInput);
        return true;
    }
    else if (event->type == InputTypePress && event->key == InputKeyOk)
    {
        // save the settings
        save_settings(wifi_playlist->ssids[ssid_index], wifi_playlist->passwords[ssid_index]);

        // initialize uart
        FlipperHTTP *fhttp = flipper_http_alloc();
        if (!fhttp)
        {
            easy_flipper_dialog("[ERROR]", "Failed to initialize flipper http");
            return false;
        }

        // clear response
        if (fhttp->last_response)
            snprintf(fhttp->last_response, RX_BUF_SIZE, "%s", "");

        if (!flipper_http_save_wifi(fhttp, wifi_playlist->ssids[ssid_index], wifi_playlist->passwords[ssid_index]))
        {
            easy_flipper_dialog("[ERROR]", "Failed to save WiFi settings");
            return false;
        }

        while (!fhttp->last_response || strlen(fhttp->last_response) == 0)
        {
            furi_delay_ms(100);
        }

        // check success (if [SUCCESS] is in the response)
        if (strstr(fhttp->last_response, "[SUCCESS]") == NULL)
        {
            char response[256];
            snprintf(response, sizeof(response), "Failed to save WiFi settings:\n%s", fhttp->last_response);
            easy_flipper_dialog("[ERROR]", response);
            flipper_http_free(fhttp);
            return false;
        }

        flipper_http_free(fhttp);

        easy_flipper_dialog("[SUCCESS]", "All FlipperHTTP apps will now\nuse the selected network.");
        return true;
    }
    else if (event->type == InputTypePress && event->key == InputKeyLeft)
    {
        // shift the remaining ssids and passwords
        for (uint32_t i = ssid_index; i < wifi_playlist->count - 1; i++)
        {
            // Use strncpy to prevent buffer overflows and ensure null termination
            strncpy(wifi_playlist->ssids[i], wifi_playlist->ssids[i + 1], MAX_SSID_LENGTH - 1);
            wifi_playlist->ssids[i][MAX_SSID_LENGTH - 1] = '\0'; // Ensure null-termination

            strncpy(wifi_playlist->passwords[i], wifi_playlist->passwords[i + 1], MAX_SSID_LENGTH - 1);
            wifi_playlist->passwords[i][MAX_SSID_LENGTH - 1] = '\0'; // Ensure null-termination

            // Shift ssid_list
            ssid_list[i] = ssid_list[i + 1];
        }
        wifi_playlist->count--;

        // delete the last ssid and password
        wifi_playlist->ssids[wifi_playlist->count][0] = '\0';
        wifi_playlist->passwords[wifi_playlist->count][0] = '\0';

        // save the playlist to storage
        save_playlist(wifi_playlist);

        // re draw the saved submenu
        callback_redraw_submenu_saved(app);
        // switch back to the saved view
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenu);
        return true;
    }
    return false;
}
static bool callback_set_html()
{
    DialogsApp *dialogs = furi_record_open(RECORD_DIALOGS);
    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, ".js", NULL);

    browser_options.extension = "html";
    browser_options.base_path = STORAGE_APP_DATA_PATH_PREFIX;
    browser_options.skip_assets = true;
    browser_options.hide_dot_files = true;
    browser_options.icon = NULL;
    browser_options.hide_ext = false;

    FuriString *marauder_html_path = furi_string_alloc_set_str("/ext/apps_data/marauder/html");
    if (!marauder_html_path)
    {
        furi_record_close(RECORD_DIALOGS);
        return false;
    }

    if (dialog_file_browser_show(dialogs, marauder_html_path, marauder_html_path, &browser_options))
    {
        // Store the selected script file path
        const char *file_path = furi_string_get_cstr(marauder_html_path);
        save_char("ap_html_path", file_path);
    }

    furi_string_free(marauder_html_path);
    furi_record_close(RECORD_DIALOGS);
    return true;
}
static bool callback_run_ap_mode(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);
    if (!app->fhttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is NULL");
        return false;
    }
    char ssid[64];
    if (!load_char("ap_ssid", ssid, sizeof(ssid)))
    {
        FURI_LOG_E(TAG, "Failed to load AP SSID");
        return false;
    }
    // clear response
    if (app->fhttp->last_response)
        snprintf(app->fhttp->last_response, RX_BUF_SIZE, "%s", "");
    char stat_command[128];
    snprintf(stat_command, sizeof(stat_command), "[WIFI/AP]{\"ssid\":\"%s\"}", ssid);
    if (!flipper_http_send_data(app->fhttp, stat_command))
    {
        FURI_LOG_E(TAG, "Failed to start AP mode");
        return false;
    }
    Loading *loading = loading_alloc();
    int32_t loading_view_id = 87654321; // Random ID
    view_dispatcher_add_view(app->view_dispatcher, loading_view_id, loading_get_view(loading));
    view_dispatcher_switch_to_view(app->view_dispatcher, loading_view_id);
    while (app->fhttp->last_response == NULL || strlen(app->fhttp->last_response) == 0)
    {
        furi_delay_ms(100);
    }
    // check success (if [AP/CONNECTED] is in the response)
    if (strstr(app->fhttp->last_response, "[AP/CONNECTED]") != NULL)
    {
        // send the HTML file
        char html_path[128];
        if (!load_char("ap_html_path", html_path, sizeof(html_path)))
        {
            FURI_LOG_E(TAG, "Failed to load HTML path");
            return false;
        }
        flipper_http_send_data(app->fhttp, "[WIFI/AP/UPDATE]");
        furi_delay_ms(1000);
        FuriString *html_content = flipper_http_load_from_file(html_path);
        if (html_content == NULL || furi_string_size(html_content) == 0)
        {
            FURI_LOG_E(TAG, "Failed to load HTML file");
            if (html_content)
                furi_string_free(html_content);
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenu);
            view_dispatcher_remove_view(app->view_dispatcher, loading_view_id);
            loading_free(loading);
            loading = NULL;
            return false;
        }
        furi_string_cat_str(html_content, "\n");
        const char *send_buffer = furi_string_get_cstr(html_content);
        const size_t send_buffer_size = furi_string_size(html_content);

        app->fhttp->state = SENDING;
        size_t offset = 0;
        while (offset < send_buffer_size)
        {
            size_t chunk_size = send_buffer_size - offset > 512 ? 512 : send_buffer_size - offset;
            furi_hal_serial_tx(app->fhttp->serial_handle, (const uint8_t *)(send_buffer + offset), chunk_size);
            offset += chunk_size;
            furi_delay_ms(50); // cant go faster than this, no matter the chunk size
        }
        // send the [WIFI/AP/UPDATE/END] command
        flipper_http_send_data(app->fhttp, "[WIFI/AP/UPDATE/END]");
        app->fhttp->state = IDLE;
        furi_string_free(html_content);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenu);
        view_dispatcher_remove_view(app->view_dispatcher, loading_view_id);
        loading_free(loading);
        loading = NULL;
        return true;
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, loading_view_id);
    loading_free(loading);
    loading = NULL;
    return false;
}
void callback_timer_callback(void *context)
{
    furi_check(context, "callback_timer_callback: Context is NULL");
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FlipWiFiCustomEventAP);
}

static size_t last_response_len = 0;
static void update_text_box(FlipWiFiApp *app)
{
    furi_check(app, "FlipWiFiApp is NULL");
    furi_check(app->textbox, "Text_box is NULL");
    if (app->fhttp)
    {
        if (!app->fhttp->last_response_str || furi_string_size(app->fhttp->last_response_str) == 0)
        {
            text_box_reset(app->textbox);
            text_box_set_focus(app->textbox, TextBoxFocusEnd);
            text_box_set_font(app->textbox, TextBoxFontText);
            text_box_set_text(app->textbox, "Connected... please wait");
        }
        else if (furi_string_size(app->fhttp->last_response_str) != last_response_len)
        {
            text_box_reset(app->textbox);
            text_box_set_focus(app->textbox, TextBoxFocusEnd);
            text_box_set_font(app->textbox, TextBoxFontText);
            last_response_len = furi_string_size(app->fhttp->last_response_str);
            text_box_set_text(app->textbox, furi_string_get_cstr(app->fhttp->last_response_str));
        }
    }
}

static void callback_loader_process_callback(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app, "FlipWiFiApp is NULL");
    update_text_box(app);
}

static bool callback_custom_event_callback(void *context, uint32_t index)
{
    furi_check(context, "callback_custom_event_callback: Context is NULL");
    switch (index)
    {
    case FlipWiFiCustomEventAP:
        callback_loader_process_callback(context);
        return true;
    default:
        FURI_LOG_E(TAG, "callback_custom_event_callback. Unknown index: %ld", index);
        return false;
    }
}
// scan for wifi ad parse the results
static bool callback_scan(FlipperHTTP *fhttp)
{
    if (!fhttp)
    {
        FURI_LOG_E(TAG, "FlipperHTTP is NULL");
        return false;
    }

    // storage setup
    Storage *storage = furi_record_open(RECORD_STORAGE);

    snprintf(fhttp->file_path, sizeof(fhttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_wifi/data/scan.txt");
    storage_simply_remove_recursive(storage, fhttp->file_path); // ensure the file is empty

    // ensure flip_wifi directory is there
    char directory_path[128];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_wifi");
    storage_common_mkdir(storage, directory_path);

    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_wifi/data");
    storage_common_mkdir(storage, directory_path);

    furi_record_close(RECORD_STORAGE);

    fhttp->just_started = true;
    fhttp->save_received_data = true;
    FURI_LOG_I(TAG, "callback_scan: Sending scan command");
    return flipper_http_send_command(fhttp, HTTP_CMD_SCAN);
}

static bool callback_handle_scan(FlipperHTTP *fhttp, void *context)
{
    FURI_LOG_I(TAG, "callback_handle_scan1");
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);
    FURI_LOG_I(TAG, "callback_handle_scan2");
    if (!fhttp || !context)
    {
        FURI_LOG_E(TAG, "FlipperHTTP or context is NULL");
        return false;
    }

    // load the received data from the saved file
    FuriString *scan_data = flipper_http_load_from_file(fhttp->file_path);
    if (scan_data == NULL)
    {
        FURI_LOG_E(TAG, "Failed to load received data from file.");
        return false;
    }

    uint8_t ssid_count = 0;

    for (uint8_t i = 0; i < MAX_SCAN_NETWORKS; i++)
    {
        char *ssid_item = get_json_array_value("networks", i, furi_string_get_cstr(scan_data));
        if (ssid_item == NULL)
        {
            // end of the list
            break;
        }
        ssid_list[i] = malloc(MAX_SSID_LENGTH);
        if (ssid_list[i] == NULL)
        {
            FURI_LOG_E(TAG, "Failed to allocate memory for SSID");
            furi_string_free(scan_data);
            return false;
        }
        snprintf(ssid_list[i], MAX_SSID_LENGTH, "%s", ssid_item);
        free(ssid_item);
        ssid_count++;
    }

    // Add each SSID as a submenu item
    submenu_reset(app->submenu_wifi);
    submenu_set_header(app->submenu_wifi, "WiFi Nearby");
    for (uint8_t i = 0; i < ssid_count; i++)
    {
        char *ssid_item = ssid_list[i];
        if (ssid_item == NULL)
        {
            // end of the list
            break;
        }
        char ssid[64];
        snprintf(ssid, sizeof(ssid), "%s", ssid_item);
        submenu_add_item(app->submenu_wifi, ssid, FlipWiFiSubmenuIndexWiFiScanStart + i, callback_submenu_choices, app);
    }
    furi_string_free(scan_data);
    FURI_LOG_I(TAG, "Scan completed. Found %d networks.", ssid_count);
    return true;
}

void callback_submenu_choices(void *context, uint32_t index)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);
    switch (index)
    {
    case FlipWiFiSubmenuIndexWiFiScan:
        free_all(app);
        if (!alloc_submenus(app, FlipWiFiViewSubmenuScan))
        {
            easy_flipper_dialog("[ERROR]", "Failed to allocate submenus for WiFi Scan");
            return;
        }
        app->fhttp = flipper_http_alloc();
        if (!app->fhttp)
        {
            FURI_LOG_E(TAG, "Failed to allocate FlipperHTTP");
            easy_flipper_dialog("Error", "Failed to start UART.\nUART is likely busy or device\nis not connected.");
            return;
        }
        if (callback_scan(app->fhttp))
        {
            furi_delay_ms(1000); // wait for the command to be sent
            // wait for the scan to complete
            Loading *loading = loading_alloc();
            int32_t loading_view_id = 87654321; // Random ID
            view_dispatcher_add_view(app->view_dispatcher, loading_view_id, loading_get_view(loading));
            view_dispatcher_switch_to_view(app->view_dispatcher, loading_view_id);
            while (app->fhttp->state != IDLE)
            {
                furi_delay_ms(100);
            }
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenu);
            if (!callback_handle_scan(app->fhttp, app))
            {
                FURI_LOG_E(TAG, "Failed to handle scan");
                easy_flipper_dialog("[ERROR]", "Failed to handle scan");
                return;
            }
            view_dispatcher_remove_view(app->view_dispatcher, loading_view_id);
            loading_free(loading);
            loading = NULL;
            flipper_http_free(app->fhttp);
        }
        else
        {
            flipper_http_free(app->fhttp);
            easy_flipper_dialog("[ERROR]", "Failed to scan for WiFi networks");
            return;
        }
        break;
    case FlipWiFiSubmenuIndexWiFiDeauth:
        free_all(app);
        if (!alloc_text_inputs(app, FlipWiFiViewTextInputDeauth))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Saved Add Password");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewTextInput);
        break;
    case FlipWiFiSubmenuIndexWiFiSaved:
        free_all(app);
        if (!alloc_submenus(app, FlipWiFiViewSubmenuSaved))
        {
            FURI_LOG_E(TAG, "Failed to allocate submenus for WiFi Saved");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenu);
        break;
    case FlipWiFiSubmenuIndexAbout:
        free_all(app);
        if (!alloc_widgets(app, FlipWiFiViewAbout))
        {
            FURI_LOG_E(TAG, "Failed to allocate widget for About");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewAbout);
        break;
    case FlipWiFiSubmenuIndexWiFiSavedAddSSID:
        free_text_inputs(app);
        if (!alloc_text_inputs(app, FlipWiFiViewTextInputSavedAddSSID))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Saved Add Password");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewTextInput);
        break;
    case FlipWiFiSubmenuIndexWiFiAP:
        free_all(app);
        if (!alloc_submenus(app, FlipWiFiViewSubmenuAP))
        {
            FURI_LOG_E(TAG, "Failed to allocate submenus for APs");
            return;
        }
        app->fhttp = flipper_http_alloc();
        if (!app->fhttp)
        {
            FURI_LOG_E(TAG, "Failed to allocate FlipperHTTP");
            easy_flipper_dialog("Error", "Failed to start UART.\nUART is likely busy or device\nis not connected.");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenu);
        break;
    case FlipWiFiSubmenuIndexWiFiAPStart:
        // start AP
        view_dispatcher_set_custom_event_callback(app->view_dispatcher, callback_custom_event_callback);
        // send to AP View to see the responses
        if (!callback_run_ap_mode(app))
        {
            easy_flipper_dialog("[ERROR]", "Failed to start AP mode");
            return;
        }
        free_text_box(app);
        if (!alloc_text_box(app))
        {
            FURI_LOG_E(TAG, "Failed to allocate text box");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewWiFiAP);
        break;
    case FlipWiFiSubmenuIndexWiFiAPSetSSID:
        // set SSID
        free_text_inputs(app);
        if (!alloc_text_inputs(app, FlipWiFiSubmenuIndexWiFiAPSetSSID))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input for Fast Command");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewTextInput);
        break;
    case FlipWiFiSubmenuIndexWiFiAPSetHTML:
        // set HTML
        callback_set_html();
        break;
    case FlipWiFiSubmenuIndexCommands:
        free_all(app);
        if (!alloc_submenus(app, FlipWiFiViewSubmenuCommands))
        {
            FURI_LOG_E(TAG, "Failed to allocate submenus for Commands");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenu);
        break;
    case FlipWiFiSubmenuIndexFastCommandStart ... FlipWiFiSubmenuIndexFastCommandStart + 4:
        // initialize uart
        if (!app->fhttp)
        {
            app->fhttp = flipper_http_alloc();
            if (!app->fhttp)
            {
                FURI_LOG_E(TAG, "Failed to allocate FlipperHTTP");
                easy_flipper_dialog("Error", "Failed to start UART.\nUART is likely busy or device\nis not connected.");
                return;
            }
        }
        // Handle fast commands
        switch (index)
        {
        case FlipWiFiSubmenuIndexFastCommandStart + 0:
            // CUSTOM - send to text input and return
            free_text_inputs(app);
            if (!alloc_text_inputs(app, FlipWiFiSubmenuIndexFastCommandStart))
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for Fast Command");
                return;
            }
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewTextInput);
            return;
        case FlipWiFiSubmenuIndexFastCommandStart + 1:
            // PING
            flipper_http_send_command(app->fhttp, HTTP_CMD_PING);
            break;
        case FlipWiFiSubmenuIndexFastCommandStart + 2:
            // LIST
            flipper_http_send_command(app->fhttp, HTTP_CMD_LIST_COMMANDS);
            break;
        case FlipWiFiSubmenuIndexFastCommandStart + 3:
            // IP/ADDRESS
            flipper_http_send_command(app->fhttp, HTTP_CMD_IP_ADDRESS);
            break;
        case FlipWiFiSubmenuIndexFastCommandStart + 4:
            // WIFI/IP
            flipper_http_send_command(app->fhttp, HTTP_CMD_IP_WIFI);

            break;
        default:
            break;
        }
        while (app->fhttp->last_response == NULL || strlen(app->fhttp->last_response) == 0)
        {
            // Wait for the response
            furi_delay_ms(100);
        }
        if (app->fhttp->last_response != NULL)
        {
            char response[100];
            snprintf(response, sizeof(response), "%s", app->fhttp->last_response);
            easy_flipper_dialog("", response);
        }
        flipper_http_free(app->fhttp);
        break;
    case 100 ... 199:
        ssid_index = index - FlipWiFiSubmenuIndexWiFiScanStart;
        free_views(app);
        if (!alloc_views(app, FlipWiFiViewWiFiScan))
        {
            FURI_LOG_E(TAG, "Failed to allocate views for WiFi Scan");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewGeneric);
        break;
    case 200 ... 299:
        ssid_index = index - FlipWiFiSubmenuIndexWiFiSavedStart;
        free_views(app);
        snprintf(current_ssid, sizeof(current_ssid), "%s", wifi_playlist->ssids[ssid_index]);
        snprintf(current_password, sizeof(current_password), "%s", wifi_playlist->passwords[ssid_index]);
        if (!alloc_views(app, FlipWiFiViewWiFiSaved))
        {
            FURI_LOG_E(TAG, "Failed to allocate views for WiFi Saved");
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewGeneric);
        break;
    default:
        break;
    }
}

void callback_text_updated_password_scan(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);

    // Store the entered text with buffer size limit
    strncpy(app->uart_text_input_buffer, app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size - 1);
    // Ensure null-termination
    app->uart_text_input_buffer[app->uart_text_input_buffer_size - 1] = '\0';

    if (!alloc_playlist(app))
    {
        FURI_LOG_E(TAG, "Failed to allocate playlist");
        return;
    }

    // Ensure ssid_index is valid
    if (ssid_index >= MAX_SCAN_NETWORKS)
    {
        FURI_LOG_E(TAG, "Invalid ssid_index: %ld", ssid_index);
        return;
    }

    // Check if there's space in the playlist
    if (wifi_playlist->count >= MAX_SAVED_NETWORKS)
    {
        FURI_LOG_E(TAG, "Playlist is full. Cannot add more entries.");
        return;
    }

    // Add the SSID and password to the playlist
    snprintf(wifi_playlist->ssids[wifi_playlist->count], MAX_SSID_LENGTH, "%s", ssid_list[ssid_index]);
    snprintf(wifi_playlist->passwords[wifi_playlist->count], MAX_SSID_LENGTH, "%s", app->uart_text_input_buffer);
    wifi_playlist->count++;

    // Save the updated playlist to storage
    save_playlist(wifi_playlist);

    // Redraw the submenu to reflect changes
    callback_redraw_submenu_saved(app);

    // Switch back to the scan view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenu);
}

void callback_text_updated_password_saved(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);

    // store the entered text
    strncpy(app->uart_text_input_buffer, app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size);

    // Ensure null-termination
    app->uart_text_input_buffer[app->uart_text_input_buffer_size - 1] = '\0';

    // update the password_saved in the playlist
    snprintf(wifi_playlist->passwords[ssid_index], MAX_SSID_LENGTH, app->uart_text_input_buffer);

    // save the playlist to storage
    save_playlist(wifi_playlist);

    // switch to back to the saved view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenu);
}

void callback_text_updated_add_ssid(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);

    // check if empty
    if (strlen(app->uart_text_input_temp_buffer) == 0)
    {
        easy_flipper_dialog("[ERROR]", "SSID cannot be empty");
        return;
    }

    // store the entered text
    strncpy(app->uart_text_input_buffer, app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size);

    // Ensure null-termination
    app->uart_text_input_buffer[app->uart_text_input_buffer_size - 1] = '\0';
    save_char("wifi-ssid", app->uart_text_input_buffer);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenuMain);
    text_input_reset(app->uart_text_input);
    text_input_set_header_text(app->uart_text_input, "Enter Password");
    app->uart_text_input_buffer_size = MAX_SSID_LENGTH;
    free(app->uart_text_input_buffer);
    free(app->uart_text_input_temp_buffer);
    easy_flipper_set_buffer(&app->uart_text_input_buffer, app->uart_text_input_buffer_size);
    easy_flipper_set_buffer(&app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size);
    text_input_set_result_callback(app->uart_text_input, callback_text_updated_add_password, app, app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size, false);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewTextInput);
}
void callback_text_updated_add_password(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);

    // check if empty
    if (strlen(app->uart_text_input_temp_buffer) == 0)
    {
        easy_flipper_dialog("[ERROR]", "Password cannot be empty");
        return;
    }

    // store the entered text
    strncpy(app->uart_text_input_buffer, app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size);
    // Ensure null-termination
    app->uart_text_input_buffer[app->uart_text_input_buffer_size - 1] = '\0';

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenuMain);

    save_char("wifi-password", app->uart_text_input_buffer);

    char wifi_ssid[64];
    if (!load_char("wifi-ssid", wifi_ssid, sizeof(wifi_ssid)))
    {
        FURI_LOG_E(TAG, "Failed to load wifi ssid");
        return;
    }

    // add the SSID and password_scan to the playlist
    snprintf(wifi_playlist->ssids[wifi_playlist->count], MAX_SSID_LENGTH, wifi_ssid);
    snprintf(wifi_playlist->passwords[wifi_playlist->count], MAX_SSID_LENGTH, app->uart_text_input_buffer);
    wifi_playlist->count++;

    // save the playlist to storage
    save_playlist(wifi_playlist);

    callback_redraw_submenu_saved(app);

    // switch to back to the saved view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipWiFiViewSubmenu);
}