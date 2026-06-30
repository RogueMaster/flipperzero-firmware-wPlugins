#include "callback/alloc.h"
#include <callback/callback.h>
#include <flip_storage/flip_wifi_storage.h>

bool alloc_playlist(void *context)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);
    if (!wifi_playlist)
    {
        wifi_playlist = (WiFiPlaylist *)malloc(sizeof(WiFiPlaylist));
        if (!wifi_playlist)
        {
            FURI_LOG_E(TAG, "Failed to allocate playlist");
            return false;
        }
        wifi_playlist->count = 0;
    }
    // Load the playlist from storage
    if (!load_playlist(wifi_playlist))
    {
        FURI_LOG_E(TAG, "Failed to load playlist");

        // playlist is empty?
        submenu_reset(app->submenu_wifi);
        submenu_set_header(app->submenu_wifi, "Saved APs");
        submenu_add_item(app->submenu_wifi, "[Add Network]", FlipWiFiSubmenuIndexWiFiSavedAddSSID, callback_submenu_choices, app);
    }
    else
    {
        // Update the submenu
        callback_redraw_submenu_saved(app);
    }
    return true;
}

bool alloc_submenus(void *context, uint32_t view)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);
    switch (view)
    {
    case FlipWiFiViewSubmenuScan:
        if (!app->submenu_wifi)
        {
            if (!easy_flipper_set_submenu(&app->submenu_wifi, FlipWiFiViewSubmenu, "WiFi Nearby", callback_to_submenu_main, &app->view_dispatcher))
            {
                return false;
            }
            if (!app->submenu_wifi)
            {
                FURI_LOG_E(TAG, "Failed to allocate submenu for WiFi Scan");
                return false;
            }
        }
        return true;
    case FlipWiFiViewSubmenuSaved:
        if (!app->submenu_wifi)
        {
            if (!easy_flipper_set_submenu(&app->submenu_wifi, FlipWiFiViewSubmenu, "Saved APs", callback_to_submenu_main, &app->view_dispatcher))
            {
                return false;
            }
            if (!app->submenu_wifi)
            {
                FURI_LOG_E(TAG, "Failed to allocate submenu for WiFi Saved");
                return false;
            }
            if (!alloc_playlist(app))
            {
                FURI_LOG_E(TAG, "Failed to allocate playlist");
                return false;
            }
        }
        return true;
    case FlipWiFiViewSubmenuCommands:
        if (!app->submenu_wifi)
        {
            if (!easy_flipper_set_submenu(&app->submenu_wifi, FlipWiFiViewSubmenu, "Fast Commands", callback_to_submenu_main, &app->view_dispatcher))
            {
                return false;
            }
            if (!app->submenu_wifi)
            {
                FURI_LOG_E(TAG, "Failed to allocate submenu for Commands");
                return false;
            }
            //  PING, LIST, WIFI/LIST, IP/ADDRESS, and WIFI/IP.
            submenu_add_item(app->submenu_wifi, "[CUSTOM]", FlipWiFiSubmenuIndexFastCommandStart + 0, callback_submenu_choices, app);
            submenu_add_item(app->submenu_wifi, "PING", FlipWiFiSubmenuIndexFastCommandStart + 1, callback_submenu_choices, app);
            submenu_add_item(app->submenu_wifi, "LIST", FlipWiFiSubmenuIndexFastCommandStart + 2, callback_submenu_choices, app);
            submenu_add_item(app->submenu_wifi, "IP/ADDRESS", FlipWiFiSubmenuIndexFastCommandStart + 3, callback_submenu_choices, app);
            submenu_add_item(app->submenu_wifi, "WIFI/IP", FlipWiFiSubmenuIndexFastCommandStart + 4, callback_submenu_choices, app);
            submenu_add_item(app->submenu_wifi, "WIFI/AP", FlipWiFiSubmenuIndexFastCommandStart + 5, callback_submenu_choices, app);
        }
        return true;
    case FlipWiFiViewSubmenuAP:
        if (!app->submenu_wifi)
        {
            if (!easy_flipper_set_submenu(&app->submenu_wifi, FlipWiFiViewSubmenu, "AP Mode", callback_to_submenu_main, &app->view_dispatcher))
            {
                return false;
            }
            if (!app->submenu_wifi)
            {
                FURI_LOG_E(TAG, "Failed to allocate submenu for AP Mode");
                return false;
            }
            // start, set SSID, set HTML
            submenu_add_item(app->submenu_wifi, "Start AP", FlipWiFiSubmenuIndexWiFiAPStart, callback_submenu_choices, app);
            submenu_add_item(app->submenu_wifi, "Set SSID", FlipWiFiSubmenuIndexWiFiAPSetSSID, callback_submenu_choices, app);
            submenu_add_item(app->submenu_wifi, "Change HTML", FlipWiFiSubmenuIndexWiFiAPSetHTML, callback_submenu_choices, app);
        }
        return true;
    }
    return false;
}

bool alloc_text_box(FlipWiFiApp *app)
{
    furi_check(app, "FlipWiFiApp is NULL");
    if (app->textbox)
    {
        FURI_LOG_E(TAG, "Text box already allocated");
        return false;
    }
    if (!easy_flipper_set_text_box(
            &app->textbox,
            FlipWiFiViewWiFiAP,
            app->fhttp->last_response_str && furi_string_size(app->fhttp->last_response_str) > 0 ? (char *)furi_string_get_cstr(app->fhttp->last_response_str) : "AP Connected... please wait",
            false,
            callback_submenu_ap,
            &app->view_dispatcher))
    {
        FURI_LOG_E(TAG, "Failed to allocate text box");
        return false;
    }
    if (app->timer == NULL)
    {
        app->timer = furi_timer_alloc(callback_timer_callback, FuriTimerTypePeriodic, app);
    }
    furi_timer_start(app->timer, 250);
    return app->textbox != NULL;
}

bool alloc_text_inputs(void *context, uint32_t view)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);
    app->uart_text_input_buffer_size = MAX_SSID_LENGTH;
    if (!app->uart_text_input_buffer)
    {
        if (!easy_flipper_set_buffer(&app->uart_text_input_buffer, app->uart_text_input_buffer_size))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input buffer");
            return false;
        }
        if (!app->uart_text_input_buffer)
        {
            FURI_LOG_E(TAG, "Failed to allocate text input buffer");
            return false;
        }
    }
    if (!app->uart_text_input_temp_buffer)
    {
        if (!easy_flipper_set_buffer(&app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size))
        {
            FURI_LOG_E(TAG, "Failed to allocate text input temp buffer");
            return false;
        }
        if (!app->uart_text_input_temp_buffer)
        {
            FURI_LOG_E(TAG, "Failed to allocate text input temp buffer");
            return false;
        }
    }
    switch (view)
    {
    case FlipWiFiViewTextInputScan:
        if (!app->uart_text_input)
        {
            if (!easy_flipper_set_uart_text_input(&app->uart_text_input, FlipWiFiViewTextInput, "Enter WiFi Password", app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size, callback_text_updated_password_scan, callback_to_submenu_scan, &app->view_dispatcher, app))
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Scan");
                return false;
            }
            if (!app->uart_text_input)
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Scan");
                return false;
            }
        }
        return true;
    case FlipWiFiViewTextInputSaved:
        if (!app->uart_text_input)
        {
            if (!easy_flipper_set_uart_text_input(&app->uart_text_input, FlipWiFiViewTextInput, "Enter WiFi Password", app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size, callback_text_updated_password_saved, callback_to_submenu_saved, &app->view_dispatcher, app))
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Saved");
                return false;
            }
            if (!app->uart_text_input)
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Saved");
                return false;
            }
        }
        return true;
    case FlipWiFiViewTextInputSavedAddSSID:
        if (!app->uart_text_input)
        {
            if (!easy_flipper_set_uart_text_input(&app->uart_text_input, FlipWiFiViewTextInput, "Enter SSID", app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size, callback_text_updated_add_ssid, callback_to_submenu_saved, &app->view_dispatcher, app))
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Saved Add SSID");
                return false;
            }
            if (!app->uart_text_input)
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Saved Add SSID");
                return false;
            }
        }
        return true;
    case FlipWiFiViewTextInputSavedAddPassword:
        if (!app->uart_text_input)
        {
            if (!easy_flipper_set_uart_text_input(&app->uart_text_input, FlipWiFiViewTextInput, "Enter Password", app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size, callback_text_updated_add_password, callback_to_submenu_saved, &app->view_dispatcher, app))
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Saved Add Password");
                return false;
            }
            if (!app->uart_text_input)
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Saved Add Password");
                return false;
            }
        }
        return true;
    case FlipWiFiSubmenuIndexFastCommandStart:
        if (!app->uart_text_input)
        {
            if (!easy_flipper_set_uart_text_input(&app->uart_text_input, FlipWiFiViewTextInput, "Enter Command", app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size, callback_custom_command_updated, callback_to_submenu_saved, &app->view_dispatcher, app))
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for Fast Command");
                return false;
            }
            if (!app->uart_text_input)
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for Fast Command");
                return false;
            }
        }
        return true;
    case FlipWiFiSubmenuIndexWiFiAPSetSSID:
        if (!app->uart_text_input)
        {
            if (!easy_flipper_set_uart_text_input(&app->uart_text_input, FlipWiFiViewTextInput, "Enter AP SSID", app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size, callback_ap_ssid_updated, callback_to_submenu_main, &app->view_dispatcher, app))
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for Fast Command");
                return false;
            }
            if (!app->uart_text_input)
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for Fast Command");
                return false;
            }
        }
        return true;
    case FlipWiFiViewTextInputDeauth:
        if (!app->uart_text_input)
        {
            if (!easy_flipper_set_uart_text_input(&app->uart_text_input, FlipWiFiViewTextInput, "Enter SSID", app->uart_text_input_temp_buffer, app->uart_text_input_buffer_size, callback_text_updated_deauth, callback_to_submenu_main, &app->view_dispatcher, app))
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Deauth");
                return false;
            }
            if (!app->uart_text_input)
            {
                FURI_LOG_E(TAG, "Failed to allocate text input for WiFi Deauth");
                return false;
            }
            char deauth_ssid[64];
            if (load_char("deauth_ssid", deauth_ssid, sizeof(deauth_ssid)))
            {
                strncpy(app->uart_text_input_temp_buffer, deauth_ssid, app->uart_text_input_buffer_size);
                app->uart_text_input_temp_buffer[app->uart_text_input_buffer_size - 1] = '\0';
            }
        }
        return true;
    }
    return false;
}

bool alloc_views(void *context, uint32_t view)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);
    switch (view)
    {
    case FlipWiFiViewWiFiScan:
        if (!app->view_wifi)
        {
            if (!easy_flipper_set_view(&app->view_wifi, FlipWiFiViewGeneric, callback_view_draw_callback_scan, callback_view_input_callback_scan, callback_to_submenu_scan, &app->view_dispatcher, app))
            {
                return false;
            }
            if (!app->view_wifi)
            {
                FURI_LOG_E(TAG, "Failed to allocate view for WiFi Scan");
                return false;
            }
        }
        return true;
    case FlipWiFiViewWiFiSaved:
        if (!app->view_wifi)
        {
            if (!easy_flipper_set_view(&app->view_wifi, FlipWiFiViewGeneric, callback_view_draw_callback_saved, callback_view_input_callback_saved, callback_to_submenu_saved, &app->view_dispatcher, app))
            {
                return false;
            }
            if (!app->view_wifi)
            {
                FURI_LOG_E(TAG, "Failed to allocate view for WiFi Scan");
                return false;
            }
        }
        return true;
    case FlipWiFiViewWiFiDeauth:
        if (!app->view_wifi)
        {
            if (!easy_flipper_set_view(&app->view_wifi, FlipWiFiViewGeneric, callback_view_draw_callback_deauth, callback_view_input_callback_deauth, callback_to_submenu_main, &app->view_dispatcher, app))
            {
                return false;
            }
            if (!app->view_wifi)
            {
                FURI_LOG_E(TAG, "Failed to allocate view for WiFi Deauth");
                return false;
            }
        }
        return true;
    default:
        return false;
    }
}

bool alloc_widgets(void *context, uint32_t widget)
{
    FlipWiFiApp *app = (FlipWiFiApp *)context;
    furi_check(app);
    switch (widget)
    {
    case FlipWiFiViewAbout:
        if (!app->widget_info)
        {
            char about_text[128];
            snprintf(about_text, sizeof(about_text), "FlipWiFi v%s\n-----\nFlipperHTTP companion app.\nScan and save WiFi networks.\n-----\nwww.github.com/jblanked", VERSION);
            if (!easy_flipper_set_widget(&app->widget_info, FlipWiFiViewAbout, about_text, callback_to_submenu_main, &app->view_dispatcher))
            {
                return false;
            }
            if (!app->widget_info)
            {
                FURI_LOG_E(TAG, "Failed to allocate widget for About");
                return false;
            }
        }
        return true;
    default:
        return false;
    }
}