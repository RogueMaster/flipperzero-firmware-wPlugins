#include "alloc/flip_trader_alloc.h"

// Function to allocate resources for the FlipTraderApp
FlipTraderApp *flip_trader_app_alloc()
{
    FlipTraderApp *app = (FlipTraderApp *)malloc(sizeof(FlipTraderApp));

    Gui *gui = furi_record_open(RECORD_GUI);

    // initialize uart
    if (!flipper_http_init(flipper_http_rx_callback, app))
    {
        FURI_LOG_E(TAG, "Failed to initialize flipper http");
        return NULL;
    }

    // Allocate the text input buffer
    app->uart_text_input_buffer_size_ssid = 64;
    app->uart_text_input_buffer_size_password = 64;
    if (!easy_flipper_set_buffer(&app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_size_ssid))
    {
        return NULL;
    }
    if (!easy_flipper_set_buffer(&app->uart_text_input_temp_buffer_ssid, app->uart_text_input_buffer_size_ssid))
    {
        return NULL;
    }
    if (!easy_flipper_set_buffer(&app->uart_text_input_buffer_password, app->uart_text_input_buffer_size_password))
    {
        return NULL;
    }
    if (!easy_flipper_set_buffer(&app->uart_text_input_temp_buffer_password, app->uart_text_input_buffer_size_password))
    {
        return NULL;
    }

    asset_names = asset_names_alloc();
    if (!asset_names)
    {
        return NULL;
    }

    // Allocate ViewDispatcher
    if (!easy_flipper_set_view_dispatcher(&app->view_dispatcher, gui, app))
    {
        return NULL;
    }
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, flip_trader_custom_event_callback);
    // Main view
    if (!easy_flipper_set_view(&app->view_loader, FlipTraderViewLoader, flip_trader_loader_draw_callback, NULL, callback_to_assets_submenu, &app->view_dispatcher, app))
    {
        return NULL;
    }
    flip_trader_loader_init(app->view_loader);

    // Widget
    if (!easy_flipper_set_widget(&app->widget_about, FlipTraderViewAbout, "FlipTrader v1.2\n-----\nUse WiFi to get the price of\nstocks and currency pairs.\n-----\nwww.github.com/jblanked", callback_to_submenu, &app->view_dispatcher))
    {
        return NULL;
    }
    if (!easy_flipper_set_widget(&app->widget_result, FlipTraderViewWidgetResult, "Error, try again.", callback_to_assets_submenu, &app->view_dispatcher))
    {
        return NULL;
    }

    // Text Input
    if (!easy_flipper_set_uart_text_input(&app->uart_text_input_ssid, FlipTraderViewTextInputSSID, "Enter SSID", app->uart_text_input_temp_buffer_ssid, app->uart_text_input_buffer_size_ssid, text_updated_ssid, callback_to_wifi_settings, &app->view_dispatcher, app))
    {
        return NULL;
    }
    if (!easy_flipper_set_uart_text_input(&app->uart_text_input_password, FlipTraderViewTextInputPassword, "Enter password", app->uart_text_input_temp_buffer_password, app->uart_text_input_buffer_size_password, text_updated_password, callback_to_wifi_settings, &app->view_dispatcher, app))
    {
        return NULL;
    }

    // Variable Item List
    if (!easy_flipper_set_variable_item_list(&app->variable_item_list_wifi, FlipTraderViewWiFiSettings, settings_item_selected, callback_to_submenu, &app->view_dispatcher, app))
    {
        return NULL;
    }
    app->variable_item_ssid = variable_item_list_add(app->variable_item_list_wifi, "SSID", 0, NULL, NULL);
    app->variable_item_password = variable_item_list_add(app->variable_item_list_wifi, "Password", 0, NULL, NULL);
    variable_item_set_current_value_text(app->variable_item_ssid, "");
    variable_item_set_current_value_text(app->variable_item_password, "");

    // Submenu
    if (!easy_flipper_set_submenu(&app->submenu_main, FlipTraderViewMainSubmenu, "FlipTrader v1.2", easy_flipper_callback_exit_app, &app->view_dispatcher))
    {
        return NULL;
    }
    if (!easy_flipper_set_submenu(&app->submenu_assets, FlipTraderViewAssetsSubmenu, "Assets", callback_to_submenu, &app->view_dispatcher))
    {
        return NULL;
    }
    submenu_add_item(app->submenu_main, "Assets", FlipTradeSubmenuIndexAssets, callback_submenu_choices, app);
    submenu_add_item(app->submenu_main, "About", FlipTraderSubmenuIndexAbout, callback_submenu_choices, app);
    submenu_add_item(app->submenu_main, "WiFi", FlipTraderSubmenuIndexSettings, callback_submenu_choices, app);
    // add the assets
    for (uint32_t i = 0; i < ASSET_COUNT; i++)
    {
        submenu_add_item(app->submenu_assets, asset_names[i], FlipTraderSubmenuIndexAssetStartIndex + i, callback_submenu_choices, app);
    }

    // load settings
    if (load_settings(app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_size_ssid, app->uart_text_input_buffer_password, app->uart_text_input_buffer_size_password))
    {
        // Update variable items
        if (app->variable_item_ssid)
            variable_item_set_current_value_text(app->variable_item_ssid, app->uart_text_input_buffer_ssid);
        // dont show password

        // Copy items into their temp buffers with safety checks
        if (app->uart_text_input_buffer_ssid && app->uart_text_input_temp_buffer_ssid)
        {
            strncpy(app->uart_text_input_temp_buffer_ssid, app->uart_text_input_buffer_ssid, app->uart_text_input_buffer_size_ssid - 1);
            app->uart_text_input_temp_buffer_ssid[app->uart_text_input_buffer_size_ssid - 1] = '\0';
        }
        if (app->uart_text_input_buffer_password && app->uart_text_input_temp_buffer_password)
        {
            strncpy(app->uart_text_input_temp_buffer_password, app->uart_text_input_buffer_password, app->uart_text_input_buffer_size_password - 1);
            app->uart_text_input_temp_buffer_password[app->uart_text_input_buffer_size_password - 1] = '\0';
        }
    }

    // start with the main view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipTraderViewMainSubmenu);

    return app;
}