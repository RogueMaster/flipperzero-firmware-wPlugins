#include <flip_trader.h>
void flip_trader_loader_free_model(View *view);

void asset_names_free(char **names)
{
    if (names)
    {
        for (int i = 0; i < ASSET_COUNT; i++)
        {
            free(names[i]);
        }
        free(names);
    }
}

char **asset_names_alloc()
{
    // Allocate memory for an array of 42 string pointers
    char **names = malloc(ASSET_COUNT * sizeof(char *));
    if (!names)
    {
        FURI_LOG_E(TAG, "Failed to allocate memory for asset names");
        return NULL;
    }

    // Assign each asset name to the array
    names[0] = strdup("ETHUSD");
    names[1] = strdup("BTCUSD");
    names[2] = strdup("AAPL");
    names[3] = strdup("AMZN");
    names[4] = strdup("GOOGL");
    names[5] = strdup("MSFT");
    names[6] = strdup("TSLA");
    names[7] = strdup("NFLX");
    names[8] = strdup("META");
    names[9] = strdup("NVDA");
    names[10] = strdup("AMD");
    names[11] = strdup("INTC");
    names[12] = strdup("PLTR");
    names[13] = strdup("EURUSD");
    names[14] = strdup("GBPUSD");
    names[15] = strdup("AUDUSD");
    names[16] = strdup("NZDUSD");
    names[17] = strdup("XAUUSD");
    names[18] = strdup("USDJPY");
    names[19] = strdup("USDCHF");
    names[20] = strdup("USDCAD");
    names[21] = strdup("EURJPY");
    names[22] = strdup("EURGBP");
    names[23] = strdup("EURCHF");
    names[24] = strdup("EURCAD");
    names[25] = strdup("EURAUD");
    names[26] = strdup("EURNZD");
    names[27] = strdup("AUDJPY");
    names[28] = strdup("AUDCHF");
    names[29] = strdup("AUDCAD");
    names[30] = strdup("NZDJPY");
    names[31] = strdup("NZDCHF");
    names[32] = strdup("NZDCAD");
    names[33] = strdup("GBPJPY");
    names[34] = strdup("GBPCHF");
    names[35] = strdup("GBPCAD");
    names[36] = strdup("CHFJPY");
    names[37] = strdup("CADJPY");
    names[38] = strdup("CADCHF");
    names[39] = strdup("GBPAUD");
    names[40] = strdup("GBPNZD");
    names[41] = strdup("AUDNZD");

    return names;
}

char **asset_names = NULL;
// index
uint32_t asset_index = 0;
FlipTraderApp *app_instance = NULL;
// Function to free the resources used by FlipTraderApp
void flip_trader_app_free(FlipTraderApp *app)
{
    if (!app)
    {
        FURI_LOG_E(TAG, "FlipTraderApp is NULL");
        return;
    }

    // Free View(s)
    if (app->view_loader)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipTraderViewLoader);
        flip_trader_loader_free_model(app->view_loader);
        view_free(app->view_loader);
    }

    // Free Submenu(s)
    if (app->submenu_main)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipTraderViewMainSubmenu);
        submenu_free(app->submenu_main);
    }
    if (app->submenu_assets)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipTraderViewAssetsSubmenu);
        submenu_free(app->submenu_assets);
    }

    // Free Widget(s)
    if (app->widget_about)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipTraderViewAbout);
        widget_free(app->widget_about);
    }
    if (app->widget_result)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipTraderViewWidgetResult);
        widget_free(app->widget_result);
    }

    // Free Variable Item List(s)
    if (app->variable_item_list_wifi)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipTraderViewWiFiSettings);
        variable_item_list_free(app->variable_item_list_wifi);
    }

    // Free Text Input(s)
    if (app->uart_text_input_ssid)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipTraderViewTextInputSSID);
        text_input_free(app->uart_text_input_ssid);
    }
    if (app->uart_text_input_password)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipTraderViewTextInputPassword);
        text_input_free(app->uart_text_input_password);
    }

    asset_names_free(asset_names);

    // deinitalize flipper http
    flipper_http_deinit();

    // free the view dispatcher
    view_dispatcher_free(app->view_dispatcher);

    // close the gui
    furi_record_close(RECORD_GUI);

    // free the app
    free(app);
}